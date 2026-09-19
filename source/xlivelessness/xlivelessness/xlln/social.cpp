#define _CRT_RAND_S
#include <winsock2.h>
#include <ws2tcpip.h>
#include "social.hpp"
#include "social-overlay.hpp"
#include "debug-log.hpp"
#include "xlln.hpp"
#include "wnd-main.hpp"
#include "xlln-network.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/xsession.hpp"
#include "../xlive/xnotify.hpp"
#include <mutex>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace {
const UINT SocialMessage = WM_APP + 711;
const uint8_t PacketType = 0x70;
// Keep the deployed v1 presence/invite wire format and join path intact.
enum FriendMessage : uint16_t { FriendRequest=6, FriendAccept=7, FriendDecline=8, FriendCancel=9, FriendReceipt=10 };
const size_t MaxPeers=256, MaxIncomingRequests=32;
#pragma pack(push, 1)
struct Packet {
    uint8_t type; uint32_t magic; uint16_t version; uint16_t kind;
    XUID sender; XUID recipient; uint32_t title; uint32_t state;
    XNKID session; uint64_t invite; char name[XUSER_NAME_SIZE];
};
#pragma pack(pop)
struct Peer {
    std::string ip; XUID xuid = 0; std::string name;
    XONLINE_FRIEND info = {}; ULONGLONG seen = 0;
    uint64_t incoming = 0, outgoing = 0, lastIncoming = 0;
    XNKID incomingSession = {}, outgoingSession = {};
    ULONGLONG incomingUntil = 0, outgoingUntil = 0;
    bool acknowledged = false;
    SocialRelationship relationship = SocialRelationship::Friend;
    uint64_t request = 0;
    uint16_t control = 0;
    bool controlAcknowledged = false;
    ULONGLONG lastRequest = 0;
    std::vector<uint64_t> retiredRequests;
};
std::recursive_mutex gate;
std::vector<Peer> peers;
bool loaded = false;
bool ready = false;
std::wstring path;
HWND window = 0;
uint32_t localUser = 0;
ULONGLONG lastTick = 0;
uint64_t serial = 0;
void Load() {
    if (loaded) return;
    loaded = true;
    if (path.empty()) {
    wchar_t file[MAX_PATH]; GetModuleFileNameW(xlln_hModule, file, MAX_PATH);
    path = file; path = path.substr(0, path.find_last_of(L"\\/") + 1) + L"XLiveLessNess\\friends.ini";
    }
    int count = GetPrivateProfileIntW(L"Friends", L"Count", 0, path.c_str());
    for (int i = 0; i < count && i < (int)MaxPeers; ++i) {
        wchar_t section[32], ip[64], name[64], id[32]; swprintf_s(section, L"Friend%d", i);
        GetPrivateProfileStringW(section,L"IP",L"",ip,64,path.c_str());
        GetPrivateProfileStringW(section,L"Name",L"",name,64,path.c_str());
        GetPrivateProfileStringW(section,L"XUID",L"0",id,32,path.c_str());
        char addr[64], label[64]; WideCharToMultiByte(CP_UTF8,0,ip,-1,addr,64,0,0); WideCharToMultiByte(CP_UTF8,0,name,-1,label,64,0,0);
        IN_ADDR parsed; if (InetPtonA(AF_INET,addr,&parsed)!=1) continue;
        Peer p; p.ip=addr; p.name=label; p.xuid=_wcstoui64(id,0,16);
        // Files from v2.4 have no Relationship key: preserve them as friends.
        auto state=GetPrivateProfileIntW(section,L"Relationship",0,path.c_str());
        if(state>(uint32_t)SocialRelationship::Blocked) continue;
        p.relationship=(SocialRelationship)state;
        GetPrivateProfileStringW(section,L"Request",L"0",id,32,path.c_str());p.request=_wcstoui64(id,0,16);
        p.control=(uint16_t)GetPrivateProfileIntW(section,L"Control",0,path.c_str());
        if(p.control && (p.control<FriendRequest || p.control>FriendCancel)) p.control=0;
        p.controlAcknowledged=GetPrivateProfileIntW(section,L"ControlAcknowledged",0,path.c_str())!=0;
        wchar_t retired[512];GetPrivateProfileStringW(section,L"RetiredRequests",L"",retired,512,path.c_str());
        std::wistringstream history(retired);uint64_t retiredId=0;
        while(p.retiredRequests.size()<16 && (history>>std::hex>>retiredId))p.retiredRequests.push_back(retiredId);
        if(p.relationship!=SocialRelationship::Friend && !p.request && p.relationship!=SocialRelationship::Blocked && p.relationship!=SocialRelationship::Removed) continue;
        peers.push_back(p);
    }
}
bool Save() {
    // Commit the whole relationship transaction before acknowledging it on the wire.
    // A write failure must leave both the previous file and in-memory state intact.
    const std::wstring temporary=path+L".pending";
    if(!DeleteFileW(temporary.c_str()) && GetLastError()!=ERROR_FILE_NOT_FOUND) return false;
    wchar_t count[16]; swprintf_s(count,L"%u",(unsigned)peers.size());
    bool ok=WritePrivateProfileStringW(L"Friends",L"Count",count,temporary.c_str())!=0;
    for(size_t i=0;i<peers.size();++i) {
        wchar_t section[32], ip[64], name[64], id[32]; swprintf_s(section,L"Friend%u",(unsigned)i);
        MultiByteToWideChar(CP_UTF8,0,peers[i].ip.c_str(),-1,ip,64); MultiByteToWideChar(CP_UTF8,0,peers[i].name.c_str(),-1,name,64);
        swprintf_s(id,L"%016llx",peers[i].xuid);
        ok=WritePrivateProfileStringW(section,L"IP",ip,temporary.c_str()) && ok;
        ok=WritePrivateProfileStringW(section,L"Name",name,temporary.c_str()) && ok;
        ok=WritePrivateProfileStringW(section,L"XUID",id,temporary.c_str()) && ok;
        swprintf_s(id,L"%u",(unsigned)peers[i].relationship);
        ok=WritePrivateProfileStringW(section,L"Relationship",id,temporary.c_str()) && ok;
        swprintf_s(id,L"%016llx",peers[i].request);
        ok=WritePrivateProfileStringW(section,L"Request",id,temporary.c_str()) && ok;
        swprintf_s(id,L"%u",peers[i].control);
        ok=WritePrivateProfileStringW(section,L"Control",id,temporary.c_str()) && ok;
        ok=WritePrivateProfileStringW(section,L"ControlAcknowledged",peers[i].controlAcknowledged?L"1":L"0",temporary.c_str()) && ok;
        std::wostringstream history;for(auto retired:peers[i].retiredRequests)history<<std::hex<<retired<<L' ';
        ok=WritePrivateProfileStringW(section,L"RetiredRequests",history.str().c_str(),temporary.c_str()) && ok;
    }
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,temporary.c_str());
    if(ok) ok=MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok) DeleteFileW(temporary.c_str());
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,path.c_str());
    return ok;
}
Packet Local(uint16_t kind) {
    Packet p={}; p.type=PacketType; p.magic=0x31465358; p.version=1; p.kind=kind;
    EnterCriticalSection(&xlive_critsec_local_user);
    const auto &u=xlive_local_users[localUser];
    if(u.signin_state==eXUserSigninState_SignedInToLive && u.online_enabled) {
        p.sender=u.xuid; strcpy_s(p.name,u.username); p.state=XONLINE_FRIENDSTATE_FLAG_ONLINE | XONLINE_FRIENDSTATE_FLAG_PLAYING;
    }
    LeaveCriticalSection(&xlive_critsec_local_user);
    p.title=xlive_title_id;
    EnterCriticalSection(&xlive_critsec_xsession);
    for(auto &entry:xlive_xsession_local_sessions) {
        auto s=entry.second->liveSession;
        if(s && (s->sessionFlags & XSESSION_CREATE_HOST) && (s->sessionFlags & XSESSION_CREATE_USES_PRESENCE) && !(s->sessionFlags & XSESSION_CREATE_INVITES_DISABLED)) {
            p.session=s->xnkid;
            if(!(s->sessionFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED) && !(entry.second->eState==XSESSION_STATE_INGAME && (s->sessionFlags & XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED)) && s->slotsPublicFilledCount+s->slotsPrivateFilledCount < s->slotsPublicMaxCount+s->slotsPrivateMaxCount)
                p.state |= XONLINE_FRIENDSTATE_FLAG_JOINABLE;
            break;
        }
    }
    LeaveCriticalSection(&xlive_critsec_xsession);
    return p;
}
bool Send(Peer &peer, Packet p) {
    if(!p.sender || !xlln_network_instance_port) return false;
    p.recipient=peer.xuid;
    auto out=new XLLN_NET_SEND_PACKET_INFO;
    sockaddr_in dest={}; dest.sin_family=AF_INET; dest.sin_port=htons(39001); InetPtonA(AF_INET,peer.ip.c_str(),&dest.sin_addr);
    memcpy(&out->destinationAddress,&dest,sizeof(dest));
    out->dataSize=sizeof(p); out->data=new uint8_t[sizeof(p)]; memcpy(out->data,&p,sizeof(p));
    if(!SendPacketToRemoteInstance(out)) {delete out; return false;} return true;
}
uint64_t NewRequestId() {
    unsigned int lo=0,hi=0;
    if(rand_s(&lo)==0 && rand_s(&hi)==0 && (lo || hi)) return ((uint64_t)hi<<32)|lo;
    FILETIME time;GetSystemTimeAsFileTime(&time);
    return ((((uint64_t)time.dwHighDateTime<<32)|time.dwLowDateTime) ^ ++serial) | 1;
}
void SendControl(Peer& peer,uint16_t kind) {
    Packet p=Local(kind);p.invite=peer.request;
    // Pending peers receive identity and request metadata, not game/session presence.
    p.state=0;p.session={};p.title=0;Send(peer,p);
}
void SendReceipt(Peer& peer,const Packet& received) {
    Packet p=Local(FriendReceipt);p.recipient=received.sender;
    p.invite=received.invite;p.state=received.kind;p.session={};p.title=0;Send(peer,p);
}
bool StorePeer(Peer& peer,const Peer& before) {
    if(Save()) return true;peer=before;return false;
}
void RetireRequest(Peer& peer,uint64_t id) {
    if(!id || std::find(peer.retiredRequests.begin(),peer.retiredRequests.end(),id)!=peer.retiredRequests.end())return;
    if(peer.retiredRequests.size()==16)peer.retiredRequests.erase(peer.retiredRequests.begin());
    peer.retiredRequests.push_back(id);
}
bool ReceiveFriend(const Packet& packet,const std::string& ip) {
    auto it=std::find_if(peers.begin(),peers.end(),[&](const Peer& p){return p.ip==ip;});
    Packet me=Local(1);
    if(!me.sender || packet.sender==me.sender || (packet.recipient && packet.recipient!=me.sender) || !packet.invite) return false;
    if(packet.kind!=FriendRequest && packet.kind!=FriendCancel && packet.recipient!=me.sender) return false;
    if(std::any_of(peers.begin(),peers.end(),[&](const Peer& p){return p.relationship==SocialRelationship::Blocked && p.xuid==packet.sender;}))return false;
    if(it==peers.end()) {
        if((packet.kind!=FriendRequest && packet.kind!=FriendCancel) || peers.size()>=MaxPeers ||
            std::count_if(peers.begin(),peers.end(),[](const Peer& p){return p.relationship==SocialRelationship::Incoming;})>=MaxIncomingRequests) return false;
        Peer p;p.ip=ip;p.name=packet.name;p.xuid=packet.sender;p.relationship=packet.kind==FriendRequest?SocialRelationship::Incoming:SocialRelationship::Removed;p.request=packet.invite;
        p.lastRequest=GetTickCount64();peers.push_back(p);
        if(!Save()) {peers.pop_back();return false;}
        SendReceipt(peers.back(),packet);
        XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS|XLLN_LOG_LEVEL_INFO,"Friends: received %s from %s, id=%016llx.",packet.kind==FriendRequest?"request":"cancellation",ip.c_str(),packet.invite);
        if(packet.kind==FriendRequest && !SocialOverlayNotifyFriend(localUser)) PostMessageW(xlln_window_hwnd,SocialMessage,localUser,1);
        return true;
    }
    Peer& p=*it;
    if(p.relationship==SocialRelationship::Blocked || (p.xuid && p.xuid!=packet.sender)) return false;
    if(packet.kind==FriendRequest) {
        if(std::find(p.retiredRequests.begin(),p.retiredRequests.end(),packet.invite)!=p.retiredRequests.end())return false;
        if(p.request==packet.invite) {
            if(p.relationship==SocialRelationship::Incoming) SendReceipt(p,packet);
            else if(p.control==FriendAccept || p.control==FriendDecline || p.control==FriendCancel) SendControl(p,p.control);
            return true; // A replay never recreates a declined/removed request.
        }
        if(p.relationship==SocialRelationship::Incoming) return false; // One outstanding request per address.
        if(p.relationship==SocialRelationship::Outgoing && me.sender<packet.sender) {
            auto before=p;RetireRequest(p,packet.invite);if(!StorePeer(p,before))return false;
            SendControl(p,FriendRequest);return true;
        }
        const auto now=GetTickCount64();
        if(p.lastRequest && now-p.lastRequest<60000 && p.relationship!=SocialRelationship::Outgoing) return false;
        auto before=p;
        // A request from an already accepted, pinned friend can repair their local list.
        // Otherwise explicit acceptance is required, including crossed requests.
        bool accepted=p.relationship==SocialRelationship::Friend && p.xuid==packet.sender;
        if(!accepted && std::count_if(peers.begin(),peers.end(),[](const Peer& peer){return peer.relationship==SocialRelationship::Incoming;})>=MaxIncomingRequests) return false;
        RetireRequest(p,p.request);p.xuid=packet.sender;p.name=packet.name;p.request=packet.invite;p.lastRequest=now;
        p.relationship=accepted?SocialRelationship::Friend:SocialRelationship::Incoming;
        p.control=accepted?FriendAccept:0;p.controlAcknowledged=false;
        p.incoming=p.outgoing=0;
        if(!StorePeer(p,before)) return false;
        SendReceipt(p,packet);if(accepted)SendControl(p,FriendAccept);
        else if(!SocialOverlayNotifyFriend(localUser))PostMessageW(xlln_window_hwnd,SocialMessage,localUser,1);
        return true;
    }
    if(packet.invite!=p.request) return false;
    auto before=p;
    if(packet.kind==FriendReceipt) {
        if(packet.state!=p.control || !p.control) return false;
        if(p.controlAcknowledged) return true;
        p.xuid=packet.sender;p.name=packet.name;p.controlAcknowledged=true;
        return StorePeer(p,before);
    }
    if(packet.kind==FriendAccept) {
        if(p.relationship==SocialRelationship::Removed && p.control==FriendCancel) {SendControl(p,FriendCancel);return true;}
        if(p.relationship!=SocialRelationship::Outgoing && p.relationship!=SocialRelationship::Friend) return false;
        bool added=p.relationship==SocialRelationship::Outgoing;
        p.relationship=SocialRelationship::Friend;p.xuid=packet.sender;p.name=packet.name;p.control=0;p.controlAcknowledged=false;
        if(!StorePeer(p,before)) return false;
        SendReceipt(p,packet);
        if(added)XLiveNotifyAddEvent(XN_FRIENDS_FRIEND_ADDED,localUser);
        if(added)XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS|XLLN_LOG_LEVEL_INFO,"Friends: request accepted by %s, id=%016llx.",ip.c_str(),packet.invite);
        Send(p,Local(1));return true;
    }
    if(packet.kind==FriendDecline) {
        if(p.relationship!=SocialRelationship::Outgoing && p.relationship!=SocialRelationship::Declined && p.relationship!=SocialRelationship::Removed) return false;
        if(p.relationship!=SocialRelationship::Removed)p.relationship=SocialRelationship::Declined;
        p.xuid=packet.sender;p.name=packet.name;p.control=0;p.controlAcknowledged=false;
        if(!StorePeer(p,before)) return false;
        SendReceipt(p,packet);return true;
    }
    if(packet.kind==FriendCancel) {
        // Cancellation may race the acceptance; only the matching transaction can revoke it.
        if(p.relationship==SocialRelationship::Outgoing) return false;
        bool removed=p.relationship==SocialRelationship::Friend;
        p.relationship=SocialRelationship::Removed;p.control=0;p.incoming=p.outgoing=0;p.seen=0;
        if(!StorePeer(p,before)) return false;
        SendReceipt(p,packet);if(removed)XLiveNotifyAddEvent(XN_FRIENDS_FRIEND_REMOVED,localUser);return true;
    }
    return false;
}
void Refresh() {
    if(!window) return;
    HWND list=GetDlgItem(window,1); LRESULT selected=SendMessageW(list,LB_GETCURSEL,0,0);
    SendMessageW(list,LB_RESETCONTENT,0,0);
    ULONGLONG now=GetTickCount64();
    for(auto &p:SocialPeers()) {
        std::string line=p.name+"  ["+p.ip+"]  ";
        if(p.relationship==SocialRelationship::Incoming)line+="Friend request received";
        else if(p.relationship==SocialRelationship::Outgoing)line+=p.requestDelivered?"Request delivered":"Request queued";
        else if(p.relationship==SocialRelationship::Declined)line+="Request declined";
        else if(p.relationship==SocialRelationship::Blocked)line+="Blocked";
        else line+=p.joinable?"Joinable":p.online?"Online":"Offline";
        if(p.incoming) line+=" - Invitation received";
        if(p.outgoing) line+=p.delivered?" - Invite delivered":" - Sending invite";
        SendMessageA(list,LB_ADDSTRING,0,(LPARAM)line.c_str());
    }
    if(selected>=0 && selected<(LRESULT)peers.size()) SendMessageW(list,LB_SETCURSEL,selected,0);
}
LRESULT CALLBACK Proc(HWND h,UINT msg,WPARAM w,LPARAM l) {
    std::lock_guard<std::recursive_mutex> lock(gate);
    if(msg==WM_DESTROY) { window=0; return 0; }
    if(msg==WM_CLOSE) {ShowWindow(h,SW_HIDE); return 0;}
    if(msg==WM_COMMAND) {
        if(!ready) return 0;
        int selected=(int)SendDlgItemMessageW(h,1,LB_GETCURSEL,0,0);
        auto views=SocialPeers();
        if(LOWORD(w)==4) {
            char ip[64], name[64]; GetDlgItemTextA(h,2,ip,64); GetDlgItemTextA(h,3,name,64);
            if(SocialAddPeer(ip,name)!=ERROR_SUCCESS) MessageBoxW(h,L"Could not queue request. Check the address, existing entry, and folder write access.",L"Friend request",MB_OK);
        } else if(selected>=0 && selected<(int)views.size()) {
            const auto p=views[selected];uint32_t result=ERROR_SUCCESS;
            if(LOWORD(w)==5) result=SocialRemovePeer(p.ip);
            if(LOWORD(w)==6) {XUID id=p.xuid;result=SocialInvite(localUser,1,&id);}
            if(LOWORD(w)==7) result=p.relationship==SocialRelationship::Incoming?SocialRespondFriend(p.ip,true):SocialJoinPeer(localUser,p.ip,true);
            if(LOWORD(w)==8) result=p.relationship==SocialRelationship::Incoming?SocialRespondFriend(p.ip,false):SocialDeclinePeer(p.ip);
            if(result!=ERROR_SUCCESS) MessageBoxW(h,L"Action unavailable. Check the selected request or friend and folder write access.",L"Friends",MB_OK);
        }
        Refresh(); return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
}
void SocialShow(uint32_t user) {
    if(user>=XLIVE_LOCAL_USER_COUNT) return;
    if(SocialOverlayShow(user)) return;
    // All controls are created on the existing XLLN window thread.
    PostMessageW(xlln_window_hwnd,WM_APP+711,user,0);
}
void SocialOpenOnUiThread(uint32_t user, bool activate) {
    if(user >= XLIVE_LOCAL_USER_COUNT) return;
    std::lock_guard<std::recursive_mutex> lock(gate); Load(); localUser=user;
    if(!window) {
        WNDCLASSW wc={}; wc.lpfnWndProc=Proc; wc.hInstance=xlln_hModule; wc.lpszClassName=L"XLLNFriendsV1"; wc.hCursor=LoadCursor(0,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1); RegisterClassW(&wc);
        window=CreateWindowW(wc.lpszClassName,L"Friends and invitations",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU, CW_USEDEFAULT,CW_USEDEFAULT,650,350,xlln_window_hwnd,0,xlln_hModule,0);
        auto control=[](const wchar_t* cls,const wchar_t* text,int id,int x,int y,int width,int height,DWORD style) {
            HWND c=CreateWindowW(cls,text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,x,y,width,height,window,(HMENU)(INT_PTR)id,xlln_hModule,0);
            SendMessageW(c,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
        };
        control(L"STATIC",L"Send a friend request using their IPv4 address. They only need to accept.",0,12,10,610,22,0);
        control(L"LISTBOX",L"",1,12,38,610,155,WS_BORDER|WS_VSCROLL|LBS_NOTIFY);
        control(L"STATIC",L"Peer IPv4",0,12,203,170,20,0); control(L"STATIC",L"Name",0,200,203,180,20,0);
        control(L"EDIT",L"",2,12,225,175,24,WS_BORDER|ES_AUTOHSCROLL); control(L"EDIT",L"",3,200,225,180,24,WS_BORDER|ES_AUTOHSCROLL);
        control(L"BUTTON",L"Request",4,395,225,100,24,0); control(L"BUTTON",L"Remove/Cancel",5,510,225,110,24,0);
        control(L"BUTTON",L"Invite",6,12,265,150,28,0); control(L"BUTTON",L"Accept",7,180,265,150,28,0); control(L"BUTTON",L"Decline",8,350,265,150,28,0);
    }
    Refresh(); ShowWindow(window,activate ? SW_SHOW : SW_SHOWNOACTIVATE);
    if(activate) SetForegroundWindow(window);
    else { FlashWindow(window,TRUE); MessageBeep(MB_ICONINFORMATION); }
}
void SocialSetReady(bool value) { std::lock_guard<std::recursive_mutex> lock(gate); ready=value; }
void SocialTick() {
    std::lock_guard<std::recursive_mutex> lock(gate); if(!ready) return; Load();
    ULONGLONG now=GetTickCount64(); if(now-lastTick<3000) return; lastTick=now;
    Packet p=Local(1);
    for(auto &peer:peers) {
        if(peer.control && (!peer.controlAcknowledged || peer.relationship==SocialRelationship::Outgoing)) SendControl(peer,peer.control);
        if(peer.relationship!=SocialRelationship::Friend) continue;
        Send(peer,p);
        if(peer.seen && now-peer.seen>=15000) {peer.seen=0; XLiveNotifyAddEvent(XN_FRIENDS_PRESENCE_CHANGED,localUser);}
        if(peer.outgoing && (!(p.state & XONLINE_FRIENDSTATE_FLAG_JOINABLE) || memcmp(&p.session,&peer.outgoingSession,sizeof(XNKID)))) peer.outgoing=0;
        if(peer.outgoing && now<peer.outgoingUntil && !peer.acknowledged) {Packet invite=Local(2); invite.invite=peer.outgoing; Send(peer,invite);}
    }
    Refresh();
}
bool SocialReceive(const uint8_t* data,size_t size,const SOCKADDR_STORAGE* source) {
    if(size!=sizeof(Packet) || source->ss_family!=AF_INET) return false;
    Packet packet; memcpy(&packet,data,size);
    if(packet.type!=PacketType || packet.magic!=0x31465358 || packet.version!=1 || packet.kind<1 || packet.kind>FriendReceipt || !packet.sender || !packet.name[0] || !memchr(packet.name,0,sizeof(packet.name))) return false;
    if(((const sockaddr_in*)source)->sin_port!=htons(39001)) return false;
    char ip[64]; InetNtopA(AF_INET,(void*)&((const sockaddr_in*)source)->sin_addr,ip,64);
    std::lock_guard<std::recursive_mutex> lock(gate); if(!ready) return false; Load();
    if(packet.kind>=FriendRequest) return ReceiveFriend(packet,ip);
    auto it=std::find_if(peers.begin(),peers.end(),[&](const Peer&p){return p.ip==ip;}); if(it==peers.end()) return false;
    Peer &p=*it;
    if(p.relationship!=SocialRelationship::Friend) return false;
    if(p.xuid && p.xuid!=packet.sender) return false;
    Packet me=Local(1); if(!me.sender || (packet.recipient && packet.recipient!=me.sender)) return false;
    bool added=!p.xuid; bool changed=!p.seen || p.info.dwFriendState!=packet.state || memcmp(&p.info.sessionID,&packet.session,sizeof(XNKID));
    p.xuid=packet.sender; p.name=packet.name; p.seen=GetTickCount64(); p.info={}; p.info.xuid=p.xuid; strcpy_s(p.info.szGamertag,packet.name);
    p.info.dwFriendState=packet.state; p.info.dwTitleID=packet.title; p.info.sessionID=packet.session; GetSystemTimeAsFileTime(&p.info.ftUserTime);
    if(added) {Save(); XLiveNotifyAddEvent(XN_FRIENDS_FRIEND_ADDED,localUser);}
    if(changed) XLiveNotifyAddEvent(XN_FRIENDS_PRESENCE_CHANGED,localUser);
    if(packet.kind==2 && packet.invite && packet.title==xlive_title_id && (packet.state & XONLINE_FRIENDSTATE_FLAG_JOINABLE)) {
        if(packet.invite!=p.lastIncoming) {p.lastIncoming=packet.invite; p.incoming=packet.invite; p.incomingUntil=p.seen+60000; p.incomingSession=packet.session; if(!SocialOverlayNotifyInvite(localUser)) PostMessageW(xlln_window_hwnd,WM_APP+711,localUser,1);}
        Packet ack=Local(3); ack.invite=packet.invite; Send(p,ack);
    }
    if(packet.invite && packet.invite==p.outgoing) {
        if(packet.kind==3) p.acknowledged=true;
        if(packet.kind==4 || packet.kind==5) p.outgoing=0;
    }
    return true;
}
std::vector<XONLINE_FRIEND> SocialFriends() {
    std::lock_guard<std::recursive_mutex> lock(gate); Load(); std::vector<XONLINE_FRIEND> result;
    auto now=GetTickCount64();
    for(auto &p:peers) if(p.relationship==SocialRelationship::Friend && p.xuid) {
        auto f=p.info; f.xuid=p.xuid; strncpy_s(f.szGamertag,p.name.c_str(),_TRUNCATE);
        if(!p.seen || now-p.seen>=15000) { f.dwFriendState=0; f.dwTitleID=0; memset(&f.sessionID,0,sizeof(f.sessionID)); }
        if(p.incoming && now<p.incomingUntil) { f.dwFriendState|=XONLINE_FRIENDSTATE_FLAG_RECEIVEDINVITE; f.xnkidInvite=p.incomingSession; }
        if(p.outgoing && now<p.outgoingUntil) f.dwFriendState|=XONLINE_FRIENDSTATE_FLAG_SENTINVITE;
        result.push_back(f);
    }
    return result;
}
bool SocialIsFriend(XUID id) { for(auto &f:SocialFriends()) if(f.xuid==id) return true; return false; }
std::vector<SocialPeerView> SocialPeers() {
    std::lock_guard<std::recursive_mutex> lock(gate); Load();
    std::vector<SocialPeerView> result;
    const auto now=GetTickCount64();
    for(const auto& p:peers) {
        if(p.relationship==SocialRelationship::Removed) continue;
        SocialPeerView v; v.ip=p.ip; v.name=p.name; v.xuid=p.xuid;
        v.relationship=p.relationship;v.requestDelivered=p.controlAcknowledged;
        v.confirmingFriendship=p.relationship==SocialRelationship::Friend && p.control==FriendAccept && !p.controlAcknowledged;
        if(p.relationship!=SocialRelationship::Friend) {result.push_back(v);continue;}
        v.online=p.seen && now-p.seen<15000 && (p.info.dwFriendState & XONLINE_FRIENDSTATE_FLAG_ONLINE);
        v.joinable=v.online && p.info.dwTitleID==xlive_title_id && (p.info.dwFriendState & XONLINE_FRIENDSTATE_FLAG_JOINABLE);
        v.incoming=p.incoming && now<p.incomingUntil;
        v.outgoing=p.outgoing && now<p.outgoingUntil; v.delivered=p.acknowledged;
        result.push_back(v);
    }
    return result;
}
uint32_t SocialAddPeer(const std::string& ip,const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(gate); Load();
    if(!ready) return ERROR_NOT_READY;
    IN_ADDR parsed;
    if(InetPtonA(AF_INET,ip.c_str(),&parsed)!=1 || !parsed.s_addr || parsed.s_addr==INADDR_BROADCAST || IN_MULTICAST(ntohl(parsed.s_addr)) || name.size()>=XUSER_NAME_SIZE) return ERROR_INVALID_PARAMETER;
    if(!Local(1).sender) return ERROR_NOT_LOGGED_ON;
    auto it=std::find_if(peers.begin(),peers.end(),[&](const Peer& p){return p.ip==ip;});
    if(it!=peers.end()) {
        if(it->relationship==SocialRelationship::Blocked) return ERROR_ACCESS_DENIED;
        if(it->relationship!=SocialRelationship::Removed && it->relationship!=SocialRelationship::Declined) return ERROR_ALREADY_EXISTS;
        auto before=*it;
        RetireRequest(*it,it->request);
        it->relationship=SocialRelationship::Outgoing;it->request=NewRequestId();it->control=FriendRequest;it->controlAcknowledged=false;
        it->name=name.empty()?ip:name;it->seen=0;it->incoming=it->outgoing=0;
        if(!StorePeer(*it,before)) return ERROR_WRITE_FAULT;
        SendControl(*it,FriendRequest);return ERROR_SUCCESS;
    }
    if(peers.size()>=MaxPeers) return ERROR_TOO_MANY_NAMES;
    Peer p; p.ip=ip; p.name=name.empty()?ip:name;p.relationship=SocialRelationship::Outgoing;
    p.request=NewRequestId();p.control=FriendRequest;peers.push_back(p);
    if(!Save()) { peers.pop_back(); return ERROR_WRITE_FAULT; }
    SendControl(peers.back(),FriendRequest);
    XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS|XLLN_LOG_LEVEL_INFO,"Friends: queued request to %s, id=%016llx.",ip.c_str(),p.request);
    return ERROR_SUCCESS;
}
uint32_t SocialRemovePeer(const std::string& ip) {
    std::lock_guard<std::recursive_mutex> lock(gate); Load();
    if(!ready) return ERROR_NOT_READY;
    auto it=std::find_if(peers.begin(),peers.end(),[&](const Peer& p){return p.ip==ip;});
    if(it==peers.end()) return ERROR_NOT_FOUND;
    if(it->relationship==SocialRelationship::Blocked) return ERROR_ACCESS_DENIED;
    if(it->relationship==SocialRelationship::Incoming) return SocialRespondFriend(ip,false);
    auto before=*it;
    it->control=it->relationship==SocialRelationship::Outgoing?FriendCancel:0;
    it->relationship=SocialRelationship::Removed;it->controlAcknowledged=false;it->seen=0;it->incoming=it->outgoing=0;
    if(!StorePeer(*it,before)) return ERROR_WRITE_FAULT;
    if(it->control)SendControl(*it,it->control);
    if(before.relationship==SocialRelationship::Friend)XLiveNotifyAddEvent(XN_FRIENDS_FRIEND_REMOVED,localUser);
    return ERROR_SUCCESS;
}
uint32_t SocialRespondFriend(const std::string& ip,bool accept) {
    std::lock_guard<std::recursive_mutex> lock(gate);Load();
    if(!ready) return ERROR_NOT_READY;
    if(!Local(1).sender) return ERROR_NOT_LOGGED_ON;
    auto it=std::find_if(peers.begin(),peers.end(),[&](const Peer& p){return p.ip==ip;});
    if(it==peers.end() || it->relationship!=SocialRelationship::Incoming) return ERROR_NOT_FOUND;
    auto before=*it;it->relationship=accept?SocialRelationship::Friend:SocialRelationship::Removed;
    it->control=accept?FriendAccept:FriendDecline;it->controlAcknowledged=false;
    if(!StorePeer(*it,before)) return ERROR_WRITE_FAULT;
    SendControl(*it,it->control);
    XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS|XLLN_LOG_LEVEL_INFO,"Friends: %s request from %s, id=%016llx.",accept?"accepted":"declined",ip.c_str(),it->request);
    if(accept) {XLiveNotifyAddEvent(XN_FRIENDS_FRIEND_ADDED,localUser);Send(*it,Local(1));}
    return ERROR_SUCCESS;
}
uint32_t SocialBlockPeer(const std::string& ip,bool block) {
    std::lock_guard<std::recursive_mutex> lock(gate);Load();
    if(!ready) return ERROR_NOT_READY;
    auto it=std::find_if(peers.begin(),peers.end(),[&](const Peer& p){return p.ip==ip;});
    if(it==peers.end()) return ERROR_NOT_FOUND;
    auto before=peers;const auto id=it->xuid;bool removed=false;
    // A known player's LAN and remote entries must obey the same block decision.
    for(auto& p:peers)if(p.ip==ip || (id && p.xuid==id)) {
        removed=removed || p.relationship==SocialRelationship::Friend;
        p.relationship=block?SocialRelationship::Blocked:SocialRelationship::Removed;
        p.control=0;p.controlAcknowledged=false;p.seen=0;p.incoming=p.outgoing=0;
    }
    if(!Save()) {peers=before;return ERROR_WRITE_FAULT;}
    if(removed)XLiveNotifyAddEvent(XN_FRIENDS_FRIEND_REMOVED,localUser);
    return ERROR_SUCCESS;
}
uint32_t SocialJoinPeer(uint32_t user,const std::string& ip,bool acceptInvite) {
    SOCKADDR_STORAGE storage={}; XNKID session={};
    {
        std::lock_guard<std::recursive_mutex> lock(gate); Load();
        if(!ready) return ERROR_NOT_READY;
        if(user>=XLIVE_LOCAL_USER_COUNT) return ERROR_INVALID_PARAMETER;
        auto it=std::find_if(peers.begin(),peers.end(),[&](const Peer& p){return p.ip==ip;});
        if(it==peers.end()) return ERROR_NOT_FOUND;
        auto& p=*it; const auto now=GetTickCount64();
        if(p.relationship!=SocialRelationship::Friend) return ERROR_ACCESS_DENIED;
        if(!p.seen || now-p.seen>=15000 || p.info.dwTitleID!=xlive_title_id) return ERROR_NOT_READY;
        if(acceptInvite ? (!p.incoming || now>=p.incomingUntil) : !(p.info.dwFriendState & XONLINE_FRIENDSTATE_FLAG_JOINABLE)) return ERROR_NOT_READY;
        localUser=user; if(!Local(1).sender) return ERROR_NOT_LOGGED_ON;
        session=acceptInvite?p.incomingSession:p.info.sessionID;
        sockaddr_in dest={}; dest.sin_family=AF_INET; dest.sin_port=htons(39001); InetPtonA(AF_INET,p.ip.c_str(),&dest.sin_addr);
        memcpy(&storage,&dest,sizeof(dest));
        if(acceptInvite) { Packet reply=Local(4); reply.invite=p.incoming; if(!Send(p,reply)) return ERROR_NETWORK_UNREACHABLE; p.incoming=0; }
    }
    // Never hold the peer lock while entering the title's connection path.
    char password[500]={}; GetDlgItemTextA(xlln_window_hwnd,XLLNControlsMessageNumbers::MAIN_TBX_DIRECT_IP_CONNECT_PASSWORD,password,sizeof(password));
    XllnDirectIpConnectTo(user,&storage,password,&session);
    return ERROR_SUCCESS; // Connection requested; completion remains asynchronous.
}
uint32_t SocialDeclinePeer(const std::string& ip) {
    std::lock_guard<std::recursive_mutex> lock(gate); Load();
    if(!ready) return ERROR_NOT_READY;
    auto it=std::find_if(peers.begin(),peers.end(),[&](const Peer& p){return p.ip==ip;});
    if(it==peers.end()) return ERROR_NOT_FOUND;
    if(it->relationship!=SocialRelationship::Friend) return ERROR_ACCESS_DENIED;
    if(!it->incoming || GetTickCount64()>=it->incomingUntil) return ERROR_NOT_READY;
    Packet reply=Local(5); reply.invite=it->incoming;
    if(!Send(*it,reply)) return ERROR_NETWORK_UNREACHABLE;
    it->incoming=0; return ERROR_SUCCESS;
}
uint32_t SocialInvite(uint32_t user,size_t count,const XUID* ids) {
    std::lock_guard<std::recursive_mutex> lock(gate); Load();
    if(!ready) return ERROR_NOT_READY;
    if(user>=XLIVE_LOCAL_USER_COUNT || !count || !ids) return ERROR_INVALID_PARAMETER;
    localUser=user; Packet packet=Local(2); if(!(packet.state & XONLINE_FRIENDSTATE_FLAG_JOINABLE)) return ERROR_NOT_READY;
    for(size_t i=0;i<count;++i) {
        auto it=std::find_if(peers.begin(),peers.end(),[&](const Peer&p){return p.relationship==SocialRelationship::Friend && p.xuid && p.xuid==ids[i] && p.seen && GetTickCount64()-p.seen<15000;});
        if(it==peers.end()) return ERROR_NOT_FOUND;
    }
    for(size_t i=0;i<count;++i) for(auto &p:peers) if(p.relationship==SocialRelationship::Friend && p.xuid==ids[i]) {
        packet.invite=(GetTickCount64()<<16) ^ ++serial; if(!Send(p,packet)) return ERROR_NETWORK_UNREACHABLE;
        p.outgoing=packet.invite; p.outgoingSession=packet.session; p.outgoingUntil=GetTickCount64()+60000; p.acknowledged=false;
    }
    return ERROR_SUCCESS;
}

