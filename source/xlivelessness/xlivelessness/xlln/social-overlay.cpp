#include <winsock2.h>
#include "social-overlay.hpp"
#include "social.hpp"
#include "social-kick.hpp"
#include "social-controller.hpp"
#include "debug-log.hpp"
#include "../xlive/xrender.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/xnotify.hpp"
#include <d3d9.h>
#include <windowsx.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cmath>

namespace {
const int Width=800, Height=600, Rows=6;
const COLORREF Background=RGB(23,22,20), Panel=RGB(38,33,25);
const COLORREF Gold=RGB(231,193,101), White=RGB(247,239,216), Muted=RGB(179,168,142);
struct Input { UINT message; WPARAM key; LPARAM position; };
std::mutex inputGate;
std::vector<Input> inputs;
std::atomic<bool> available(false), opened(false);
std::atomic<uint32_t> requestedUser(0);
std::atomic<ULONGLONG> inviteUntil(0);
IDirect3DDevice9* device=nullptr; // Borrowed; released by xrender after Detach.
IDirect3DTexture9* texture=nullptr;
HWND titleWindow=nullptr;
WNDPROC previousProc=nullptr;
HDC canvas=nullptr;
HBITMAP bitmap=nullptr, oldBitmap=nullptr;
HFONT bodyFont=nullptr, headingFont=nullptr;
HFONT detailFont=nullptr;
std::vector<uint32_t> surface;
void* pixels=nullptr;
bool notified=false, cursorShown=false, dirty=true, paintedOpen=false;
int cursorAdjustments=0, selected=-1, page=0, focus=0;
float left=0, top=0, scale=1;
std::string ip, name;
std::wstring status;
ULONGLONG lastPaint=0;
uint32_t activeUser=0;
bool joinPending=false, pendingAccept=false;
uint32_t pendingUser=0;
std::string pendingAddress;
XUID confirmGuest=0;
uint64_t confirmToken=0;
std::string confirmName;
enum class PadTarget { Friends, Invite, Join, Accept, Decline, Remove, Kick, Block,
    PreviousPage, NextPage, Address, Name, SendRequest, Back };
PadTarget padTarget=PadTarget::Friends;
bool padMode=false, padConfirmYes=false, controllerPresent=false;

void ConfirmKick(bool yes) {
    if(yes) {
        uint32_t result=SocialKickRequest(confirmGuest,confirmToken);
        if(result==ERROR_SUCCESS) opened=false;
        else status=L"That guest is no longer available to kick. Select the current guest again.";
    }
    confirmGuest=0;confirmToken=0;confirmName.clear();dirty=true;
}

std::wstring Wide(const std::string& text) {
    if(text.empty()) return L"";
    int length=MultiByteToWideChar(CP_UTF8,0,text.data(),(int)text.size(),nullptr,0);
    std::wstring result(length,L' ');
    MultiByteToWideChar(CP_UTF8,0,text.data(),(int)text.size(),&result[0],length);
    return result;
}
void Blend(int x,int y,COLORREF color,int alpha) {
    if(x<0 || y<0 || x>=Width || y>=Height)return;
    auto& d=surface[y*Width+x];int inv=255-alpha;
    int a=alpha+((d>>24)*inv+127)/255;
    int r=(GetRValue(color)*alpha+((d>>16)&255)*inv+127)/255;
    int g=(GetGValue(color)*alpha+((d>>8)&255)*inv+127)/255;
    int b=(GetBValue(color)*alpha+(d&255)*inv+127)/255;
    d=(a<<24)|(r<<16)|(g<<8)|b;
}
void Fill(int x,int y,int w,int h,COLORREF color,int alpha=255) {
    for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i)Blend(i,j,color,alpha);
}
void Rule(int x,int y,int w) {
    for(int i=0;i<w;++i) {
        int a=90+int(150*(1-std::abs(2.0*i/w-1)));
        Blend(x+i,y,Gold,a);Blend(x+i,y+1,RGB(75,49,16),200);
    }
}
void Band(int x,int y,int w,int h,COLORREF color,int alpha) {
    for(int i=0;i<w;++i) {
        double fade=1-std::abs(2.0*i/w-1);
        Fill(x+i,y,1,h,color,int(alpha*(.35+.65*fade)));
    }
}
void Frame(int x,int y,int w,int h,int opacity=185) {
    // Rounded glass with a shallow raised centre, like the Sanctuary's frame.
    for(int j=0;j<h;++j)for(int i=0;i<w;++i) {
        double crown=h>80?4*(1-std::abs(2.0*i/w-1)):0;
        double radius=(std::min)(20.0,h*.5-5);
        double px=i-w*.5,py=j-h*.5;
        double qx=std::abs(px)-(w*.5-radius-4),qy=std::abs(py+crown)-(h*.5-radius-4);
        double outside=std::sqrt((std::max)(qx,0.0)*(std::max)(qx,0.0)+(std::max)(qy,0.0)*(std::max)(qy,0.0));
        double d=outside+(std::min)((std::max)(qx,qy),0.0)-radius;
        if(d>1)continue;
        if(d>-1)Blend(x+i,y+j,RGB(160,125,55),int(230*(1-d)/2));
        else if(d>-4)Blend(x+i,y+j,RGB(10,9,8),245);
        else if(d>-6)Blend(x+i,y+j,RGB(111,93,60),220);
        else {
            int shine=(int)(18*(1.0-j/(double)h));
            Blend(x+i,y+j,RGB(23+shine,22+shine,19+shine),opacity);
        }
    }
}
void Text(const std::wstring& text,int x,int y,int w,int h,COLORREF color=White,bool heading=false,bool center=false,bool detail=false) {
    // GDI only supplies antialiased glyph coverage. Composite it into a separate
    // premultiplied surface so text stays crisp over translucent glass.
    auto old=SelectObject(canvas,detail?detailFont:heading?headingFont:bodyFont);
    RECT r={x,y,x+w,y+h};FillRect(canvas,&r,(HBRUSH)GetStockObject(BLACK_BRUSH));
    SetTextColor(canvas,RGB(255,255,255)); SetBkMode(canvas,TRANSPARENT);
    DrawTextW(canvas,text.c_str(),(int)text.size(),&r,(center?DT_CENTER:DT_LEFT)|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
    GdiFlush();auto mask=(uint32_t*)pixels;
    for(int j=(std::max)(y,0);j<(std::min)(y+h,Height);++j)
        for(int i=(std::max)(x,0);i<(std::min)(x+w,Width);++i)
            if(mask[j*Width+i]&255)Blend(i,j,color,mask[j*Width+i]&255);
    SelectObject(canvas,old);
}
void Button(const wchar_t* label,int x,int y,int w,bool enabled=true) {
    if(enabled) {Band(x,y,w,36,RGB(108,77,28),70);Rule(x,y+34,w);}
    Text(label,x+6,y,w-12,34,enabled?White:Muted,false,true,wcslen(label)>10);
}
bool Inside(int x,int y,int bx,int by,int w,int h) { return x>=bx && x<bx+w && y>=by && y<by+h; }
void Result(uint32_t result,const wchar_t* success) {
    if(result==ERROR_SUCCESS) status=success;
    else if(result==ERROR_NOT_READY) status=L"Unavailable: check that both players are online and the host can be joined.";
    else if(result==ERROR_NOT_LOGGED_ON) status=L"Sign in to your local XLLN profile first.";
    else if(result==ERROR_ALREADY_EXISTS) status=L"That address is already in your friends list.";
    else if(result==ERROR_INVALID_PARAMETER) status=L"Enter an IPv4 address. A name of up to 15 characters is optional.";
    else if(result==ERROR_WRITE_FAULT) status=L"Could not save your friends list. Check folder write access.";
    else if(result==ERROR_NETWORK_UNREACHABLE) status=L"The invitation could not be sent. Check the peer connection.";
    else if(result==ERROR_ACCESS_DENIED) status=L"Accept the friend request first, or unblock this player.";
    else if(result==ERROR_TOO_MANY_NAMES) status=L"Your saved peer limit has been reached.";
    else status=L"Friend unavailable. Refresh the selection and try again.";
    dirty=true;
}
void Add() {
    uint32_t result=SocialAddPeer(ip,name);
    Result(result,L"Request queued. It will be delivered when both games are online.");
    if(result==ERROR_SUCCESS) {ip.clear();name.clear();focus=0;}
}
void Act(int action,const std::vector<SocialPeerView>& peers) {
    if(selected<0 || selected>=(int)peers.size()) {status=L"Select a friend first.";dirty=true;return;}
    const auto p=peers[selected];
    const bool friendRequest=p.relationship==SocialRelationship::Incoming;
    const bool accepted=p.relationship==SocialRelationship::Friend;
    if(action==2 && friendRequest) {Result(SocialRespondFriend(p.ip,true),L"Friend accepted. Sending confirmation to their PC.");return;}
    if(action==3 && friendRequest) {Result(SocialRespondFriend(p.ip,false),L"Friend request declined.");return;}
    if((action==0 && (!accepted || !p.online)) || (action==1 && (!accepted || !p.joinable)) ||
       ((action==2 || action==3) && (!accepted || !p.incoming))) return;
    if(action==0) {
        uint32_t result=SocialInvite(activeUser,1,&p.xuid);
        XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS|XLLN_LOG_LEVEL_INFO,
            "SocialOverlay: Invite peer=%s result=%u.",p.ip.c_str(),result);
        Result(result,L"Invitation sent. Waiting for your friend to accept.");
    }
    if(action==1 || action==2) {
        pendingAddress=p.ip;pendingUser=activeUser;pendingAccept=action==2;joinPending=true;
        opened=false;
        XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS|XLLN_LOG_LEVEL_INFO,
            "SocialOverlay: %s peer=%s queued until the title consumes UI close.",pendingAccept?"Accept":"Join",p.ip.c_str());
    }
    if(action==3) Result(SocialDeclinePeer(p.ip),L"Invitation declined.");
    if(action==4) {Result(SocialRemovePeer(p.ip),p.relationship==SocialRelationship::Outgoing?L"Friend request cancelled.":L"Entry removed.");selected=-1;}
    if(action==6) {const bool block=p.relationship!=SocialRelationship::Blocked;Result(SocialBlockPeer(p.ip,block),block?L"Player blocked. Their messages will be ignored.":L"Player unblocked. You can send a new request.");selected=-1;}
    if(action==5) {
        auto token=SocialKickToken(p.xuid);
        if(!token) {status=L"Only the host can kick the current co-op guest.";dirty=true;return;}
        confirmGuest=p.xuid;confirmToken=token;confirmName=p.name;padConfirmYes=false;dirty=true;
    }
}
struct PadControl {PadTarget target;int x,y,w,h;};
std::vector<PadControl> PadControls(const std::vector<SocialPeerView>& peers) {
    std::vector<PadControl> controls={{PadTarget::Friends,24,102,752,252}};
    const auto p=selected>=0 && selected<(int)peers.size()?&peers[selected]:nullptr;
    const bool accepted=p && p->relationship==SocialRelationship::Friend;
    const bool incoming=p && (p->relationship==SocialRelationship::Incoming || (accepted && p->incoming));
    auto add=[&](PadTarget target,int x,int y,int w,bool enabled=true) {
        if(enabled)controls.push_back({target,x,y,w,36});
    };
    add(PadTarget::Kick,24,360,160,p && SocialKickToken(p->xuid)!=0);
    add(PadTarget::Block,196,360,116,p!=nullptr);
    add(PadTarget::PreviousPage,628,360,68,page>0);
    add(PadTarget::NextPage,706,360,70,(page+1)*Rows<(int)peers.size());
    add(PadTarget::Invite,24,400,140,accepted && p->online);
    add(PadTarget::Join,176,400,140,accepted && p->joinable);
    add(PadTarget::Accept,328,400,140,incoming);
    add(PadTarget::Decline,480,400,140,incoming);
    add(PadTarget::Remove,632,400,140,p && p->relationship!=SocialRelationship::Blocked);
    add(PadTarget::Address,24,484,280);add(PadTarget::Name,320,484,240);
    add(PadTarget::SendRequest,588,484,188);add(PadTarget::Back,630,555,146);
    return controls;
}
void PadFocus(PadTarget target) {
    padTarget=target;focus=target==PadTarget::Address?1:target==PadTarget::Name?2:0;dirty=true;
}
void PadPrimary(const std::vector<SocialPeerView>& peers) {
    if(selected<0 || selected>=(int)peers.size()) {PadFocus(PadTarget::Address);return;}
    const auto& p=peers[selected];
    PadFocus(p.relationship==SocialRelationship::Incoming || p.incoming?PadTarget::Accept:
        p.joinable?PadTarget::Join:p.online?PadTarget::Invite:
        p.relationship==SocialRelationship::Blocked?PadTarget::Block:PadTarget::Remove);
}
void ConsumeController(WORD buttons,const std::vector<SocialPeerView>& peers) {
    if(!opened || !buttons)return;
    padMode=true;dirty=true;
    if(selected<0 && !peers.empty())selected=0;
    if(confirmGuest) {
        if(buttons & SocialPadB) {ConfirmKick(false);return;}
        if(buttons & SocialPadLeft)padConfirmYes=false;
        else if(buttons & SocialPadRight)padConfirmYes=true;
        if(buttons & SocialPadA)ConfirmKick(padConfirmYes);
        return;
    }
    if(buttons & SocialPadB) {
        if(padTarget==PadTarget::Friends)opened=false;
        else PadFocus(PadTarget::Friends);
        return;
    }
    auto controls=PadControls(peers);
    auto current=std::find_if(controls.begin(),controls.end(),[](const PadControl& c){return c.target==padTarget;});
    // Presence/requests may change while the menu is open; never activate a
    // newly disabled action or silently substitute a destructive action.
    if(current==controls.end()) {PadFocus(PadTarget::Friends);return;}
    if(buttons & SocialPadA) {
        switch(padTarget) {
        case PadTarget::Friends:PadPrimary(peers);break;
        case PadTarget::Invite:Act(0,peers);break;
        case PadTarget::Join:Act(1,peers);break;
        case PadTarget::Accept:Act(2,peers);break;
        case PadTarget::Decline:Act(3,peers);break;
        case PadTarget::Remove:Act(4,peers);PadFocus(PadTarget::Friends);break;
        case PadTarget::Kick:Act(5,peers);break;
        case PadTarget::Block:Act(6,peers);PadFocus(PadTarget::Friends);break;
        case PadTarget::PreviousPage:--page;selected=page*Rows;PadFocus(PadTarget::Friends);break;
        case PadTarget::NextPage:++page;selected=page*Rows;PadFocus(PadTarget::Friends);break;
        case PadTarget::SendRequest:Add();break;
        case PadTarget::Address:case PadTarget::Name:status=L"Use the keyboard to enter text. Your controller can select Send request.";break;
        case PadTarget::Back:opened=false;break;
        }
        return;
    }
    int dx=0,dy=0;
    if(buttons & SocialPadUp)dy=-1;
    else if(buttons & SocialPadDown)dy=1;
    else if(buttons & SocialPadLeft)dx=-1;
    else if(buttons & SocialPadRight)dx=1;
    if(!dx && !dy)return;
    if(padTarget==PadTarget::Friends) {
        if(dy<0) {selected=(std::max)(0,selected-1);page=selected/Rows;return;}
        if(dy>0 && selected+1<(int)peers.size()) {++selected;page=selected/Rows;return;}
        if(dx>0) {PadPrimary(peers);return;}
    }
    const int cx=current->x+current->w/2,cy=current->y+current->h/2;
    int best=INT_MAX;PadTarget next=padTarget;
    for(const auto& c:controls) {
        int x=c.x+c.w/2-cx,y=c.y+c.h/2-cy;
        if((dx && x*dx<=0) || (dy && y*dy<=0))continue;
        if((dx && abs(y)>abs(x)) || (dy && abs(x)>abs(y)))continue;
        int score=dx?abs(x)+2*abs(y):abs(y)+2*abs(x);
        if(score<best) {best=score;next=c.target;}
    }
    PadFocus(next);
}
void PadOutline(int x,int y,int w,int h) {
    Fill(x,y,w,2,Gold);Fill(x,y+h-2,w,2,Gold);
    Fill(x,y,2,h,Gold);Fill(x+w-2,y,2,h,Gold);
}
LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_KILLFOCUS || (message==WM_ACTIVATEAPP && !w))SocialControllerSuspend();
    if(message==WM_KEYDOWN && w==VK_F6 && available && GetTickCount64()<inviteUntil.load()) {
        if(!(l & (1L<<30))) {SocialKickCancel();opened=true;}
        return 0;
    }
    if(opened) {
        if(message==WM_SETCURSOR) {SetCursor(LoadCursor(nullptr,IDC_ARROW));return TRUE;}
        if(message==WM_KEYDOWN || message==WM_CHAR || message==WM_LBUTTONDOWN || message==WM_MOUSEWHEEL) {
            std::lock_guard<std::mutex> lock(inputGate);
            if(inputs.size()<256) inputs.push_back({message,w,l});
            return 0;
        }
        if(message==WM_INPUT) return DefWindowProcW(window,message,w,l); // Release raw-input resources without passing input to the title.
        if(message==WM_KEYUP || message==WM_MOUSEMOVE ||
           message==WM_LBUTTONUP || message==WM_LBUTTONDBLCLK || message==WM_RBUTTONDOWN ||
           message==WM_RBUTTONUP || message==WM_MBUTTONDOWN || message==WM_MBUTTONUP) return 0;
        // Alt+Tab, window activation, and the title's lifecycle messages still pass through.
    }
    return CallWindowProcW(previousProc,window,message,w,l);
}
void Consume(const std::vector<SocialPeerView>& peers) {
    std::vector<Input> pending;
    {std::lock_guard<std::mutex> lock(inputGate);pending.swap(inputs);}
    for(const auto& event:pending) {
        if(!opened) break;
        dirty=true;
        padMode=false;
        if(event.message==WM_KEYDOWN) {
            if(confirmGuest) {if(event.key==VK_ESCAPE)ConfirmKick(false);continue;}
            if(event.key==VK_ESCAPE) {opened=false;continue;}
            if(event.key==VK_TAB) focus=(focus+1)%3;
            if(focus==0 && !peers.empty()) {
                if(event.key==VK_DOWN) selected=(std::min)(selected+1,(int)peers.size()-1);
                if(event.key==VK_UP) selected=(std::max)(0,selected-1);
                if(selected>=0) page=selected/Rows;
            }
            if(event.key==VK_RETURN && focus) Add();
        } else if(event.message==WM_CHAR && focus && !confirmGuest) {
            auto& target=focus==1?ip:name;
            if(event.key==8 && !target.empty()) target.pop_back();
            // Peer labels use the protocol's existing 15-byte name limit.
            else if(event.key>=32 && event.key<=126 && target.size()<(focus==1?15u:15u)) {
                if(focus==2 || (event.key>='0' && event.key<='9') || event.key=='.') target+=(char)event.key;
            }
        } else if(event.message==WM_MOUSEWHEEL) {
            if(confirmGuest)continue;
            const int maxPage=peers.empty()?0:((int)peers.size()-1)/Rows;
            page=(std::max)(0,(std::min)(maxPage,page+(GET_WHEEL_DELTA_WPARAM(event.key)>0?-1:1)));
        } else if(event.message==WM_LBUTTONDOWN) {
            RECT client={}; GetClientRect(titleWindow,&client);
            D3DVIEWPORT9 viewport={}; device->GetViewport(&viewport);
            if(client.right<=0 || client.bottom<=0) continue;
            int x=(int)((GET_X_LPARAM(event.position)*(float)viewport.Width/client.right-left)/scale);
            int y=(int)((GET_Y_LPARAM(event.position)*(float)viewport.Height/client.bottom-top)/scale);
            if(confirmGuest) {
                if(Inside(x,y,230,330,160,36))ConfirmKick(false);
                else if(Inside(x,y,410,330,160,36))ConfirmKick(true);
                continue;
            }
            if(Inside(x,y,630,555,146,36)) opened=false;
            if(Inside(x,y,24,102,752,Rows*42)) {int row=page*Rows+(y-102)/42;if(row<(int)peers.size())selected=row;focus=0;}
            if(Inside(x,y,628,360,68,30)) page=(std::max)(0,page-1);
            if(Inside(x,y,706,360,70,30)) page=(std::min)(peers.empty()?0:((int)peers.size()-1)/Rows,page+1);
            for(int i=0;i<5;++i) if(Inside(x,y,24+i*152,400,140,36)) Act(i,peers);
            if(Inside(x,y,24,360,160,36)) Act(5,peers);
            if(Inside(x,y,196,360,116,36)) Act(6,peers);
            if(Inside(x,y,24,484,280,36)) focus=1;
            if(Inside(x,y,320,484,240,36)) focus=2;
            if(Inside(x,y,588,484,188,36)) Add();
        }
    }
}
bool MakeCanvas() {
    if(canvas) return true;
    canvas=CreateCompatibleDC(nullptr);
    BITMAPINFO info={}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=Width;
    info.bmiHeader.biHeight=-Height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    bitmap=CreateDIBSection(canvas,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!canvas || !bitmap || !pixels) return false;
    oldBitmap=(HBITMAP)SelectObject(canvas,bitmap);
    bodyFont=CreateFontW(-22,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Palatino Linotype");
    headingFont=CreateFontW(-29,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Palatino Linotype");
    detailFont=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Palatino Linotype");
    return bodyFont && headingFont && detailFont;
}
bool Paint(bool full,const std::vector<SocialPeerView>& peers) {
    if(!MakeCanvas()) return false;
    static std::vector<uint32_t> glass[2];
    auto& base=glass[full?1:0];
    if(base.empty()) {
        surface.assign(Width*Height,0);Frame(4,4,792,full?592:90);base=surface;
    } else surface=base;
    if(!full) {
        const bool request=std::any_of(peers.begin(),peers.end(),[](const SocialPeerView& p){return p.relationship==SocialRelationship::Incoming;});
        Text(request?L"Friend request received":L"Game invitation received",24,8,752,36,Gold,true,true);
        Text(L"Press F6 or open Friends in the Sanctuary to respond.",24,44,752,32,White,false,true);
        Rule(24,82,752);
    } else {
        Text(L"Friends and Invitations",24,18,752,38,Gold,true,true);
        Rule(24,60,752);
        Band(24,62,752,35,RGB(97,32,26),130);
        Text(L"Accept friend requests, then invite or join your friends.",36,62,728,35,White,false,true);
        Rule(24,97,752);
        for(int row=0;row<Rows;++row) {
            int index=page*Rows+row, y=102+row*42;
            if(index>=(int)peers.size()) continue;
            const auto& p=peers[index];
            if(index==selected) {
                Band(24,y,752,40,RGB(38,111,157),190);
                Rule(24,y,752);Rule(24,y+39,752);
            }
            Text(Wide(p.name),44,y,235,40);
            Text(Wide(p.ip),283,y,186,40,Muted,false,true,true);
            const wchar_t* label=p.incoming?L"Invitation received":p.outgoing?(p.delivered?L"Invite delivered":L"Sending invite"):p.joinable?L"Joinable":p.online?L"Online":L"Offline";
            if(p.relationship==SocialRelationship::Incoming) label=L"Friend request received";
            else if(p.relationship==SocialRelationship::Outgoing) label=p.requestDelivered?L"Request delivered":L"Request queued";
            else if(p.relationship==SocialRelationship::Declined) label=L"Request declined";
            else if(p.relationship==SocialRelationship::Blocked) label=L"Blocked";
            else if(p.confirmingFriendship) label=L"Confirming friendship";
            Text(label,489,y,265,40,(p.incoming || p.relationship==SocialRelationship::Incoming)?Gold:(p.online?RGB(184,211,161):Muted),false,true,true);
        }
        if(peers.empty()) {
            Text(L"Your friends will appear here",40,177,720,44,White,false,true);
            Text(L"Add a friend using their address below.",40,222,720,32,Muted,false,true,true);
        }
        Rule(24,352,752);
        Text(std::to_wstring(peers.size())+L" entries",316,360,128,30,Muted,false,true,true);
        Text(L"Page "+std::to_wstring(page+1),450,360,170,30,Muted,false,true,true);
        Button(L"<",628,360,68);Button(L">",706,360,70);
        const SocialPeerView* p=selected>=0 && selected<(int)peers.size()?&peers[selected]:nullptr;
        Button(L"Kick player",24,360,160,p && SocialKickToken(p->xuid)!=0);
        Button(p && p->relationship==SocialRelationship::Blocked?L"Unblock":L"Block",196,360,116,p!=nullptr);
        const bool request=p && p->relationship==SocialRelationship::Incoming;
        const bool friendPeer=p && p->relationship==SocialRelationship::Friend;
        Button(L"Invite",24,400,140,friendPeer && p->online);Button(L"Join",176,400,140,friendPeer && p->joinable);
        Button(request?L"Accept friend":L"Accept invite",328,400,140,request || (friendPeer && p->incoming));
        Button(L"Decline",480,400,140,request || (friendPeer && p->incoming));
        Button(p && p->relationship==SocialRelationship::Outgoing?L"Cancel request":L"Remove",632,400,140,p && p->relationship!=SocialRelationship::Blocked);
        Rule(24,443,752);
        Text(L"Friend's IPv4 address",24,450,280,30,Muted,false,false,true);
        Text(L"Name (optional)",320,450,240,30,Muted,false,false,true);
        Band(24,484,280,36,focus==1?RGB(42,102,140):RGB(13,12,11),focus==1?150:100);
        Rule(24,519,280);Text(Wide(ip)+(focus==1?L"|":L""),34,484,260,36);
        Band(320,484,240,36,focus==2?RGB(42,102,140):RGB(13,12,11),focus==2?150:100);
        Rule(320,519,240);Text(Wide(name)+(focus==2?L"|":L""),330,484,220,36);
        Button(L"Send request",588,484,188);
        Text(status.empty()?L"Enter their address once. They only need to accept your request.":status,24,523,752,28,status.empty()?Muted:Gold,false,true,true);
        Band(24,555,752,34,RGB(156,104,26),170);Rule(24,553,752);
        Text(padMode?L"D-pad / L stick  Move     A  Select":L"Up / Down  Select     Tab  Edit",40,555,530,34,White,false,false,true);
        Text(L"Back",633,555,82,34,White,false,true);
        // An outlined key prompt uses the same Esc action as the Sanctuary menus.
        Frame(715,555,61,35);Text(padMode?L"B":L"Esc",718,554,55,36,padMode?RGB(245,152,126):White,false,true,true);
        if(padMode && !confirmGuest)for(const auto& c:PadControls(peers))
            if(c.target==padTarget)PadOutline(c.x,c.y,c.w,c.h);
        if(confirmGuest) {
            Fill(18,18,764,565,RGB(8,7,6),135);
            Frame(96,196,608,218,255);
            Text(L"Kick "+Wide(confirmName)+L" from your game?",120,216,560,52,Gold,true,true);
            Rule(120,270,560);
            Text(L"They will leave this session and remain in your friends list.",120,278,560,36,White,false,true,true);
            Button(L"Cancel",230,330,160);Button(L"Kick player",410,330,160);
            if(padMode) {
                PadOutline(padConfirmYes?410:230,330,160,36);
                Text(L"Left / Right  Choose     A  Select     B  Cancel",120,370,560,28,White,false,true,true);
            }
        }
    }
    // Premultiplied RGBA: transparent corners and glass, opaque text and trim.
    memcpy(pixels,surface.data(),Width*Height*4);
    auto data=(uint32_t*)pixels;
    if(!texture && FAILED(device->CreateTexture(Width,Height,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr))) return false;
    D3DLOCKED_RECT locked={};if(FAILED(texture->LockRect(0,&locked,nullptr,0)))return false;
    for(int y=0;y<Height;++y)memcpy((char*)locked.pBits+y*locked.Pitch,data+y*Width,Width*4);
    texture->UnlockRect(0);return true;
}
void Draw(bool full,const D3DVIEWPORT9& viewport) {
    IDirect3DStateBlock9* state=nullptr;
    if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&state)))return;
    if(FAILED(state->Capture())) {state->Release();return;}
    const float width=Width*scale, height=(full?Height:96)*scale;
    const float v=full?1.0f:96.0f/Height;
    struct Vertex {float x,y,z,rhw;DWORD color;float u,v;};
    Vertex vertices[]={{left-.5f,top-.5f,0,1,0xffffffff,0,0},{left+width-.5f,top-.5f,0,1,0xffffffff,1,0},
        {left-.5f,top+height-.5f,0,1,0xffffffff,0,v},{left+width-.5f,top+height-.5f,0,1,0xffffffff,1,v}};
    device->SetViewport(&viewport);device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
    device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
    device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
    device->SetRenderState(D3DRS_FOGENABLE,FALSE);device->SetRenderState(D3DRS_LIGHTING,FALSE);
    device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE,FALSE);device->SetRenderState(D3DRS_CLIPPLANEENABLE,0);
    device->SetRenderState(D3DRS_COLORWRITEENABLE,15);device->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);
    device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
    device->SetTexture(0,texture);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
    device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
    device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
    device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);device->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
    device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
    device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
    device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);device->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);
    HRESULT result=device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex));
    // Fable may keep its OS cursor hidden. Draw a pointer in the game surface
    // without depending on ShowCursor's per-thread display counter.
    if(full && GetForegroundWindow()==titleWindow) {
        POINT point={};RECT client={};GetCursorPos(&point);ScreenToClient(titleWindow,&point);GetClientRect(titleWindow,&client);
        if(client.right>0 && client.bottom>0) {
            float x=point.x*(float)viewport.Width/client.right+viewport.X,y=point.y*(float)viewport.Height/client.bottom+viewport.Y;
            Vertex pointer[]={{x,y,0,1,0xffffd78a,0,0},{x+7*scale,y+19*scale,0,1,0xffffd78a,0,0},{x+12*scale,y+11*scale,0,1,0xffffd78a,0,0}};
            device->SetTexture(0,nullptr);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
            device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,pointer,sizeof(Vertex));
        }
    }
    state->Apply();state->Release();
    static bool reported=false;
    if(SUCCEEDED(result) && !reported) {reported=true;XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS|XLLN_LOG_LEVEL_INFO,"SocialOverlay: first D3D9 panel draw completed.");}
}
void SyncOpen() {
    const bool full=opened.load();
    if(full!=notified) {
        notified=full;
        SocialControllerCapture(full);
        XllnUpdateXliveSystemUIOpenState();
        if(full) {
            cursorAdjustments=0;do{++cursorAdjustments;}while(ShowCursor(TRUE)<0);cursorShown=true;
            activeUser=requestedUser;status.clear();dirty=true;
            PadFocus(PadTarget::Friends);padConfirmYes=false;
        } else if(cursorShown) {
            confirmGuest=0;confirmToken=0;confirmName.clear();
            while(cursorAdjustments-->0)ShowCursor(FALSE);cursorAdjustments=0;cursorShown=false;
            std::lock_guard<std::mutex> lock(inputGate);inputs.clear();
        }
    }
}
void DispatchPendingJoin() {
    if(!joinPending)return;
    // A reopened guide cancels the old action; never join behind a new panel.
    if(opened) {joinPending=false;return;}
    if(!XLiveNotifyIsSystemUIClosed())return;
    joinPending=false;
    uint32_t result=SocialJoinPeer(pendingUser,pendingAddress,pendingAccept);
    XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS|XLLN_LOG_LEVEL_INFO,
        "SocialOverlay: %s peer=%s dispatched after UI close; result=%u.",pendingAccept?"Accept":"Join",pendingAddress.c_str(),result);
    if(result!=ERROR_SUCCESS) {requestedUser=pendingUser;opened=true;SyncOpen();}
    Result(result,L"Connection requested.");
}
}
bool SocialOverlayIsOpen() {return opened.load();}
bool SocialOverlayShow(uint32_t user) {
    if(!available || user>=XLIVE_LOCAL_USER_COUNT)return false;
    SocialKickCancel();
    requestedUser=user;opened=true;SocialControllerCapture(true);return true;
}
bool SocialOverlayClose() {bool wasOpen=opened.exchange(false);SocialControllerCapture(false);return wasOpen;}
bool SocialOverlayNotifyInvite(uint32_t user) {
    if(!available)return false;
    requestedUser=user;inviteUntil=GetTickCount64()+60000;return true;
}
bool SocialOverlayNotifyFriend(uint32_t user) {return SocialOverlayNotifyInvite(user);}
void SocialOverlayAttach(IDirect3DDevice9* titleDevice,HWND window) {
    SocialOverlayDetach();if(!titleDevice || !window)return;
    // Managed resources must not be used with D3D9Ex. Fable's verified path is D3D9.
    IDirect3DDevice9Ex* extended=nullptr;
    if(SUCCEEDED(titleDevice->QueryInterface(__uuidof(IDirect3DDevice9Ex),(void**)&extended))) {extended->Release();return;}
    device=titleDevice;titleWindow=window;
    previousProc=(WNDPROC)GetWindowLongPtrW(window,GWLP_WNDPROC);
    SetLastError(0);
    auto prior=SetWindowLongPtrW(window,GWLP_WNDPROC,(LONG_PTR)WindowProc);
    if(!prior && GetLastError()) {device=nullptr;titleWindow=nullptr;previousProc=nullptr;return;}
    previousProc=(WNDPROC)prior;available=true;
    SocialControllerAttach();
    XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS|XLLN_LOG_LEVEL_INFO,"SocialOverlay: attached to D3D9 title window; Sanctuary friends UI will render in game.");
}
void SocialOverlayDetach() {
    SocialKickCancel();confirmGuest=0;confirmToken=0;confirmName.clear();
    available=false;opened=false;inviteUntil=0;joinPending=false;pendingAddress.clear();SyncOpen();
    SocialControllerDetach();padMode=false;controllerPresent=false;PadFocus(PadTarget::Friends);
    if(titleWindow && previousProc && (WNDPROC)GetWindowLongPtrW(titleWindow,GWLP_WNDPROC)==WindowProc)
        SetWindowLongPtrW(titleWindow,GWLP_WNDPROC,(LONG_PTR)previousProc);
    titleWindow=nullptr;previousProc=nullptr;device=nullptr;
    if(texture){texture->Release();texture=nullptr;}
    if(canvas && oldBitmap)SelectObject(canvas,oldBitmap);
    if(bitmap)DeleteObject(bitmap);if(canvas)DeleteDC(canvas);
    if(bodyFont)DeleteObject(bodyFont);if(headingFont)DeleteObject(headingFont);if(detailFont)DeleteObject(detailFont);detailFont=nullptr;surface.clear();
    canvas=nullptr;bitmap=nullptr;oldBitmap=nullptr;bodyFont=nullptr;headingFont=nullptr;pixels=nullptr;
    {std::lock_guard<std::mutex> lock(inputGate);inputs.clear();}
    dirty=true;lastPaint=0;paintedOpen=false;
}
void SocialOverlayReset(HWND window) {
    if(window!=titleWindow && device) {
        auto currentDevice=device;auto user=requestedUser.load();bool wasOpen=opened.load();
        SocialOverlayAttach(currentDevice,window);
        if(wasOpen)SocialOverlayShow(user);
    }
    dirty=true;
}
void SocialOverlayRender() {
    if(!available || !device)return;
    SyncOpen();
    DispatchPendingJoin();
    uint32_t kickResult=ERROR_SUCCESS;
    if(SocialKickTakeResult(kickResult) && kickResult!=ERROR_SUCCESS) {
        opened=true;SyncOpen();status=L"Kick cancelled: the guest changed or the game was not ready.";dirty=true;
    }
    bool full=opened.load();const auto now=GetTickCount64();
    if(!full && now>=inviteUntil.load())return;
    if(FAILED(device->TestCooperativeLevel())) {SocialControllerSuspend();return;}
    D3DVIEWPORT9 viewport={};if(FAILED(device->GetViewport(&viewport)) || !viewport.Width || !viewport.Height)return;
    scale=(std::min)(1.5f,(std::min)(viewport.Width/850.0f,viewport.Height/650.0f));
    left=viewport.X+(viewport.Width-Width*scale)/2;top=full?viewport.Y+(viewport.Height-Height*scale)/2:viewport.Y+20;
    auto peers=SocialPeers();
    if(!full && std::none_of(peers.begin(),peers.end(),[](const SocialPeerView& p){return p.incoming || p.relationship==SocialRelationship::Incoming;})) {inviteUntil=0;return;}
    if(selected>=(int)peers.size())selected=-1;
    page=(std::min)(page,peers.empty()?0:((int)peers.size()-1)/Rows);
    if(full) {
        const bool foreground=GetForegroundWindow()==titleWindow;
        auto controller=SocialControllerPoll(foreground,now);
        if(controller.connected && !controllerPresent) {padMode=true;dirty=true;}
        controllerPresent=controller.connected;
        if(controller.connected && selected<0 && !peers.empty()) {selected=0;page=0;dirty=true;}
        if(foreground) {Consume(peers);ConsumeController(controller.pressed,peers);}
        if(!controller.connected && padMode) {padMode=false;dirty=true;}
    }
    SyncOpen();if(full && !opened)return;
    if(dirty || full!=paintedOpen || now-lastPaint>=250) {
        if(!Paint(full,peers))return;
        dirty=false;paintedOpen=full;lastPaint=now;
    }
    if(texture)Draw(full,viewport);
}

