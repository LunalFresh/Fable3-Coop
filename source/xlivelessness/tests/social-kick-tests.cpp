// Test the real session/queue guards; native game code is replaced at the boundary.
#include "../xlivelessness/xlln/social-kick.cpp"
#include <cassert>
#include <cstdio>
#include <crtdbg.h>
CRITICAL_SECTION xlive_critsec_xsession;
std::map<HANDLE,LIVE_SESSION_XSESSION*> xlive_xsession_local_sessions;
bool panelOpen=true, uiClosed=false;
int kicks=0;
bool SocialOverlayIsOpen(){return panelOpen;}
bool XLiveNotifyIsSystemUIClosed(){return uiClosed;}
uint32_t WINAPI XLLNDebugLogF(uint32_t,const char* const,...){return 0;}
void WINAPI Native(void* context){assert(context==(void*)123);assert(!panelOpen && uiClosed);++kicks;}
void Tick(bool host=true,uintptr_t link=456){XLLNSocialKickUpdate(1,host,(void*)123,link,Native);}
int main(){
    _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    InitializeCriticalSection(&xlive_critsec_xsession);
    LIVE_SESSION live;live.sessionFlags=XSESSION_CREATE_HOST|XSESSION_CREATE_USES_PRESENCE;live.xnkid={};
    LIVE_SESSION_XSESSION session;session.liveSession=&live;
    auto local=new XSESSION_MEMBER{};local->xuidOnline=111;local->dwUserIndex=0;
    auto remote=new XSESSION_MEMBER{};remote->xuidOnline=222;remote->dwUserIndex=XLIVE_LOCAL_USER_INVALID;
    session.sessionMembers[111]=local;xlive_xsession_local_sessions[(HANDLE)1]=&session;
    Tick();assert(!SocialKickToken(111) && !SocialKickToken(222));
    session.sessionMembers[222]=remote;Tick(false);assert(!SocialKickToken(222));
    Tick();auto token=SocialKickToken(222);assert(token && !SocialKickToken(333));
    assert(SocialKickRequest(333,token)==ERROR_NOT_READY);
    // No native call until both panel closure and the title's consumed UI-close state.
    assert(!SocialKickRequest(222,token));Tick();assert(!kicks);
    panelOpen=false;Tick();assert(!kicks);uiClosed=true;Tick();assert(kicks==1);
    Tick();assert(kicks==1);assert(SocialKickRequest(222,token)==ERROR_NOT_READY);
    // Cancelling, changing the session, replacing the link, and losing host status
    // all invalidate previously confirmed actions without touching friendship data.
    panelOpen=true;uiClosed=false;token=SocialKickToken(222);
    assert(!SocialKickRequest(222,token));SocialKickCancel();panelOpen=false;uiClosed=true;Tick();assert(kicks==1);
    token=SocialKickToken(222);live.xnkid.ab[0]=1;Tick();assert(SocialKickRequest(222,token)==ERROR_NOT_READY);
    token=SocialKickToken(222);assert(!SocialKickRequest(222,token));Tick(true,789);assert(kicks==1);
    uint32_t result=0;assert(SocialKickTakeResult(result) && result==ERROR_NOT_READY);
    token=SocialKickToken(222);assert(!SocialKickRequest(222,token));Tick(false,789);assert(kicks==1);
    Tick();token=SocialKickToken(222);assert(!SocialKickRequest(222,token));remote->dwFlags=XSESSION_MEMBER_FLAGS_ZOMBIE;Tick();assert(kicks==1 && !SocialKickToken(222));
    remote->dwFlags=0;Tick();auto another=new XSESSION_MEMBER{};another->xuidOnline=333;another->dwUserIndex=XLIVE_LOCAL_USER_INVALID;
    session.sessionMembers[333]=another;Tick();assert(!SocialKickToken(222));session.sessionMembers.erase(333);delete another;
    Tick();token=SocialKickToken(222);assert(!SocialKickRequest(222,token));deadline=0;Tick();assert(kicks==1);
    Tick();lastUpdate=GetTickCount64()-1501;assert(!SocialKickToken(222));
    assert(XLLNSocialKickUpdate(2,TRUE,(void*)123,456,Native)==ERROR_REVISION_MISMATCH);
    xlive_xsession_local_sessions.clear();DeleteCriticalSection(&xlive_critsec_xsession);
    puts("PASS: host/current-guest guards, UI-close handoff, stale confirmation, disconnect/rejoin, cancellation, timeout, one-shot dispatch");
}
