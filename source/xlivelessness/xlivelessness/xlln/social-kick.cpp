#include <winsock2.h>
#include "social-kick.hpp"
#include "social-overlay.hpp"
#include "debug-log.hpp"
#include "../xlive/xsession.hpp"
#include "../xlive/xnotify.hpp"
#include <mutex>

namespace {
std::mutex kickGate;
struct Guest {
    XUID xuid=0;
    XNKID session={};
    void* context=nullptr;
    uintptr_t link=0;
};
Guest currentGuest;
uint64_t generation=1, pendingToken=0;
ULONGLONG lastUpdate=0, deadline=0;
bool resultReady=false;
uint32_t lastResult=ERROR_SUCCESS;

Guest ReadGuest(void* context, uintptr_t link) {
    Guest result;
    bool ambiguous=false;
    EnterCriticalSection(&xlive_critsec_xsession);
    for(const auto& entry:xlive_xsession_local_sessions) {
        auto details=entry.second;
        auto session=details?details->liveSession:nullptr;
        if(!session || !(session->sessionFlags & XSESSION_CREATE_HOST) ||
           !(session->sessionFlags & XSESSION_CREATE_USES_PRESENCE)) continue;
        for(const auto& member:details->sessionMembers) {
            auto m=member.second;
            if(!m || !m->xuidOnline || m->dwUserIndex!=XLIVE_LOCAL_USER_INVALID ||
               (m->dwFlags & XSESSION_MEMBER_FLAGS_ZOMBIE)) continue;
            if(result.xuid) ambiguous=true;
            result.xuid=m->xuidOnline;result.session=session->xnkid;
        }
    }
    LeaveCriticalSection(&xlive_critsec_xsession);
    if(ambiguous || !result.xuid)return Guest{};
    result.context=context;result.link=link;
    return result;
}
bool Same(const Guest& a,const Guest& b) {
    return a.xuid==b.xuid && a.context==b.context && a.link==b.link &&
        !memcmp(&a.session,&b.session,sizeof(XNKID));
}
}

uint64_t SocialKickToken(XUID guest) {
    std::lock_guard<std::mutex> lock(kickGate);
    return guest && currentGuest.xuid==guest && !pendingToken &&
        GetTickCount64()-lastUpdate<1500 ? generation : 0;
}
uint32_t SocialKickRequest(XUID guest,uint64_t token) {
    std::lock_guard<std::mutex> lock(kickGate);
    if(!guest || currentGuest.xuid!=guest || !token || token!=generation ||
       pendingToken || GetTickCount64()-lastUpdate>=1500)return ERROR_NOT_READY;
    pendingToken=token;deadline=GetTickCount64()+5000;resultReady=false;
    XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS|XLLN_LOG_LEVEL_INFO,
        "SocialKick: confirmed guest=%016llx; waiting for UI close on game update.",guest);
    return ERROR_SUCCESS;
}
void SocialKickCancel() {
    std::lock_guard<std::mutex> lock(kickGate);
    pendingToken=0;resultReady=false;
}
bool SocialKickTakeResult(uint32_t& result) {
    std::lock_guard<std::mutex> lock(kickGate);
    if(!resultReady)return false;
    result=lastResult;resultReady=false;return true;
}
uint32_t WINAPI XLLNSocialKickUpdate(uint32_t version,BOOL hostReady,
    void* context,uintptr_t link,SocialNativeKick kick) {
    if(version!=1)return ERROR_REVISION_MISMATCH;
    Guest observed=hostReady && context && link && kick?ReadGuest(context,link):Guest{};
    bool dispatch=false;
    {
        std::lock_guard<std::mutex> lock(kickGate);
        if(!Same(currentGuest,observed)) {
            currentGuest=observed;++generation;
            XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS|XLLN_LOG_LEVEL_INFO,
                "SocialKick: current host guest=%016llx link=%p generation=%llu.",observed.xuid,(void*)observed.link,generation);
        }
        lastUpdate=GetTickCount64();
        if(pendingToken) {
            if(pendingToken!=generation || !observed.xuid || lastUpdate>=deadline) {
                pendingToken=0;lastResult=ERROR_NOT_READY;resultReady=true;
                XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS|XLLN_LOG_LEVEL_INFO,
                    "SocialKick: cancelled because the current guest/session changed or UI close timed out.");
            } else if(!SocialOverlayIsOpen() && XLiveNotifyIsSystemUIClosed()) {
                pendingToken=0;currentGuest=Guest{};++generation;
                lastResult=ERROR_SUCCESS;resultReady=true;dispatch=true;
            }
        }
    }
    // Never hold our mutex or the XSession lock across native teardown callbacks.
    if(dispatch) {
        XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS|XLLN_LOG_LEVEL_INFO,
            "SocialKick: invoking native host kick for guest=%016llx link=%p.",observed.xuid,(void*)link);
        kick(context);
    }
    return ERROR_SUCCESS;
}
