#include "../xlivelessness/xlln/social.cpp"
#include <cassert>
#include <cstdio>
#include <crtdbg.h>
HINSTANCE xlln_hModule = nullptr;
HWND xlln_window_hwnd = nullptr;
CRITICAL_SECTION xlive_critsec_local_user;
LOCAL_USER_INFO xlive_local_users[XLIVE_LOCAL_USER_COUNT];
CRITICAL_SECTION xlive_critsec_xsession;
std::map<HANDLE,LIVE_SESSION_XSESSION*> xlive_xsession_local_sessions;
uint32_t xlive_title_id=0x4d53090a;
uint16_t xlln_network_instance_port=39001;
std::vector<Packet> sent;
std::vector<uint32_t> notices;
uint32_t WINAPI XLLNDebugLogF(uint32_t,const char* const,...) {return 0;}
bool SendPacketToRemoteInstance(XLLN_NET_SEND_PACKET_INFO* p) {
    assert(p->dataSize==sizeof(Packet)); Packet packet; memcpy(&packet,p->data,sizeof(packet)); sent.push_back(packet); delete p; return true;
}
void XLiveNotifyAddEvent(uint32_t id,size_t) { notices.push_back(id); }
bool SocialOverlayShow(uint32_t) {return false;}
bool SocialOverlayNotifyInvite(uint32_t) {return false;}
bool SocialOverlayNotifyFriend(uint32_t) {return false;}
int joins=0;XNKID joinedSession={};
void XllnDirectIpConnectTo(uint32_t,SOCKADDR_STORAGE*,const char*,const XNKID* session) {++joins;joinedSession=*session;}
int main() {
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE); _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    InitializeCriticalSection(&xlive_critsec_local_user); InitializeCriticalSection(&xlive_critsec_xsession);
    loaded=true; ready=true; wchar_t testPath[MAX_PATH]; GetFullPathNameW(L"social-test-friends.ini",MAX_PATH,testPath,0); path=testPath;
    auto &u=xlive_local_users[0];u.xuid=123;u.online_enabled=true;u.signin_state=eXUserSigninState_SignedInToLive;strcpy_s(u.username,"Host");
    Peer p;p.ip="100.64.0.2";p.name="Friend";peers.push_back(p);
    sockaddr_in src={};src.sin_family=AF_INET;src.sin_port=htons(39001);InetPtonA(AF_INET,p.ip.c_str(),&src.sin_addr);
    SOCKADDR_STORAGE address={};memcpy(&address,&src,sizeof(src));
    Packet packet=Local(1);packet.sender=456;packet.recipient=123;strcpy_s(packet.name,"Friend");
    auto receive=[&](){return SocialReceive((uint8_t*)&packet,sizeof(packet),&address);};
    assert(!SocialReceive((uint8_t*)&packet,sizeof(packet)-1,&address));
    packet.version=2;assert(!receive());packet.version=1;
    packet.recipient=999;assert(!receive());packet.recipient=123;
    ((sockaddr_in*)&address)->sin_port=htons(39002);assert(!receive());((sockaddr_in*)&address)->sin_port=htons(39001);
    InetPtonA(AF_INET,"100.64.0.3",&((sockaddr_in*)&address)->sin_addr);assert(!receive());memcpy(&address,&src,sizeof(src));
    assert(receive());assert(SocialPeerAddresses().size()==1);assert(SocialIsFriend(456));assert(!SocialIsFriend(999));assert(SocialFriends().size()==1);
    packet.sender=789;assert(!receive());packet.sender=456;
    memset(packet.name,'a',sizeof(packet.name));assert(!receive());strcpy_s(packet.name,"Friend");
    peers[0].seen=GetTickCount64()-16000;assert(SocialFriends()[0].dwFriendState==0);
    assert(receive());
    XUID id=456;assert(SocialInvite(0,1,&id)==ERROR_NOT_READY);
    LIVE_SESSION session;session.sessionFlags=XSESSION_CREATE_HOST|XSESSION_CREATE_USES_PRESENCE;session.slotsPublicMaxCount=2;session.slotsPublicFilledCount=1;memset(&session.xnkid,7,sizeof(session.xnkid));
    LIVE_SESSION_XSESSION state;state.liveSession=&session;xlive_xsession_local_sessions[(HANDLE)1]=&state;
    assert(SocialInvite(0,1,&id)==ERROR_SUCCESS);assert(sent.back().kind==2);assert(peers[0].outgoing!=0);
    packet.kind=3;packet.invite=peers[0].outgoing;assert(receive());assert(peers[0].acknowledged);
    packet.kind=5;assert(receive());assert(!peers[0].outgoing);
    packet.kind=2;packet.invite=99;packet.state|=XONLINE_FRIENDSTATE_FLAG_JOINABLE;packet.session=session.xnkid;
    assert(receive());assert(peers[0].incoming==99);auto expiry=peers[0].incomingUntil;assert(sent.back().kind==3);
    assert(receive());assert(peers[0].incomingUntil==expiry);
    peers[0].incoming=0;assert(receive());assert(!peers[0].incoming); // duplicate cannot resurrect declined invite
    packet.invite=100;assert(receive());assert(peers[0].incoming==100);
    peers[0].incomingUntil=GetTickCount64()-1;assert(!(SocialFriends()[0].dwFriendState & XONLINE_FRIENDSTATE_FLAG_RECEIVEDINVITE));
    session.slotsPublicFilledCount=2;assert(SocialInvite(0,1,&id)==ERROR_NOT_READY);
    session.slotsPublicFilledCount=1;session.sessionFlags|=XSESSION_CREATE_INVITES_DISABLED;assert(SocialInvite(0,1,&id)==ERROR_NOT_READY);
    auto snapshot=SocialFriends();snapshot.push_back(snapshot[0]);
    XONLINE_FRIEND row={};size_t count=123;
    assert(SocialReadSnapshot(snapshot,&row,sizeof(row)-1,&count)==ERROR_INSUFFICIENT_BUFFER && snapshot.size()==2 && count==0);
    assert(SocialReadSnapshot(snapshot,&row,sizeof(row),&count)==ERROR_SUCCESS && count==1 && snapshot.size()==1 && row.xuid==456);
    assert(SocialReadSnapshot(snapshot,&row,sizeof(row),&count)==ERROR_SUCCESS && snapshot.empty());
    assert(SocialReadSnapshot(snapshot,&row,sizeof(row),&count)==ERROR_NO_MORE_FILES && count==0);
    assert(Save());peers.clear();loaded=false;Load();
    assert(peers.size()==1 && peers[0].xuid==456 && peers[0].ip=="100.64.0.2");
    assert(SocialFriends()[0].dwFriendState==0);
    assert(GetPrivateProfileIntW(L"Friends",L"Count",0,testPath)==1);
    // The in-game controls must reach the same peer/session backend.
    assert(SocialPeers().size()==1 && !SocialPeers()[0].online);
    assert(SocialAddPeer("not-an-address","Bad")==ERROR_INVALID_PARAMETER);
    assert(SocialAddPeer("100.64.0.2","Duplicate")==ERROR_ALREADY_EXISTS);
    assert(SocialAddPeer("100.64.0.3","Second")==ERROR_SUCCESS);
    assert(SocialPeers().size()==2 && SocialPeers()[1].xuid==0);
    assert(SocialRemovePeer("100.64.0.3")==ERROR_SUCCESS);
    assert(SocialJoinPeer(0,"100.64.0.2",false)==ERROR_NOT_READY && joins==0);
    packet.kind=1;packet.state|=XONLINE_FRIENDSTATE_FLAG_JOINABLE;assert(receive());
    assert(SocialJoinPeer(0,"100.64.0.2",false)==ERROR_SUCCESS && joins==1);
    assert(!memcmp(&joinedSession,&packet.session,sizeof(XNKID)));
    packet.kind=2;packet.invite=101;assert(receive());
    assert(SocialPeers()[0].incoming);
    assert(SocialJoinPeer(0,"100.64.0.2",true)==ERROR_SUCCESS && joins==2);
    assert(sent.back().kind==4 && !SocialPeers()[0].incoming);
    assert(SocialJoinPeer(0,"100.64.0.2",true)==ERROR_NOT_READY && joins==2);
    packet.invite=102;assert(receive());assert(SocialDeclinePeer("100.64.0.2")==ERROR_SUCCESS);
    assert(sent.back().kind==5 && !SocialPeers()[0].incoming);
    assert(receive() && !SocialPeers()[0].incoming); // Duplicates cannot revive a declined invite.
    loaded=true;SocialSetReady(false);assert(!receive());assert(SocialPeerAddresses().empty());assert(SocialInvite(0,1,&id)==ERROR_NOT_READY);
    assert(SocialAddPeer("100.64.0.3","Second")==ERROR_NOT_READY);
    DeleteFileW(testPath);
    puts("PASS: trusted peers, identity pinning, malformed packets, presence expiry, invitations, deduplication, expiry, full/disabled sessions, persistence and shutdown guards");
}
