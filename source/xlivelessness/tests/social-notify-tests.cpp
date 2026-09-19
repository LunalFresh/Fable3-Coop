// Exercise the real notification queue used by the panel's deferred join.
#include "../xlivelessness/xlive/xnotify.cpp"
#include <cassert>
#include <cstdio>
#include <crtdbg.h>
uint32_t WINAPI XLLNDebugLogF(uint32_t,const char* const,...) {return 0;}
void XllnTraceFunc(const char*) {}
int main() {
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    InitializeCriticalSection(&xlive_critsec_xnotify);InitXNotify();
    HANDLE listener=XNotifyCreateListener(XNOTIFY_SYSTEM|XNOTIFY_LIVE);
    uint32_t id=0,value=0;
    assert(XLiveNotifyIsSystemUIClosed());
    XLiveNotifyAddEvent(XN_SYS_UI,1);
    assert(!XLiveNotifyIsSystemUIClosed());
    assert(XNotifyGetNext(listener,0,&id,&value) && id==XN_SYS_UI && value==1);
    assert(!XLiveNotifyIsSystemUIClosed());
    // Publishing close alone is insufficient; the title must receive it.
    XLiveNotifyAddEvent(XN_SYS_UI,0);
    assert(!XLiveNotifyIsSystemUIClosed());
    assert(!XNotifyGetNext(listener,XN_LIVE_INVITE_ACCEPTED,&id,&value));
    assert(!XLiveNotifyIsSystemUIClosed());
    assert(XNotifyGetNext(listener,0,&id,&value) && id==XN_SYS_UI && value==0);
    assert(XLiveNotifyIsSystemUIClosed());
    XLiveNotifyAddEvent(XN_LIVE_INVITE_ACCEPTED,0);
    assert(XNotifyGetNext(listener,0,&id,&value) && id==XN_LIVE_INVITE_ACCEPTED);
    assert(!XNotifyGetNext(listener,0,&id,&value));
    // Fable has multiple LIVE subscribers; an unrelated handler must not steal
    // the accepted invite before the multiplayer handler gets its own copy.
    HANDLE multiplayer=XNotifyCreateListener(XNOTIFY_LIVE);
    XLiveNotifyAddEvent(XN_LIVE_INVITE_ACCEPTED,2);
    assert(XNotifyGetNext(listener,0,&id,&value) && id==XN_LIVE_INVITE_ACCEPTED && value==2);
    assert(!XNotifyGetNext(listener,0,&id,&value));
    assert(XNotifyGetNext(multiplayer,0,&id,&value) && id==XN_LIVE_INVITE_ACCEPTED && value==2);
    assert(!XNotifyGetNext(multiplayer,0,&id,&value));
    // Filtered polling preserves both delivery and the listener's wakeup.
    XLiveNotifyAddEvent(XN_LIVE_INVITE_ACCEPTED,1);
    assert(!XNotifyGetNext(multiplayer,XN_LIVE_CONNECTIONCHANGED,&id,&value));
    assert(WaitForSingleObject(multiplayer,0)==WAIT_OBJECT_0);
    assert(XNotifyGetNext(multiplayer,XN_LIVE_INVITE_ACCEPTED,&id,&value) && value==1);
    assert(!XNotifyGetNext(multiplayer,XN_LIVE_INVITE_ACCEPTED,&id,&value));
    assert(WaitForSingleObject(multiplayer,0)==WAIT_TIMEOUT);
    // Later subscribers do not receive old invitations; closing one subscriber
    // leaves the original subscriber's still-pending notification intact.
    HANDLE late=XNotifyCreateListener(XNOTIFY_LIVE);
    assert(!XNotifyGetNext(late,0,&id,&value));
    XLiveNotifyDeleteListener(multiplayer);CloseHandle(multiplayer);
    assert(XNotifyGetNext(listener,XN_LIVE_INVITE_ACCEPTED,&id,&value) && value==1);
    // Match the existing latest-value coalescing behavior independently per listener.
    XLiveNotifyAddEvent(XN_LIVE_INVITE_ACCEPTED,1);
    XLiveNotifyAddEvent(XN_LIVE_INVITE_ACCEPTED,3);
    assert(XNotifyGetNext(listener,0,&id,&value) && id==XN_LIVE_INVITE_ACCEPTED && value==3);
    assert(XNotifyGetNext(late,0,&id,&value) && id==XN_LIVE_INVITE_ACCEPTED && value==3);
    assert(!XNotifyGetNext(listener,0,&id,&value));
    assert(!XNotifyGetNext(late,0,&id,&value));
    XLiveNotifyDeleteListener(late);CloseHandle(late);
    XLiveNotifyAddEvent(XN_SYS_UI,1);assert(!XLiveNotifyIsSystemUIClosed());
    XLiveNotifyDeleteListener(listener);CloseHandle(listener);
    // Preserve accepted invites posted while no LIVE listener exists.
    XLiveNotifyAddEvent(XN_LIVE_INVITE_ACCEPTED,2);
    HANDLE first=XNotifyCreateListener(XNOTIFY_LIVE);
    assert(XNotifyGetNext(first,0,&id,&value) && id==XN_LIVE_INVITE_ACCEPTED && value==2);
    XLiveNotifyAddEvent(XN_LIVE_INVITE_ACCEPTED,1);
    XLiveNotifyDeleteListener(first);CloseHandle(first);
    HANDLE replacement=XNotifyCreateListener(XNOTIFY_LIVE);
    assert(XNotifyGetNext(replacement,0,&id,&value) && value==1);
    assert(!XNotifyGetNext(replacement,0,&id,&value));
    XLiveNotifyDeleteListener(replacement);CloseHandle(replacement);UninitXNotify();
    DeleteCriticalSection(&xlive_critsec_xnotify);
    puts("PASS: UI-close gate; per-listener invites; filtered polling and wakeup; no duplicates; late/replaced/deleted listeners; coalescing and no-listener retention");
}