uint32_t SocialReadSnapshot(std::vector<XONLINE_FRIEND>& snapshot, void* buffer, size_t bytes, size_t* count) {
    if (!count || !buffer) return ERROR_INVALID_PARAMETER;
    *count=0;
    if (snapshot.empty()) return ERROR_NO_MORE_FILES;
    if (bytes < sizeof(XONLINE_FRIEND)) return ERROR_INSUFFICIENT_BUFFER;
    *count=(std::min)(snapshot.size(),bytes/sizeof(XONLINE_FRIEND));
    memcpy(buffer,snapshot.data(),*count*sizeof(XONLINE_FRIEND));
    snapshot.erase(snapshot.begin(),snapshot.begin()+*count);
    return ERROR_SUCCESS;
}

std::vector<SOCKADDR_STORAGE> SocialPeerAddresses() {
    std::lock_guard<std::recursive_mutex> lock(gate);
    std::vector<SOCKADDR_STORAGE> result;
    if (!ready) return result;
    Load();
    for (const auto& peer : peers) {
        if(peer.relationship!=SocialRelationship::Friend) continue;
        sockaddr_in address={}; address.sin_family=AF_INET; address.sin_port=htons(39000);
        InetPtonA(AF_INET,peer.ip.c_str(),&address.sin_addr);
        SOCKADDR_STORAGE storage={}; memcpy(&storage,&address,sizeof(address)); result.push_back(storage);
    }
    return result;
}
