#pragma once
#include "../xlive/xdefs.hpp"
#include <vector>
#include <string>
enum class SocialRelationship : uint32_t { Friend = 0, Outgoing = 1, Incoming = 2, Declined = 3, Removed = 4, Blocked = 5 };
struct SocialPeerView {
    std::string ip, name;
    XUID xuid = 0;
    bool online = false, joinable = false, incoming = false;
    bool outgoing = false, delivered = false;
    SocialRelationship relationship = SocialRelationship::Friend;
    bool requestDelivered = false, confirmingFriendship = false;
};
std::vector<SocialPeerView> SocialPeers();
uint32_t SocialAddPeer(const std::string& ip, const std::string& name);
uint32_t SocialRemovePeer(const std::string& ip);
uint32_t SocialRespondFriend(const std::string& ip, bool accept);
uint32_t SocialBlockPeer(const std::string& ip, bool block);
uint32_t SocialJoinPeer(uint32_t user, const std::string& ip, bool acceptInvite);
uint32_t SocialDeclinePeer(const std::string& ip);
void SocialTick();
void SocialShow(uint32_t user);
bool SocialReceive(const uint8_t* data, size_t size, const SOCKADDR_STORAGE* source);
std::vector<XONLINE_FRIEND> SocialFriends();
bool SocialIsFriend(XUID xuid);
uint32_t SocialInvite(uint32_t user, size_t count, const XUID* recipients);

void SocialOpenOnUiThread(uint32_t user, bool activate = true);

void SocialSetReady(bool ready);

uint32_t SocialReadSnapshot(std::vector<XONLINE_FRIEND>& snapshot, void* buffer, size_t bytes, size_t* count);

std::vector<SOCKADDR_STORAGE> SocialPeerAddresses();
