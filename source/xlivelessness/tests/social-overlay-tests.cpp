// Exercises the actual panel on a hidden D3D9 device; no running game is targeted.
#include "../xlivelessness/xlln/social-overlay.cpp"
#include <cassert>
#include <cstdio>
#include <crtdbg.h>

std::vector<SocialPeerView> testPeers;
std::vector<bool> uiNotices;
int forwardedKeys=0, inviteCalls=0, joinCalls=0, acceptCalls=0, declineCalls=0;
int friendAcceptCalls=0, friendDeclineCalls=0, blockCalls=0;
uint32_t SocialRespondFriend(const std::string&,bool accept) {accept?++friendAcceptCalls:++friendDeclineCalls;return 0;}
uint32_t SocialBlockPeer(const std::string&,bool) {++blockCalls;return 0;}
bool titleConsumedClose=false;
int kickRequests=0;
uint64_t SocialKickToken(XUID id){return id==456?7:0;}
uint32_t SocialKickRequest(XUID id,uint64_t token){assert(id==456 && token==7);++kickRequests;return 0;}
void SocialKickCancel(){}
bool SocialKickTakeResult(uint32_t&){return false;}
uint32_t joinResult=ERROR_SUCCESS;
bool XLiveNotifyIsSystemUIClosed() {return titleConsumedClose;}
uint32_t WINAPI XLLNDebugLogF(uint32_t,const char* const,...) {return 0;}
bool XllnUpdateXliveSystemUIOpenState() {uiNotices.push_back(SocialOverlayIsOpen());return true;}
std::vector<SocialPeerView> SocialPeers() {return testPeers;}
uint32_t SocialAddPeer(const std::string& address,const std::string& label) {
    if(address.empty() || label.empty())return ERROR_INVALID_PARAMETER;
    SocialPeerView p;p.ip=address;p.name=label;testPeers.push_back(p);return 0;
}
uint32_t SocialRemovePeer(const std::string& address) {
    testPeers.erase(std::remove_if(testPeers.begin(),testPeers.end(),[&](const SocialPeerView& p){return p.ip==address;}),testPeers.end());return 0;
}
uint32_t SocialInvite(uint32_t,size_t,const XUID*) {++inviteCalls;return 0;}
uint32_t SocialJoinPeer(uint32_t,const std::string&,bool accept) {
    assert(titleConsumedClose && !SocialOverlayIsOpen());
    accept?++acceptCalls:++joinCalls;return joinResult;
}
uint32_t SocialDeclinePeer(const std::string&) {++declineCalls;return 0;}
LRESULT CALLBACK TestProc(HWND w,UINT m,WPARAM k,LPARAM l) {
    if(m==WM_KEYDOWN)++forwardedKeys;
    return DefWindowProcW(w,m,k,l);
}
void SaveBitmap(const wchar_t* path,void* data,int width,int height) {
    BITMAPFILEHEADER file={};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+width*height*4;
    BITMAPINFOHEADER info={};info.biSize=sizeof(info);info.biWidth=width;info.biHeight=-height;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
    FILE* out=nullptr;_wfopen_s(&out,path,L"wb");assert(out);
    fwrite(&file,sizeof(file),1,out);fwrite(&info,sizeof(info),1,out);fwrite(data,width*height*4,1,out);fclose(out);
}
int main() {
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    assert(!SocialOverlayShow(0));
    WNDCLASSW wc={};wc.lpfnWndProc=TestProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"XllnSocialRenderTest";
    RegisterClassW(&wc);
    HWND testWindow=CreateWindowW(wc.lpszClassName,L"Isolated friends panel test",WS_OVERLAPPEDWINDOW,0,0,1000,800,nullptr,nullptr,wc.hInstance,nullptr);
    assert(testWindow);
    IDirect3D9* d3d=Direct3DCreate9(D3D_SDK_VERSION);assert(d3d);
    D3DPRESENT_PARAMETERS pp={};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=testWindow;
    pp.BackBufferWidth=1000;pp.BackBufferHeight=800;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
    IDirect3DDevice9* testDevice=nullptr;
    assert(SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,testWindow,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&testDevice)));
    SocialOverlayAttach(testDevice,testWindow);
    assert(SocialOverlayShow(0));
    auto render=[&] {
        assert(SUCCEEDED(testDevice->Clear(0,nullptr,D3DCLEAR_TARGET,D3DCOLOR_XRGB(64,56,42),1,0)));
        assert(SUCCEEDED(testDevice->BeginScene()));SocialOverlayRender();assert(SUCCEEDED(testDevice->EndScene()));
    };
    auto click=[&](int x,int y) {
        RECT client={};GetClientRect(testWindow,&client);
        int cx=(int)((left+x*scale)*client.right/1000),cy=(int)((top+y*scale)*client.bottom/800);
        WindowProc(testWindow,WM_LBUTTONDOWN,0,MAKELPARAM(cx,cy));Consume(testPeers);
    };
    render();assert(!uiNotices.empty() && uiNotices.back());
    assert(texture && canvas);SaveBitmap(L"friends-empty.bmp",pixels,Width,Height);
    // The window callback consumes panel input and releases it again when closed.
    WindowProc(testWindow,WM_KEYDOWN,VK_TAB,0);Consume(testPeers);assert(focus==1 && forwardedKeys==0);
    const char* address="100.64.0.2";
    for(const char* p=address;*p;++p)WindowProc(testWindow,WM_CHAR,*p,0);
    WindowProc(testWindow,WM_KEYDOWN,VK_TAB,0);
    for(char c:std::string("AJ"))WindowProc(testWindow,WM_CHAR,c,0);
    WindowProc(testWindow,WM_KEYDOWN,VK_RETURN,0);Consume(testPeers);
    assert(testPeers.size()==1 && testPeers[0].ip==address && testPeers[0].name=="AJ");
    testPeers[0].online=true;testPeers[0].joinable=true;testPeers[0].xuid=456;
    SocialPeerView incoming;incoming.ip="100.64.0.3";incoming.name="Morgan";incoming.online=true;incoming.incoming=true;incoming.xuid=789;testPeers.push_back(incoming);
    SocialPeerView offline;offline.ip="198.51.100.20";offline.name="Offline friend";testPeers.push_back(offline);
    selected=0;status.clear();dirty=true;
    // State left by the game must be restored after drawing the panel.
    testDevice->SetRenderState(D3DRS_ZENABLE,TRUE);testDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
    testDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);testDevice->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
    testDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_DESTALPHA);testDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_SRCALPHA);
    testDevice->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_REVSUBTRACT);testDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,TRUE);
    render();DWORD value=0;testDevice->GetRenderState(D3DRS_ZENABLE,&value);assert(value==TRUE);
    testDevice->GetRenderState(D3DRS_ALPHABLENDENABLE,&value);assert(value==TRUE);
    testDevice->GetRenderState(D3DRS_CULLMODE,&value);assert(value==D3DCULL_CW);
    testDevice->GetSamplerState(0,D3DSAMP_ADDRESSU,&value);assert(value==D3DTADDRESS_WRAP);
    testDevice->GetRenderState(D3DRS_SRCBLEND,&value);assert(value==D3DBLEND_DESTALPHA);
    testDevice->GetRenderState(D3DRS_DESTBLEND,&value);assert(value==D3DBLEND_SRCALPHA);
    testDevice->GetRenderState(D3DRS_BLENDOP,&value);assert(value==D3DBLENDOP_REVSUBTRACT);
    testDevice->GetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,&value);assert(value==TRUE);
    // Glass is translucent, corners are transparent, and colors are premultiplied.
    auto rgba=(uint32_t*)pixels;assert(rgba[0]==0);
    assert((rgba[300*Width+400]>>24)>0 && (rgba[300*Width+400]>>24)<255);
    for(int i=0;i<Width*Height;++i) {auto c=rgba[i];auto a=c>>24;assert(((c>>16)&255)<=a && ((c>>8)&255)<=a && (c&255)<=a);}
    SaveBitmap(L"friends-populated.bmp",pixels,Width,Height);
    click(60,375);assert(confirmGuest==456 && kickRequests==0);render();
    SaveBitmap(L"friends-kick-confirm.bmp",pixels,Width,Height);
    // Escape cancels confirmation and keeps the friends panel open.
    WindowProc(testWindow,WM_KEYDOWN,VK_ESCAPE,0);Consume(testPeers);assert(opened && !confirmGuest && !kickRequests);
    selected=1;Act(5,testPeers);assert(!confirmGuest);selected=0;Act(5,testPeers);
    click(650,415);assert(confirmGuest==456 && testPeers.size()==3); // Underlying Remove is blocked.
    click(450,345);assert(kickRequests==1 && !opened && !confirmGuest);
    SocialOverlayShow(0);render();status.clear();dirty=true;render();
    // Verify that real D3D9 output contains the panel, not just the CPU bitmap.
    IDirect3DSurface9* back=nullptr;IDirect3DSurface9* readback=nullptr;
    assert(SUCCEEDED(testDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)));
    assert(SUCCEEDED(testDevice->CreateOffscreenPlainSurface(1000,800,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr)));
    assert(SUCCEEDED(testDevice->GetRenderTargetData(back,readback)));
    D3DLOCKED_RECT locked={};assert(SUCCEEDED(readback->LockRect(&locked,nullptr,D3DLOCK_READONLY)));
    std::vector<uint32_t> rendered(1000*800);for(int y=0;y<800;++y)memcpy(rendered.data()+1000*y,(char*)locked.pBits+locked.Pitch*y,4000);
    assert((rendered[400*1000+500]&0xffffff)!=0x40382a);
    assert((rendered[0]&0xffffff)==0x40382a);
    auto glassPixel=((uint32_t*)pixels)[300*Width+400];
    auto screenPixel=rendered[(int)(top+300*scale)*1000+(int)(left+400*scale)];
    int expectedRed=((glassPixel>>16)&255)+(64*(255-(glassPixel>>24))+127)/255;
    assert(std::abs((int)((screenPixel>>16)&255)-expectedRed)<=2);
    SaveBitmap(L"friends-d3d9.bmp",rendered.data(),1000,800);readback->UnlockRect();readback->Release();back->Release();
    Act(0,testPeers);assert(inviteCalls==1);
    Act(1,testPeers);assert(joinCalls==0 && !opened);render();assert(!uiNotices.back() && joinCalls==0);
    // Match the observed delayed UI close: repeated frames cannot dispatch early.
    render();assert(joinCalls==0);
    titleConsumedClose=true;render();assert(joinCalls==1);render();assert(joinCalls==1);
    SocialOverlayShow(0);render();selected=1;Act(3,testPeers);assert(declineCalls==1);
    titleConsumedClose=false;Act(2,testPeers);assert(acceptCalls==0 && !opened);render();assert(acceptCalls==0);
    titleConsumedClose=true;render();assert(acceptCalls==1);
    // A stale/expired peer reopens the panel with the actual failure.
    SocialOverlayShow(0);render();joinResult=ERROR_NOT_READY;selected=0;
    titleConsumedClose=false;Act(1,testPeers);render();titleConsumedClose=true;render();
    assert(opened && status.find(L"Unavailable")!=std::wstring::npos && joinCalls==2);
    joinResult=ERROR_SUCCESS;
    // Reopening or detaching cancels a queued action rather than joining later.
    titleConsumedClose=false;Act(1,testPeers);render();SocialOverlayShow(0);render();
    titleConsumedClose=true;render();assert(joinCalls==2 && !joinPending);
    SocialOverlayShow(0);render();
    // Managed texture resources must not block a title Reset, including while open.
    assert(SUCCEEDED(testDevice->Reset(&pp)));SocialOverlayReset(testWindow);render();assert(opened && texture);
    WindowProc(testWindow,WM_KEYDOWN,VK_ESCAPE,0);Consume(testPeers);render();assert(!opened && !uiNotices.back());
    WindowProc(testWindow,WM_KEYDOWN,'W',0);assert(forwardedKeys==1);
    assert(SocialOverlayNotifyInvite(0));render();assert(!opened);
    SaveBitmap(L"friends-invite-toast.bmp",pixels,Width,96);
    WindowProc(testWindow,WM_KEYDOWN,VK_F6,0);render();assert(opened);
    // Friend acceptance stays in the menu and must not dispatch a game join.
    SocialPeerView request;request.name="New friend";request.ip="100.64.0.4";request.xuid=1001;request.relationship=SocialRelationship::Incoming;
    testPeers.push_back(request);selected=(int)testPeers.size()-1;
    auto oldJoinCalls=joinCalls;auto oldAcceptCalls=acceptCalls;
    dirty=true;render();SaveBitmap(L"friends-request.bmp",pixels,Width,Height);
    Act(0,testPeers);Act(1,testPeers);assert(opened && !joinPending && joinCalls==oldJoinCalls);
    Act(2,testPeers);assert(friendAcceptCalls==1 && acceptCalls==oldAcceptCalls && opened && !joinPending);
    Act(3,testPeers);assert(friendDeclineCalls==1 && opened);
    Act(6,testPeers);assert(blockCalls==1);
    SocialOverlayClose();render();SocialOverlayNotifyFriend(0);render();SaveBitmap(L"friends-request-toast.bmp",pixels,Width,96);
    WindowProc(testWindow,WM_KEYDOWN,VK_F6,0);render();assert(opened);
    titleConsumedClose=false;selected=0;Act(1,testPeers);render();assert(joinPending);
    SocialOverlayDetach();assert(!opened && !available && !texture && !uiNotices.back());
    titleConsumedClose=true;DispatchPendingJoin();assert(!joinPending && joinCalls==2);
    assert((WNDPROC)GetWindowLongPtrW(testWindow,GWLP_WNDPROC)==TestProc);
    testDevice->Release();d3d->Release();DestroyWindow(testWindow);
    puts("PASS: D3D9 pixels, render-state restoration, input consumption, backend dispatch, invite toast, reset while open, and teardown");
}
