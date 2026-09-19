// Two independent peers run the production protocol with separate persisted files.
// Delivery is explicit so packet loss, reordering and restarts are reproducible.
#include "../xlivelessness/xlln/social.cpp"
#include <cassert>
#include <cstdio>
#include <crtdbg.h>
HINSTANCE xlln_hModule=nullptr;
HWND xlln_window_hwnd=nullptr;
CRITICAL_SECTION xlive_critsec_local_user,xlive_critsec_xsession;
LOCAL_USER_INFO xlive_local_users[XLIVE_LOCAL_USER_COUNT];
std::map<HANDLE,LIVE_SESSION_XSESSION*> xlive_xsession_local_sessions;
uint32_t xlive_title_id=0x4d53090a;
uint16_t xlln_network_instance_port=39001;
struct Frame {Packet packet;std::string from,to;};
std::vector<Frame> wire;
std::string activeIp;
int notifications=0,joins=0;
uint32_t WINAPI XLLNDebugLogF(uint32_t,const char* const,...) {return 0;}
bool SendPacketToRemoteInstance(XLLN_NET_SEND_PACKET_INFO* out) {
    Frame f={};memcpy(&f.packet,out->data,sizeof(Packet));f.from=activeIp;
    char ip[64];InetNtopA(AF_INET,&((sockaddr_in*)&out->destinationAddress)->sin_addr,ip,64);f.to=ip;
    assert(((sockaddr_in*)&out->destinationAddress)->sin_port==htons(39001));
    wire.push_back(f);delete out;return true;
}
void XLiveNotifyAddEvent(uint32_t,size_t) {}
bool SocialOverlayShow(uint32_t) {return false;}
bool SocialOverlayNotifyInvite(uint32_t) {return true;}
bool SocialOverlayNotifyFriend(uint32_t) {++notifications;return true;}
void XllnDirectIpConnectTo(uint32_t,SOCKADDR_STORAGE*,const char*,const XNKID*) {++joins;}
struct Node {std::string ip,name;XUID id;std::wstring file;std::vector<Peer> saved;};
Node alice={"100.64.0.1","Alice",123,L""},bob={"100.64.0.2","Bob",456,L""};
Node* current=nullptr;
void Switch(Node& node) {
    if(current)current->saved=peers;
    current=&node;peers=node.saved;path=node.file;loaded=ready=true;localUser=0;activeIp=node.ip;
    auto& u=xlive_local_users[0];u.xuid=node.id;u.online_enabled=true;u.signin_state=eXUserSigninState_SignedInToLive;strcpy_s(u.username,node.name.c_str());
}
void Restart() {peers.clear();loaded=false;Load();}
void Tick() {lastTick=0;SocialTick();}
Frame Take(uint16_t kind) {
    auto it=std::find_if(wire.begin(),wire.end(),[&](const Frame& f){return f.packet.kind==kind;});assert(it!=wire.end());
    auto f=*it;wire.erase(it);return f;
}
bool Deliver(const Frame& f) {
    assert(f.to==alice.ip || f.to==bob.ip);Switch(f.to==alice.ip?alice:bob);
    sockaddr_in src={};src.sin_family=AF_INET;src.sin_port=htons(39001);InetPtonA(AF_INET,f.from.c_str(),&src.sin_addr);
    SOCKADDR_STORAGE storage={};memcpy(&storage,&src,sizeof(src));
    return SocialReceive((const uint8_t*)&f.packet,sizeof(Packet),&storage);
}
void Fresh() {
    current=nullptr;wire.clear();peers.clear();alice.saved.clear();bob.saved.clear();
    DeleteFileW(alice.file.c_str());DeleteFileW(bob.file.c_str());notifications=0;joins=0;Switch(alice);
}
Frame Request() {
    Switch(alice);assert(SocialAddPeer(bob.ip,"")==ERROR_SUCCESS);assert(!SocialIsFriend(bob.id));
    assert(SocialPeerAddresses().empty());assert(SocialJoinPeer(0,bob.ip,false)==ERROR_ACCESS_DENIED);
    return Take(FriendRequest);
}
int main() {
    _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    InitializeCriticalSection(&xlive_critsec_local_user);InitializeCriticalSection(&xlive_critsec_xsession);
    wchar_t absolute[MAX_PATH];GetFullPathNameW(L"request-alice.ini",MAX_PATH,absolute,nullptr);alice.file=absolute;
    GetFullPathNameW(L"request-bob.ini",MAX_PATH,absolute,nullptr);bob.file=absolute;
    Fresh();
    // Lost request and sender restart: retry persists, receiver has never added Alice.
    auto request=Request();Restart();Tick();auto retried=Take(FriendRequest);assert(retried.packet.invite==request.packet.invite);
    assert(Deliver(retried));assert(peers.size()==1 && peers[0].ip==alice.ip && peers[0].relationship==SocialRelationship::Incoming);
    assert(SocialFriends().empty() && SocialPeerAddresses().empty() && notifications==1);
    assert(Deliver(request) && notifications==1); // Duplicate cannot notify twice.
    auto receipt=Take(FriendReceipt);assert(Deliver(receipt));assert(SocialPeers()[0].requestDelivered);
    Switch(bob);Restart();assert(SocialPeers()[0].relationship==SocialRelationship::Incoming);
    assert(SocialRespondFriend(alice.ip,true)==ERROR_SUCCESS && joins==0);
    auto acceptance=Take(FriendAccept);wire.clear();Restart();Tick();auto retriedAccept=Take(FriendAccept);
    assert(retriedAccept.packet.invite==acceptance.packet.invite && Deliver(retriedAccept));
    assert(SocialIsFriend(bob.id));auto acceptReceipt=Take(FriendReceipt);
    // Lost acceptance receipt: duplicates are acknowledged after both peers restart.
    wire.clear();Restart();assert(Deliver(acceptance));acceptReceipt=Take(FriendReceipt);
    assert(Deliver(acceptReceipt));assert(!SocialPeers()[0].confirmingFriendship);Restart();
    assert(SocialIsFriend(alice.id) && SocialPeerAddresses().size()==1);
    wire.clear();Tick();auto bobPresence=Take(1);assert(Deliver(bobPresence));assert(SocialPeers()[0].online);
    wire.clear();Tick();auto alicePresence=Take(1);assert(Deliver(alicePresence));assert(SocialPeers()[0].online);
    // Wrong identity cannot reuse a pinned endpoint; wrong request IDs cannot accept.
    auto forged=request;forged.packet.sender=999;assert(!Deliver(forged));
    Fresh();request=Request();assert(Deliver(request));assert(SocialRespondFriend(alice.ip,false)==ERROR_SUCCESS);
    auto decline=Take(FriendDecline);assert(Deliver(decline));assert(SocialPeers()[0].relationship==SocialRelationship::Declined && !SocialIsFriend(bob.id));
    Switch(bob);Restart();auto beforeNotifications=notifications;assert(Deliver(request));assert(SocialPeers().empty() && notifications==beforeNotifications);
    // A new request can be sent after decline; the old acceptance cannot approve it.
    Switch(alice);assert(SocialAddPeer(bob.ip,"Bob")==ERROR_SUCCESS);auto newer=Take(FriendRequest);
    assert(newer.packet.invite!=request.packet.invite);assert(!Deliver(acceptance));
    Switch(bob);peers[0].lastRequest=0;assert(Deliver(newer));assert(SocialPeers()[0].relationship==SocialRelationship::Incoming);
    // Cancellation delivered before its request must not create an incoming request.
    Fresh();request=Request();assert(SocialRemovePeer(bob.ip)==ERROR_SUCCESS);auto cancel=Take(FriendCancel);
    assert(Deliver(cancel));assert(Deliver(request));assert(SocialPeers().empty() && !notifications);
    assert(Deliver(Take(FriendReceipt)));Restart();assert(SocialPeers().empty() && peers[0].controlAcknowledged);
    // Cancellation races a user accepting; it cannot leave an unintended friendship.
    Fresh();request=Request();assert(Deliver(request));assert(SocialRespondFriend(alice.ip,true)==ERROR_SUCCESS);acceptance=Take(FriendAccept);
    Switch(alice);assert(SocialRemovePeer(bob.ip)==ERROR_SUCCESS);cancel=Take(FriendCancel);
    assert(Deliver(acceptance));assert(!SocialIsFriend(bob.id));assert(Deliver(cancel));assert(!SocialIsFriend(alice.id));
    // Crossed requests converge to one incoming request, still requiring acceptance.
    Fresh();request=Request();Switch(bob);assert(SocialAddPeer(alice.ip,"Alice")==ERROR_SUCCESS);auto crossed=Take(FriendRequest);
    assert(Deliver(crossed));assert(SocialPeers()[0].relationship==SocialRelationship::Outgoing);
    assert(Deliver(request));assert(SocialPeers()[0].relationship==SocialRelationship::Incoming);
    assert(SocialRespondFriend(alice.ip,true)==ERROR_SUCCESS);assert(Deliver(Take(FriendAccept)));assert(SocialIsFriend(bob.id));
    auto chosen=peers[0].request;Restart();assert(!Deliver(crossed));assert(peers[0].request==chosen && SocialIsFriend(bob.id));
    // Blocking survives restart and suppresses new request IDs; unblock permits consent again.
    Fresh();request=Request();assert(Deliver(request));assert(SocialBlockPeer(alice.ip,true)==ERROR_SUCCESS);Restart();
    request.packet.invite++;assert(!Deliver(request));assert(SocialPeers()[0].relationship==SocialRelationship::Blocked);
    auto alternate=request;alternate.from="100.64.0.99";assert(!Deliver(alternate));
    assert(SocialBlockPeer(alice.ip,false)==ERROR_SUCCESS);peers[0].lastRequest=0;assert(Deliver(request));
    // Failed persistence must not accept or acknowledge an unsaved friendship.
    auto goodPath=path;path=goodPath+L"\\missing\\friends.ini";wire.clear();
    assert(SocialRespondFriend(alice.ip,true)==ERROR_WRITE_FAULT && peers[0].relationship==SocialRelationship::Incoming && wire.empty());path=goodPath;
    // Fresh receiver write failure leaves no transient trusted peer or acknowledgment.
    Fresh();request=Request();Switch(bob);goodPath=bob.file;bob.file+=L"\\missing\\friends.ini";
    assert(!Deliver(request) && peers.empty());bob.file=goodPath;
    // Limit unsolicited outstanding requests; accepted friends are not evicted.
    Fresh();Switch(bob);
    for(size_t i=0;i<MaxIncomingRequests;++i) {Peer p;p.ip="10.0.0."+std::to_string(i+1);p.relationship=SocialRelationship::Incoming;p.request=i+1;peers.push_back(p);}
    bob.saved=peers;auto incoming=request;incoming.packet.invite++;assert(!Deliver(incoming));assert(peers.size()==MaxIncomingRequests);
    // An old friends.ini without relationship fields remains a trusted friend list.
    Fresh();Switch(alice);
    assert(WritePrivateProfileStringW(L"Friends",L"Count",L"1",path.c_str()));
    WritePrivateProfileStringW(L"Friend0",L"IP",L"100.64.0.2",path.c_str());WritePrivateProfileStringW(L"Friend0",L"Name",L"Bob",path.c_str());
    WritePrivateProfileStringW(L"Friend0",L"XUID",L"00000000000001c8",path.c_str());Restart();assert(SocialIsFriend(bob.id));
    assert(Save());Restart();assert(SocialIsFriend(bob.id));
    DeleteFileW(alice.file.c_str());DeleteFileW(bob.file.c_str());
    puts("PASS: one-sided requests, acceptance, decline, cancellation, packet loss/reordering, restart, duplicate suppression, identity pinning, crossed requests, block/unblock, persistence failures, request limits, legacy migration");
}
