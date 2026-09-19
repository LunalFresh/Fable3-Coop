#pragma once
#include <stdint.h>

#pragma pack(push, 8) // Save then set byte alignment setting.

#define DASHBOARD_TITLE_ID 0xFFFE07D1

typedef uint64_t XUID;
typedef XUID* PXUID;

#define INVALID_XUID                    ((XUID)0)
#define XUID_LIVE_ENABLED_FLAG          ((XUID)0x0009000000000000)
#define XUID_OFFLINE_FLAG               ((XUID)0xE000000000000000)
#define XUID_LIVE_GUEST_BIT             ((XUID)0x0040000000000000)

#define IsGuestXUID(xuid)               (((xuid) & 0x00C0000000000000) > 0)
#define GetXUIDGuestNumber(xuid)        ((uint8_t)(((xuid) >> (22 + 32)) & 3))
#define IsOfflineXUID(xuid)             (((xuid) & 0xF000000000000000) == XUID_OFFLINE_FLAG)
#define IsOnlineXUID(xuid)              (((xuid) & 0xFFFF000000000000) == XUID_LIVE_ENABLED_FLAG)
#define IsLiveEnabledXUID(xuid)         (((xuid) & 0x000F000000000000) == XUID_LIVE_ENABLED_FLAG)

#define BuildXUID(id, live_enabled, online, guest_number) (((live_enabled) ? XUID_LIVE_ENABLED_FLAG : 0) + ((online) ? 0 : XUID_OFFLINE_FLAG) + ((guest_number) * XUID_LIVE_GUEST_BIT) + (XUID)(id))

#define IsEqualXUID(xuid1, xuid2) (xuid1 == xuid2)
#define XOnlineAreUsersIdentical IsEqualXUID

// Official values.
#define XUSER_MAX_COUNT                 4
#define XUSER_INDEX_FOCUS               0x000000FD
#define XUSER_INDEX_NONE                0x000000FE
#define XUSER_INDEX_ANY                 0x000000FF

// XLLN version.
#define XLIVE_LOCAL_USER_COUNT XUSER_MAX_COUNT
#define XLIVE_LOCAL_USER_INVALID XUSER_INDEX_NONE

#ifndef LODWORD
	#define LODWORD(ll) ((uint32_t)(ll))
#endif
#ifndef HIDWORD
	#define HIDWORD(ll) ((uint32_t)(((uint64_t)(ll) >> 32) & 0xFFFFFFFF))
#endif
//#define LOWORD(l) ((uint16_t)(l))
//#define HIWORD(l) ((uint16_t)(((uint32_t)(l) >> 16) & 0xFFFF))
//#define LOBYTE(w) ((uint8_t)(w))
//#define HIBYTE(w) ((uint8_t)(((uint16_t)(w) >> 8) & 0xFF))

#define E_DEBUGGER_PRESENT 0x80040317;

//
// This devkit-only flag tells the XNet stack to allow insecure
// communication to untrusted hosts (such as a PC).  This flag is silently
// ignored by the secure versions of the library.
//
#define XNET_STARTUP_BYPASS_SECURITY                0x01

//
// This flag instructs XNet to pre-allocate memory for the maximum number of
// datagram (UDP and VDP) sockets during the 'XNetStartup' call and store the
// objects in an internal pool.  Otherwise, sockets are allocated on demand (by
// the 'socket' function).  In either case, SOCK_DGRAM sockets are returned to
// the internal pool once closed.  The memory will remain allocated until
// XNetCleanup is called.
//
#define XNET_STARTUP_ALLOCATE_MAX_DGRAM_SOCKETS     0x02

//
// This flag instructs XNet to pre-allocate memory for the maximum number of
// stream (TCP) sockets during the 'XNetStartup' call and store the objects in
// an internal pool.  Otherwise, sockets are allocated on demand (by the
// 'socket', 'listen', and 'accept' functions).  Note that 'listen' will still
// attempt to pre-allocate the specified maximum backlog number of sockets even
// without this flag set.  The 'accept' function will always return a socket
// retrieved from the pool, though it will also attempt to allocate a
// replacement if the cfgSockMaxStreamSockets limit and memory permit.
// In all cases, SOCK_STREAM sockets are returned to the internal pool once
// closed. The memory will remain allocated until XNetCleanup is called.
//
#define XNET_STARTUP_ALLOCATE_MAX_STREAM_SOCKETS    0x04

//
// This devkit-only flag tells the XNet stack to disable encryption for
// communication between peers.  This flag is silently ignored by the secure
// versions of the library.
//
#define XNET_STARTUP_DISABLE_PEER_ENCRYPTION        0x08

#define XLIVE_PROTECTED_DATA_FLAG_OFFLINE_ONLY 0x00000001

// The following protocol can be passed to the socket() API to create a datagram socket that
// transports encrypted data and unencrypted voice in the same packet.
#define IPPROTO_VDP 254


// Creation Flags
#define XSESSION_CREATE_USES_MASK                       0x0000003F

#define XSESSION_CREATE_HOST                            0x00000001  // Set only on the host of a multiplayer session. The user who sets the host flag is the user that interacts with Live
#define XSESSION_CREATE_USES_PRESENCE                   0x00000002  // Session is used across games to keep players together. Advertises state via Presence
#define XSESSION_CREATE_USES_STATS                      0x00000004  // Session is used for stats tracking
#define XSESSION_CREATE_USES_MATCHMAKING                0x00000008  // Session is advertised in matchmaking for searching
#define XSESSION_CREATE_USES_ARBITRATION                0x00000010  // Session stats are arbitrated (and therefore tracked for everyone in the game)
#define XSESSION_CREATE_USES_PEER_NETWORK               0x00000020  // Session XNKey is registered and PC settings are enforced

// Optional modifiers to sessions that are created with XSESSION_CREATE_USES_PRESENCE
#define XSESSION_CREATE_MODIFIERS_MASK                  0x00000F80

#define XSESSION_CREATE_SOCIAL_MATCHMAKING_ALLOWED      0x00000080  // Session may be converted to an Social Matchmaking session
#define XSESSION_CREATE_INVITES_DISABLED                0x00000100  // Game Invites cannot be sent by the HUD for this session
#define XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED      0x00000200  // Session will not ever be displayed as joinable via Presence
#define XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED       0x00000400  // Session will not be joinable between XSessionStart and XSessionEnd
#define XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY  0x00000800  // Session is only joinable via presence for friends of the host


#define XSESSION_CREATE_SINGLEPLAYER_WITH_STATS		XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_STATS | XSESSION_CREATE_INVITES_DISABLED | XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED | XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED
#define XSESSION_CREATE_LIVE_MULTIPLAYER_STANDARD	XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_STATS | XSESSION_CREATE_USES_MATCHMAKING | XSESSION_CREATE_USES_PEER_NETWORK
#define XSESSION_CREATE_LIVE_MULTIPLAYER_RANKED		XSESSION_CREATE_LIVE_MULTIPLAYER_STANDARD | XSESSION_CREATE_USES_ARBITRATION
#define XSESSION_CREATE_SYSTEMLINK					XSESSION_CREATE_USES_PEER_NETWORK
#define XSESSION_CREATE_GROUP_LOBBY					XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_PEER_NETWORK
#define XSESSION_CREATE_GROUP_GAME					XSESSION_CREATE_USES_STATS | XSESSION_CREATE_USES_MATCHMAKING | XSESSION_CREATE_USES_PEER_NETWORK


#define XSESSION_SEARCH_MAX_PARAMS                      30  // We only allow the client to have 30 params on a query
#define XSESSION_SEARCH_MAX_RETURNS                     50  // We only allow the client to have 50 columns on a search result

// Generic errors
#define XONLINE_E_OVERFLOW                              _HRESULT_TYPEDEF_(0x80150001L)
#define XONLINE_E_NO_SESSION                            _HRESULT_TYPEDEF_(0x80150002L)
#define XONLINE_E_USER_NOT_LOGGED_ON                    _HRESULT_TYPEDEF_(0x80150003L)
#define XONLINE_E_NO_GUEST_ACCESS                       _HRESULT_TYPEDEF_(0x80150004L)
#define XONLINE_E_NOT_INITIALIZED                       _HRESULT_TYPEDEF_(0x80150005L)
#define XONLINE_E_NO_USER                               _HRESULT_TYPEDEF_(0x80150006L)
#define XONLINE_E_INTERNAL_ERROR                        _HRESULT_TYPEDEF_(0x80150007L)
#define XONLINE_E_OUT_OF_MEMORY                         _HRESULT_TYPEDEF_(0x80150008L)
#define XONLINE_E_TASK_BUSY                             _HRESULT_TYPEDEF_(0x80150009L)
#define XONLINE_E_SERVER_ERROR                          _HRESULT_TYPEDEF_(0x8015000AL)
#define XONLINE_E_IO_ERROR                              _HRESULT_TYPEDEF_(0x8015000BL)
#define XONLINE_E_USER_NOT_PRESENT                      _HRESULT_TYPEDEF_(0x8015000DL)
#define XONLINE_E_INVALID_REQUEST                       _HRESULT_TYPEDEF_(0x80150010L)
#define XONLINE_E_TASK_THROTTLED                        _HRESULT_TYPEDEF_(0x80150011L)
#define XONLINE_E_TASK_ABORTED_BY_DUPLICATE             _HRESULT_TYPEDEF_(0x80150012L)
#define XONLINE_E_INVALID_TITLE_ID                      _HRESULT_TYPEDEF_(0x80150013L)
#define XONLINE_E_SERVER_CONFIG_ERROR                   _HRESULT_TYPEDEF_(0x80150014L)
#define XONLINE_E_END_OF_STREAM                         _HRESULT_TYPEDEF_(0x80150015L)
#define XONLINE_E_ACCESS_DENIED                         _HRESULT_TYPEDEF_(0x80150016L)
#define XONLINE_E_GEO_DENIED                            _HRESULT_TYPEDEF_(0x80150017L)

// Live connection state success codes
#define XONLINE_S_LOGON_CONNECTION_ESTABLISHED          _HRESULT_TYPEDEF_(0x001510F0L)
#define XONLINE_S_LOGON_DISCONNECTED                    _HRESULT_TYPEDEF_(0x001510F1L)

// Live connection state failure codes
#define XONLINE_E_LOGON_NO_NETWORK_CONNECTION           _HRESULT_TYPEDEF_(0x80151000L)
#define XONLINE_E_LOGON_CANNOT_ACCESS_SERVICE           _HRESULT_TYPEDEF_(0x80151001L)
#define XONLINE_E_LOGON_UPDATE_REQUIRED                 _HRESULT_TYPEDEF_(0x80151002L)
#define XONLINE_E_LOGON_SERVERS_TOO_BUSY                _HRESULT_TYPEDEF_(0x80151003L)
#define XONLINE_E_LOGON_CONNECTION_LOST                 _HRESULT_TYPEDEF_(0x80151004L)
#define XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON       _HRESULT_TYPEDEF_(0x80151005L)
#define XONLINE_E_LOGON_INVALID_USER                    _HRESULT_TYPEDEF_(0x80151006L)
#define XONLINE_E_LOGON_FLASH_UPDATE_REQUIRED           _HRESULT_TYPEDEF_(0x80151007L)
#define XONLINE_E_LOGON_SERVICE_NOT_REQUESTED           _HRESULT_TYPEDEF_(0x80151100L)
#define XONLINE_E_LOGON_SERVICE_NOT_AUTHORIZED          _HRESULT_TYPEDEF_(0x80151101L)
#define XONLINE_E_LOGON_SERVICE_TEMPORARILY_UNAVAILABLE _HRESULT_TYPEDEF_(0x80151102L)

//  Errors returned by matchmaking                      = 0x801551XX
#define XONLINE_E_MATCH_INVALID_SESSION_ID              _HRESULT_TYPEDEF_(0x80155100L)  // specified session id does not exist
#define XONLINE_E_MATCH_INVALID_TITLE_ID                _HRESULT_TYPEDEF_(0x80155101L)  // specified title id is zero, or does not exist
#define XONLINE_E_MATCH_INVALID_DATA_TYPE               _HRESULT_TYPEDEF_(0x80155102L)  // attribute ID or parameter type specifies an invalid data type
#define XONLINE_E_MATCH_REQUEST_TOO_SMALL               _HRESULT_TYPEDEF_(0x80155103L)  // the request did not meet the minimum length for a valid request
#define XONLINE_E_MATCH_REQUEST_TRUNCATED               _HRESULT_TYPEDEF_(0x80155104L)  // the self described length is greater than the actual buffer size
#define XONLINE_E_MATCH_INVALID_SEARCH_REQ              _HRESULT_TYPEDEF_(0x80155105L)  // the search request was invalid
#define XONLINE_E_MATCH_INVALID_OFFSET                  _HRESULT_TYPEDEF_(0x80155106L)  // one of the attribute/parameter offsets in the request was invalid.  Will be followed by the zero based offset number.
#define XONLINE_E_MATCH_INVALID_ATTR_TYPE               _HRESULT_TYPEDEF_(0x80155107L)  // the attribute type was something other than user or session
#define XONLINE_E_MATCH_INVALID_VERSION                 _HRESULT_TYPEDEF_(0x80155108L)  // bad protocol version in request
#define XONLINE_E_MATCH_OVERFLOW                        _HRESULT_TYPEDEF_(0x80155109L)  // an attribute or parameter flowed past the end of the request
#define XONLINE_E_MATCH_INVALID_RESULT_COL              _HRESULT_TYPEDEF_(0x8015510AL)  // referenced stored procedure returned a column with an unsupported data type
#define XONLINE_E_MATCH_INVALID_STRING                  _HRESULT_TYPEDEF_(0x8015510BL)  // string with length-prefix of zero, or string with no terminating null
#define XONLINE_E_MATCH_STRING_TOO_LONG                 _HRESULT_TYPEDEF_(0x8015510CL)  // string exceeded 400 characters
#define XONLINE_E_MATCH_BLOB_TOO_LONG                   _HRESULT_TYPEDEF_(0x8015510DL)  // blob exceeded 800 bytes
#define XONLINE_E_MATCH_INVALID_ATTRIBUTE_ID            _HRESULT_TYPEDEF_(0x80155110L)  // attribute id is invalid
#define XONLINE_E_MATCH_SESSION_ALREADY_EXISTS          _HRESULT_TYPEDEF_(0x80155112L)  // session id already exists in the db
#define XONLINE_E_MATCH_CRITICAL_DB_ERR                 _HRESULT_TYPEDEF_(0x80155115L)  // critical error in db
#define XONLINE_E_MATCH_NOT_ENOUGH_COLUMNS              _HRESULT_TYPEDEF_(0x80155116L)  // search result set had too few columns
#define XONLINE_E_MATCH_PERMISSION_DENIED               _HRESULT_TYPEDEF_(0x80155117L)  // incorrect permissions set on search sp
#define XONLINE_E_MATCH_INVALID_PART_SCHEME             _HRESULT_TYPEDEF_(0x80155118L)  // title specified an invalid partitioning scheme
#define XONLINE_E_MATCH_INVALID_PARAM                   _HRESULT_TYPEDEF_(0x80155119L)  // bad parameter passed to sp
#define XONLINE_E_MATCH_DATA_TYPE_MISMATCH              _HRESULT_TYPEDEF_(0x8015511DL)  // data type specified in attr id did not match type of attr being set
#define XONLINE_E_MATCH_SERVER_ERROR                    _HRESULT_TYPEDEF_(0x8015511EL)  // error on server not correctable by client
#define XONLINE_E_MATCH_NO_USERS                        _HRESULT_TYPEDEF_(0x8015511FL)  // no authenticated users in search request.
#define XONLINE_E_MATCH_INVALID_BLOB                    _HRESULT_TYPEDEF_(0x80155120L)  // invalid blob attribute
#define XONLINE_E_MATCH_TOO_MANY_USERS                  _HRESULT_TYPEDEF_(0x80155121L)  // too many users in search request
#define XONLINE_E_MATCH_INVALID_FLAGS                   _HRESULT_TYPEDEF_(0x80155122L)  // invalid flags were specified in a search request
#define XONLINE_E_MATCH_PARAM_MISSING                   _HRESULT_TYPEDEF_(0x80155123L)  // required parameter not passed to sp
#define XONLINE_E_MATCH_TOO_MANY_PARAM                  _HRESULT_TYPEDEF_(0x80155124L)  // too many paramters passed to sp or in request structure
#define XONLINE_E_MATCH_DUPLICATE_PARAM                 _HRESULT_TYPEDEF_(0x80155125L)  // a paramter was passed to twice to a search procedure
#define XONLINE_E_MATCH_TOO_MANY_ATTR                   _HRESULT_TYPEDEF_(0x80155126L)  // too many attributes in the request structure

// Errors returned by Messages                          = 0x80155AXX
#define XONLINE_E_MESSAGE_INVALID_MESSAGE_ID            _HRESULT_TYPEDEF_(0x80155A01L)  // the specified message was not found
#define XONLINE_E_MESSAGE_PROPERTY_DOWNLOAD_REQUIRED    _HRESULT_TYPEDEF_(0x80155A02L)  // the property was too large to fit into the details block, it must be retrieved separately using XOnlineMessageDownloadAttachmentxxx
#define XONLINE_E_MESSAGE_PROPERTY_NOT_FOUND            _HRESULT_TYPEDEF_(0x80155A03L)  // the specified property tag was not found
#define XONLINE_E_MESSAGE_NO_VALID_SENDS_TO_REVOKE      _HRESULT_TYPEDEF_(0x80155A04L)  // no valid sends to revoke were found
#define XONLINE_E_MESSAGE_NO_MESSAGE_DETAILS            _HRESULT_TYPEDEF_(0x80155A05L)  // the specified message does not have any details
#define XONLINE_E_MESSAGE_INVALID_TITLE_ID              _HRESULT_TYPEDEF_(0x80155A06L)  // an invalid title ID was specified
#define XONLINE_E_MESSAGE_SENDER_BLOCKED                _HRESULT_TYPEDEF_(0x80155A07L)  // a send failed because the recipient has blocked the sender
#define XONLINE_E_MESSAGE_MAX_DETAILS_SIZE_EXCEEDED     _HRESULT_TYPEDEF_(0x80155A08L)  // the property couldn't be added because the maximum details size would be exceeded
#define XONLINE_E_MESSAGE_INVALID_MESSAGE_TYPE          _HRESULT_TYPEDEF_(0x80155A09L)
#define XONLINE_E_MESSAGE_USER_OPTED_OUT                _HRESULT_TYPEDEF_(0x80155A0AL)  // a send failed because the message is marketing and the recipient has opted-out for the sending title
// Success codes returned by Messages                   = 0x00155AXX
#define XONLINE_S_MESSAGE_PENDING_SYNC                  _HRESULT_TYPEDEF_(0x00155A01L)  // updated message list is currently being retrieved (after logon or disabling summary refresh), returned results may be out of date

// Errors returned by Marketplace offer APIs            = 0x801530XX + 0x801531XX
#define XONLINE_E_OFFERING_INVALID_CONSUME_ITEMS        _HRESULT_TYPEDEF_(0x80153013L)

//  Errors returned by session APIs                     = 0x801552XX
#define XONLINE_E_SESSION_NOT_FOUND                     _HRESULT_TYPEDEF_(0x80155200L)  // specified session id does not exist
#define XONLINE_E_SESSION_INSUFFICIENT_PRIVILEGES       _HRESULT_TYPEDEF_(0x80155201L)  // The requester does not have permissions to perform this operation
#define XONLINE_E_SESSION_FULL                          _HRESULT_TYPEDEF_(0x80155202L)  // The session is full, and the join operation failed, or joining is disallowe
#define XONLINE_E_SESSION_INVITES_DISABLED              _HRESULT_TYPEDEF_(0x80155203L)  // Invites have been disabled for this session
#define XONLINE_E_SESSION_INVALID_FLAGS                 _HRESULT_TYPEDEF_(0x80155204L)  // Invalid flags passed to XSessionCreate
#define XONLINE_E_SESSION_REQUIRES_ARBITRATION          _HRESULT_TYPEDEF_(0x80155205L)  // The Session owner has the GAME_TYPE context set to ranked, but the session is not arbitrated
#define XONLINE_E_SESSION_WRONG_STATE                   _HRESULT_TYPEDEF_(0x80155206L)  // The session is in the wrong state for the requested operation to be performed
#define XONLINE_E_SESSION_INSUFFICIENT_BUFFER           _HRESULT_TYPEDEF_(0x80155207L)  // Ran out of memory attempting to process search results
#define XONLINE_E_SESSION_REGISTRATION_ERROR            _HRESULT_TYPEDEF_(0x80155208L)  // Could not perform arbitration registration because some logged on users have not joined the session
#define XONLINE_E_SESSION_NOT_LOGGED_ON                 _HRESULT_TYPEDEF_(0x80155209L)  // User is not logged on to Live but attempted to create a session using Live features.
#define XONLINE_E_SESSION_JOIN_ILLEGAL                  _HRESULT_TYPEDEF_(0x8015520AL)  // User attempted to join a USES_PRESENCE session when the user is already in a USES_PRESENCE session on the console. Can only have 1 at a time.
#define XONLINE_E_SESSION_CREATE_KEY_FAILED             _HRESULT_TYPEDEF_(0x8015520BL)  // Key creation failed during session creation.
#define XONLINE_E_SESSION_NOT_REGISTERED                _HRESULT_TYPEDEF_(0x8015520CL)  // Can not start the session if registration has not completed
#define XONLINE_E_SESSION_REGISTER_KEY_FAILED           _HRESULT_TYPEDEF_(0x8015520DL)  // Key registration failed during session creation.
#define XONLINE_E_SESSION_UNREGISTER_KEY_FAILED         _HRESULT_TYPEDEF_(0x8015520EL)  // Key registration failed during session creation.

// Errors returned by Query service                     = 0x801561XX
#define XONLINE_E_QUERY_QUOTA_FULL                      _HRESULT_TYPEDEF_(0x80156101L)  // this user or team's quota for the dataset is full.  you must remove an entity first.
#define XONLINE_E_QUERY_ENTITY_NOT_FOUND                _HRESULT_TYPEDEF_(0x80156102L)  // the requested entity didn't exist in the provided dataset.
#define XONLINE_E_QUERY_PERMISSION_DENIED               _HRESULT_TYPEDEF_(0x80156103L)  // the user tried to update or delete an entity that he didn't own.
#define XONLINE_E_QUERY_ATTRIBUTE_TOO_LONG              _HRESULT_TYPEDEF_(0x80156104L)  // attribute passed exceeds schema definition
#define XONLINE_E_QUERY_UNEXPECTED_ATTRIBUTE            _HRESULT_TYPEDEF_(0x80156105L)  // attribute passed was a bad param for the database operation
#define XONLINE_E_QUERY_INVALID_ACTION                  _HRESULT_TYPEDEF_(0x80156107L)  // the specified action (or dataset) doesn't have a select action associated with it.
#define XONLINE_E_QUERY_SPEC_COUNT_MISMATCH             _HRESULT_TYPEDEF_(0x80156108L)  // the provided number of QUERY_ATTRIBUTE_SPECs doesn't match the number returned by the procedure
#define XONLINE_E_QUERY_DATASET_NOT_FOUND               _HRESULT_TYPEDEF_(0x80156109L)  // The specified dataset id was not found.
#define XONLINE_E_QUERY_PROCEDURE_NOT_FOUND             _HRESULT_TYPEDEF_(0x8015610AL)  // The specified proc index was not found.

// Errors returned by Competitions service              = 0x801562XX
#define XONLINE_E_COMP_ACCESS_DENIED                    _HRESULT_TYPEDEF_(0x80156202L)  // The specified source (client) is not permitted to execute this method
#define XONLINE_E_COMP_REGISTRATION_CLOSED              _HRESULT_TYPEDEF_(0x80156203L)  // The competition is closed to registration
#define XONLINE_E_COMP_FULL                             _HRESULT_TYPEDEF_(0x80156204L)  // The competition has reached it's max enrollment
#define XONLINE_E_COMP_NOT_REGISTERED                   _HRESULT_TYPEDEF_(0x80156205L)  // The user or team isn't registered for the competition
#define XONLINE_E_COMP_CANCELLED                        _HRESULT_TYPEDEF_(0x80156206L)  // The competition has been cancelled, and the operation is invalid.
#define XONLINE_E_COMP_CHECKIN_TIME_INVALID             _HRESULT_TYPEDEF_(0x80156207L)  // The user is attempting to checkin to an event outside the allowed time.
#define XONLINE_E_COMP_CHECKIN_BAD_EVENT                _HRESULT_TYPEDEF_(0x80156208L)  // The user is attempting to checkin to an event in which they are not a valid participant.
#define XONLINE_E_COMP_EVENT_SCORED                     _HRESULT_TYPEDEF_(0x80156209L)  // The user is attempting to checkin to an event which has already been scored by the service (user has forfeited or been ejected)
#define XONLINE_S_COMP_EVENT_SCORED                     _HRESULT_TYPEDEF_(0x00156209L)  // The user is attempting to checkin to an event but the users event has been updated. Re-query for a new event
#define XONLINE_E_COMP_UNEXPECTED                       _HRESULT_TYPEDEF_(0x80156210L)  // Results from the Database are unexpected or inconsistent with the current operation.
#define XONLINE_E_COMP_TOPOLOGY_ERROR                   _HRESULT_TYPEDEF_(0x80156216L)  // The topology request cannot be fulfilled by the server
#define XONLINE_E_COMP_TOPOLOGY_PENDING                 _HRESULT_TYPEDEF_(0x80156217L)  // The topology request has not completed yet
#define XONLINE_E_COMP_CHECKIN_TOO_EARLY                _HRESULT_TYPEDEF_(0x80156218L)  // The user is attempting to checkin to an event before the allowed time.
#define XONLINE_E_COMP_ALREADY_REGISTERED               _HRESULT_TYPEDEF_(0x80156219L)  // The user has already registered for this competition.
#define XONLINE_E_COMP_INVALID_ENTRANT_TYPE             _HRESULT_TYPEDEF_(0x8015621AL)  // dwTeamId was non-0 for a user competition, or dwTeamId was 0 for a team competition
#define XONLINE_E_COMP_TOO_LATE                         _HRESULT_TYPEDEF_(0x8015621BL)  // The time allotted for performing the requested action has already passed
#define XONLINE_E_COMP_TOO_EARLY                        _HRESULT_TYPEDEF_(0x8015621CL)  // The specified action cannot yet be performed
#define XONLINE_E_COMP_NO_BYES_AVAILABLE                _HRESULT_TYPEDEF_(0x8015621DL)  // No bytes remain to be granted
#define XONLINE_E_COMP_SERVICE_OUTAGE                   _HRESULT_TYPEDEF_(0x8015621EL)  // A service outage has occurred, try again in a bit

// Errors returned by the v1 Message Service            = 0x801570XX
#define XONLINE_E_MSGSVR_INVALID_REQUEST                _HRESULT_TYPEDEF_(0x80157001L)  // an invalid request type was received

// Errors returned by the String Service                = 0x801571XX
#define XONLINE_E_STRING_TOO_LONG                       _HRESULT_TYPEDEF_(0x80157101L)  // the string was longer than the allowed maximum
#define XONLINE_E_STRING_OFFENSIVE_TEXT                 _HRESULT_TYPEDEF_(0x80157102L)  // the string contains offensive text
#define XONLINE_E_STRING_NO_DEFAULT_STRING              _HRESULT_TYPEDEF_(0x80157103L)  // returned by AddString when no string of the language specified as the default is found
#define XONLINE_E_STRING_INVALID_LANGUAGE               _HRESULT_TYPEDEF_(0x80157104L)  // returned by AddString when an invalid language is specified for a string

// Errors returned by the Feedback Service              = 0x801580XX
#define XONLINE_E_FEEDBACK_NULL_TARGET                  _HRESULT_TYPEDEF_(0x80158001L)  // target PUID of feedback is NULL
#define XONLINE_E_FEEDBACK_BAD_TYPE                     _HRESULT_TYPEDEF_(0x80158002L)  // bad feedback type
#define XONLINE_E_FEEDBACK_CANNOT_LOG                   _HRESULT_TYPEDEF_(0x80158006L)  // cannot write to feedback log

// Errors returned by the Statistics Service            = 0x80159XXX
#define XONLINE_E_STAT_BAD_REQUEST                      _HRESULT_TYPEDEF_(0x80159001L)  // server received incorrectly formatted request.
#define XONLINE_E_STAT_INVALID_TITLE_OR_LEADERBOARD     _HRESULT_TYPEDEF_(0x80159002L)  // title or leaderboard id were not recognized by the server.
#define XONLINE_E_STAT_TOO_MANY_SPECS                   _HRESULT_TYPEDEF_(0x80159004L)  // too many stat specs in a request.
#define XONLINE_E_STAT_TOO_MANY_STATS                   _HRESULT_TYPEDEF_(0x80159005L)  // too many stats in a spec or already stored for the user.
#define XONLINE_E_STAT_USER_NOT_FOUND                   _HRESULT_TYPEDEF_(0x80159003L)  // user not found.
#define XONLINE_E_STAT_SET_FAILED_0                     _HRESULT_TYPEDEF_(0x80159100L)  // set operation failed on spec index 0
#define XONLINE_E_STAT_PERMISSION_DENIED                _HRESULT_TYPEDEF_(0x80159200L)  // operation failed because of credentials. UserId is not logged in or this operation is not supported in production (e.g. userId=0 in XOnlineStatReset)
#define XONLINE_E_STAT_LEADERBOARD_WAS_RESET            _HRESULT_TYPEDEF_(0x80159201L)  // operation failed because user was logged on before the leaderboard was reset.
#define XONLINE_E_STAT_INVALID_ATTACHMENT               _HRESULT_TYPEDEF_(0x80159202L)  // attachment is invalid.
#define XONLINE_S_STAT_CAN_UPLOAD_ATTACHMENT            _HRESULT_TYPEDEF_(0x00159203L)  // Use XOnlineStatWriteGetResults to get a handle to upload a attachment.
#define XONLINE_E_STAT_TOO_MANY_PARAMETERS              _HRESULT_TYPEDEF_(0x80159204L)
#define XONLINE_E_STAT_TOO_MANY_PROCEDURES              _HRESULT_TYPEDEF_(0x80159205L)
#define XONLINE_E_STAT_STAT_POST_PROC_ERROR             _HRESULT_TYPEDEF_(0x80159206L)
#define XONLINE_E_STAT_NOT_ENOUGH_PARAMETERS            _HRESULT_TYPEDEF_(0x80159208L)
#define XONLINE_E_STAT_INVALID_PROCEDURE                _HRESULT_TYPEDEF_(0x80159209L)
#define XONLINE_E_STAT_EXCEEDED_WRITE_READ_LIMIT        _HRESULT_TYPEDEF_(0x8015920AL)
#define XONLINE_E_STAT_LEADERBOARD_READONLY             _HRESULT_TYPEDEF_(0x8015920BL)
#define XONLINE_E_STAT_MUSIGMA_ARITHMETIC_OVERFLOW      _HRESULT_TYPEDEF_(0x8015920CL)
#define XONLINE_E_STAT_READ_NO_SPEC                     _HRESULT_TYPEDEF_(0x8015920DL)
#define XONLINE_E_STAT_MUSIGMA_NO_GAME_MODE             _HRESULT_TYPEDEF_(0x8015920EL)  // no game mode found for this leaderboard

//  Errors returned by xsuppapi service                 = 0x8015A0XX

// Errors returned by Signature Service                 = 0x8015B0XX
#define XONLINE_E_SIGNATURE_ERROR                       _HRESULT_TYPEDEF_(0x8015B000L)
#define XONLINE_E_SIGNATURE_VER_INVALID_SIGNATURE       _HRESULT_TYPEDEF_(0x8015B001L)  // presented signature does not match
#define XONLINE_E_SIGNATURE_VER_UNKNOWN_KEY_VER         _HRESULT_TYPEDEF_(0x8015B002L)  // signature key version specified is not found among the valid signature keys
#define XONLINE_E_SIGNATURE_VER_UNKNOWN_SIGNATURE_VER   _HRESULT_TYPEDEF_(0x8015B003L)  // signature version is unknown, currently only version 1 is supported
#define XONLINE_E_SIGNATURE_BANNED_XBOX                 _HRESULT_TYPEDEF_(0x8015B004L)  // signature is not calculated or revoked because Xbox is banned
#define XONLINE_E_SIGNATURE_BANNED_USER                 _HRESULT_TYPEDEF_(0x8015B005L)  // signature is not calculated or revoked because at least one user is banned
#define XONLINE_E_SIGNATURE_BANNED_TITLE                _HRESULT_TYPEDEF_(0x8015B006L)  // signature is not calculated or revoked because the given title and version is banned
#define XONLINE_E_SIGNATURE_BANNED_DIGEST               _HRESULT_TYPEDEF_(0x8015B007L)  // signature is not calculated or revoked because the digest is banned
#define XONLINE_E_SIGNATURE_GET_BAD_AUTH_DATA           _HRESULT_TYPEDEF_(0x8015B008L)  // fail to retrieve AuthData from SG, returned by GetSigningKey api
#define XONLINE_E_SIGNATURE_SERVICE_UNAVAILABLE         _HRESULT_TYPEDEF_(0x8015B009L)  // fail to retrieve a signature server master key, returned by GetSigningKey or SignOnBehalf api

// Errors returned by Arbitration Service                          = 0x8015B1XX
#define XONLINE_E_ARBITRATION_SERVICE_UNAVAILABLE                  _HRESULT_TYPEDEF_(0x8015B101L)   // Service temporarily unavailable
#define XONLINE_E_ARBITRATION_INVALID_REQUEST                      _HRESULT_TYPEDEF_(0x8015B102L)   // The request is invalidly formatted
#define XONLINE_E_ARBITRATION_SESSION_NOT_FOUND                    _HRESULT_TYPEDEF_(0x8015B103L)   // The session is not found or has expired
#define XONLINE_E_ARBITRATION_REGISTRATION_FLAGS_MISMATCH          _HRESULT_TYPEDEF_(0x8015B104L)   // The session was registered with different flags by another Xbox
#define XONLINE_E_ARBITRATION_REGISTRATION_SESSION_TIME_MISMATCH   _HRESULT_TYPEDEF_(0x8015B105L)   // The session was registered with a different session time by another Xbox
#define XONLINE_E_ARBITRATION_REGISTRATION_TOO_LATE                _HRESULT_TYPEDEF_(0x8015B106L)   // Registration came too late, the session has already been arbitrated
#define XONLINE_E_ARBITRATION_NEED_TO_REGISTER_FIRST               _HRESULT_TYPEDEF_(0x8015B107L)   // Must register in seesion first, before any other activity
#define XONLINE_E_ARBITRATION_TIME_EXTENSION_NOT_ALLOWED           _HRESULT_TYPEDEF_(0x8015B108L)   // Time extension of this session not allowed, or session is already arbitrated
#define XONLINE_E_ARBITRATION_INCONSISTENT_FLAGS                   _HRESULT_TYPEDEF_(0x8015B109L)   // Inconsistent flags are used in the request
#define XONLINE_E_ARBITRATION_INCONSISTENT_COMPETITION_STATUS      _HRESULT_TYPEDEF_(0x8015B10AL)   // Whether the session is a competition is inconsistent between registration and report
#define XONLINE_E_ARBITRATION_REPORT_ALREADY_CALLED                _HRESULT_TYPEDEF_(0x8015b10BL)   // Report call for this session already made by this client
#define XONLINE_E_ARBITRATION_TOO_MANY_XBOXES_IN_SESSION           _HRESULT_TYPEDEF_(0x8015b10CL)   // Only up to 255 Xboxes can register in a session
#define XONLINE_E_ARBITRATION_1_XBOX_1_USER_SESSION_NOT_ALLOWED    _HRESULT_TYPEDEF_(0x8015b10DL)   // Single Xbox single user sessions should not be arbitrated
#define XONLINE_E_ARBITRATION_REPORT_TOO_LARGE                     _HRESULT_TYPEDEF_(0x8015b10EL)   // The stats or query submission is too large
#define XONLINE_E_ARBITRATION_INVALID_TEAMTICKET                   _HRESULT_TYPEDEF_(0x8015b10FL)   // An invalid team ticket was submitted
// Arbitration success HRESULTS
#define XONLINE_S_ARBITRATION_INVALID_XBOX_SPECIFIED               _HRESULT_TYPEDEF_(0x0015b1F0L)   // Invalid/duplicate Xbox specified in lost connectivity or suspicious info. Never the less, this report is accepted
#define XONLINE_S_ARBITRATION_INVALID_USER_SPECIFIED               _HRESULT_TYPEDEF_(0x0015b1F1L)   // Invalid/duplicate user specified in lost connectivity or suspicious info. Never the less, this report is accepted
#define XONLINE_S_ARBITRATION_DIFFERENT_RESULTS_DETECTED           _HRESULT_TYPEDEF_(0x0015b1F2L)   // Differing result submissions have been detected in this session. Never the less, this report submission is accepted

// Errors returned by the Storage services              = 0x8015C0XX
#define XONLINE_E_STORAGE_INVALID_REQUEST               _HRESULT_TYPEDEF_(0x8015c001L)  // Request is invalid
#define XONLINE_E_STORAGE_ACCESS_DENIED                 _HRESULT_TYPEDEF_(0x8015c002L)  // Client doesn't have the rights to upload the file
#define XONLINE_E_STORAGE_FILE_IS_TOO_BIG               _HRESULT_TYPEDEF_(0x8015c003L)  // File is too big
#define XONLINE_E_STORAGE_FILE_NOT_FOUND                _HRESULT_TYPEDEF_(0x8015c004L)  // File not found
#define XONLINE_E_STORAGE_INVALID_ACCESS_TOKEN          _HRESULT_TYPEDEF_(0x8015c005L)  // Access token signature is invalid
#define XONLINE_E_STORAGE_CANNOT_FIND_PATH              _HRESULT_TYPEDEF_(0x8015c006L)  // name resolution failed
#define XONLINE_E_STORAGE_FILE_IS_ELSEWHERE             _HRESULT_TYPEDEF_(0x8015c007L)  // redirection request
#define XONLINE_E_STORAGE_INVALID_STORAGE_PATH          _HRESULT_TYPEDEF_(0x8015c008L)  // Invalid storage path
#define XONLINE_E_STORAGE_INVALID_FACILITY              _HRESULT_TYPEDEF_(0x8015c009L)  // Invalid facility code
#define XONLINE_E_STORAGE_UNKNOWN_DOMAIN                _HRESULT_TYPEDEF_(0x8015c00AL)  // Bad pathname
#define XONLINE_E_STORAGE_SYNC_TIME_SKEW                _HRESULT_TYPEDEF_(0x8015c00BL)  // SyncDomain timestamp skew
#define XONLINE_E_STORAGE_SYNC_TIME_SKEW_LOCALTIME      _HRESULT_TYPEDEF_(0x8015c00CL)  // SyncDomain timestamp appears to be localtime
#define XONLINE_E_STORAGE_QUOTA_EXCEEDED                _HRESULT_TYPEDEF_(0x8015c00DL)  // Quota exceeded for storage domain
#define XONLINE_E_STORAGE_UNSUPPORTED_CONTENT_TYPE      _HRESULT_TYPEDEF_(0x8015c00EL)  // The type of the content is not supported by this API

#define XONLINE_E_STORAGE_FILE_ALREADY_EXISTS           _HRESULT_TYPEDEF_(0x8015c011L)  // File already exists and storage domain does not allow overwrites
#define XONLINE_E_STORAGE_DATABASE_ERROR                _HRESULT_TYPEDEF_(0x8015c012L)  // Unknown database error
#define XONLINE_S_STORAGE_FILE_NOT_MODIFIED             _HRESULT_TYPEDEF_(0x0015c013L)  // The file was not modified since the last installation


#define XONLINE_E_LOGON_SG_CONNECTION_TIMEDOUT                      0x8015100C
#define XONLINE_E_ACCOUNTS_GRADUATE_USER_NO_PRIVILEGE               0x8015102A
#define XONLINE_E_ACCOUNTS_GRADUATE_USER_NOT_CHILD                  0x8015102B
#define XONLINE_E_ACCOUNTS_GRADUATE_USER_NOT_ADULT                  0x8015102C
#define XONLINE_E_ACCOUNTS_GRADUATE_USER_NO_PI                      0x8015102D
#define XONLINE_E_ACCOUNTS_GRADUATE_USER_PI_MISMATCH                0x8015102E
#define XONLINE_E_ACCOUNTS_GRADUATE_USER_ALREADY                    0x8015102F
#define XONLINE_E_ACCOUNTS_GRADUATE_USER_QUEUED                     0x80151031
#define XONLINE_E_ACCOUNTS_SWITCH_PASSPORT_NEW_PASSPORT_INELIGIBLE  0x80151032
#define XONLINE_E_ACCOUNTS_NO_AUTHENTICATION_DATA                   0x80151033
#define XONLINE_E_ACCOUNTS_CLIENT_TYPE_CONFIG_ERROR                 0x80151034
#define XONLINE_E_ACCOUNTS_CLIENT_TYPE_MISSING                      0x80151035
#define XONLINE_E_ACCOUNTS_CLIENT_TYPE_INVALID                      0x80151036
#define XONLINE_E_ACCOUNTS_COUNTRY_NOT_AUTHORIZED                   0x80151037
#define XONLINE_E_ACCOUNTS_TAG_CHANGE_REQUIRED                      0x80151038
#define XONLINE_E_ACCOUNTS_ACCOUNT_SUSPENDED                        0x80151039
#define XONLINE_E_ACCOUNTS_TERMS_OF_SERVICE_NOT_ACCEPTED            0x8015103A
#define XONLINE_E_LOGON_PPLOGIN_FAILED                              0x80151103
#define XONLINE_E_LOGON_SPONSOR_TOKEN_INVALID                       0x80151104
#define XONLINE_E_LOGON_SPONSOR_TOKEN_BANNED                        0x80151105
#define XONLINE_E_LOGON_SPONSOR_TOKEN_USAGE_EXCEEDED                0x80151106
#define XONLINE_E_LOGON_FLASH_UPDATE_NOT_DOWNLOADED                 0x80151107
#define XONLINE_E_LOGON_UPDATE_NOT_DOWNLOADED                       0x80151108
#define XONLINE_E_LOGON_NOT_LOGGED_ON                               0x80151802
#define XONLINE_E_LOGON_DNS_LOOKUP_FAILED                           0x80151903
#define XONLINE_E_LOGON_DNS_LOOKUP_TIMEDOUT                         0x80151904
#define XONLINE_E_LOGON_MACS_FAILED                                 0x80151906
#define XONLINE_E_LOGON_MACS_TIMEDOUT                               0x80151907
#define XONLINE_E_LOGON_AUTHENTICATION_TIMEDOUT                     0x80151909
#define XONLINE_E_LOGON_UNKNOWN_TITLE                               0x80151912
#define XONLINE_E_ACCOUNTS_NAME_TAKEN                               0x80154000
#define XONLINE_E_ACCOUNTS_INVALID_KINGDOM                          0x80154001
#define XONLINE_E_ACCOUNTS_INVALID_USER                             0x80154002
#define XONLINE_E_ACCOUNTS_BAD_CREDIT_CARD                          0x80154003
#define XONLINE_E_ACCOUNTS_BAD_BILLING_ADDRESS                      0x80154004
#define XONLINE_E_ACCOUNTS_ACCOUNT_BANNED                           0x80154005
#define XONLINE_E_ACCOUNTS_PERMISSION_DENIED                        0x80154006
#define XONLINE_E_ACCOUNTS_INVALID_VOUCHER                          0x80154007
#define XONLINE_E_ACCOUNTS_DATA_CHANGED                             0x80154008
#define XONLINE_E_ACCOUNTS_VOUCHER_ALREADY_USED                     0x80154009
#define XONLINE_E_ACCOUNTS_OPERATION_BLOCKED                        0x8015400A
#define XONLINE_E_ACCOUNTS_POSTAL_CODE_REQUIRED                     0x8015400B
#define XONLINE_E_ACCOUNTS_TRY_AGAIN_LATER                          0x8015400C
#define XONLINE_E_ACCOUNTS_NOT_A_RENEWAL_OFFER                      0x8015400D
#define XONLINE_E_ACCOUNTS_RENEWAL_IS_LOCKED                        0x8015400E
#define XONLINE_E_ACCOUNTS_VOUCHER_REQUIRED                         0x8015400F
#define XONLINE_E_ACCOUNTS_ALREADY_DEPROVISIONED                    0x80154010
#define XONLINE_E_ACCOUNTS_INVALID_PRIVILEGE                        0x80154011
#define XONLINE_E_ACCOUNTS_INVALID_SIGNED_PASSPORT_PUID             0x80154012
#define XONLINE_E_ACCOUNTS_PASSPORT_ALREADY_LINKED                  0x80154013
#define XONLINE_E_ACCOUNTS_MIGRATE_NOT_XBOX1_USER                   0x80154014
#define XONLINE_E_ACCOUNTS_MIGRATE_BAD_SUBSCRIPTION                 0x80154015
#define XONLINE_E_ACCOUNTS_PASSPORT_NOT_LINKED                      0x80154016
#define XONLINE_E_ACCOUNTS_NOT_XENON_USER                           0x80154017
#define XONLINE_E_ACCOUNTS_CREDIT_CARD_REQUIRED                     0x80154018
#define XONLINE_E_ACCOUNTS_MIGRATE_NOT_XBOXCOM_USER                 0x80154019
#define XONLINE_E_ACCOUNTS_NOT_A_VOUCHER_OFFER                      0x8015401A
#define XONLINE_E_ACCOUNTS_REACHED_TRIAL_OFFER_LIMIT                0x8015401B
#define XONLINE_E_ACCOUNTS_XBOX1_MANAGEMENT_BLOCKED                 0x8015401C
#define XONLINE_E_ACCOUNTS_OFFLINE_XUID_ALREADY_USED                0x8015401D
#define XONLINE_E_ACCOUNTS_BILLING_PROVIDER_TIMEOUT                 0x8015401E
#define XONLINE_E_ACCOUNTS_MIGRATION_OFFER_NOT_FOUND                0x8015401F
#define XONLINE_E_ACCOUNTS_UNDER_AGE                                0x80154020
#define XONLINE_E_ACCOUNTS_XBOX1_LOGON_BLOCKED                      0x80154021
#define XONLINE_E_ACCOUNTS_VOUCHER_INVALID_FOR_TIER                 0x80154022
#define XONLINE_E_ACCOUNTS_SWITCH_PASSPORT_QUEUED                   0x80154023
#define XONLINE_E_ACCOUNTS_SERVICE_NOT_PROVISIONED                  0x80154024
#define XONLINE_E_ACCOUNTS_ACCOUNT_UNBAN_BLOCKED                    0x80154025
#define XONLINE_E_ACCOUNTS_SWITCH_PASSPORT_INELIGIBLE               0x80154026
#define XONLINE_E_ACCOUNTS_ADDITIONAL_DATA_REQUIRED                 0x80154027
#define XONLINE_E_ACCOUNTS_SWITCH_PASSPORT_SCS_PENDING              0x80154028
#define XONLINE_E_ACCOUNTS_SWITCH_PASSPORT_NO_BIRTHDATE             0x80154029
#define XONLINE_E_ACCOUNTS_SWITCH_PASSPORT_ADULT_TO_CHILD           0x80154030
//#define XONLINE_E_ACCOUNTS_ WHAT ???                               0x80154038
#define XONLINE_E_ACCOUNTS_USER_GET_ACCOUNT_INFO_ERROR              0x80154098
#define XONLINE_E_ACCOUNTS_USER_OPTED_OUT                           0x80154099


#define XONLINE_NOTIFICATION_TYPE_FRIENDREQUEST    0x00000001
#define XONLINE_NOTIFICATION_TYPE_FRIENDSTATUS     0x00000002
#define XONLINE_NOTIFICATION_TYPE_GAMEINVITE       0x00000004
#define XONLINE_NOTIFICATION_TYPE_GAMEINVITEANSWER 0x00000008
#define XONLINE_NOTIFICATION_TYPE_ALL              0xFFFFFFFF

#define XONLINE_FRIENDSTATE_FLAG_NONE                   0x00000000
#define XONLINE_FRIENDSTATE_FLAG_ONLINE                 0x00000001
#define XONLINE_FRIENDSTATE_FLAG_PLAYING                0x00000002
#define XONLINE_FRIENDSTATE_FLAG_CLOAKED                0x00000004
#define XONLINE_FRIENDSTATE_FLAG_VOICE                  0x00000008
#define XONLINE_FRIENDSTATE_FLAG_JOINABLE               0x00000010
#define XONLINE_FRIENDSTATE_MASK_GUESTS                 0x00000060
#define XONLINE_FRIENDSTATE_FLAG_RESERVED0              0x00000080
#define XONLINE_FRIENDSTATE_FLAG_JOINABLE_FRIENDS_ONLY  0x00000100
#define XONLINE_FRIENDSTATE_FLAG_SENTINVITE             0x04000000
#define XONLINE_FRIENDSTATE_FLAG_RECEIVEDINVITE         0x08000000
#define XONLINE_FRIENDSTATE_FLAG_INVITEACCEPTED         0x10000000
#define XONLINE_FRIENDSTATE_FLAG_INVITEREJECTED         0x20000000
#define XONLINE_FRIENDSTATE_FLAG_SENTREQUEST            0x40000000
#define XONLINE_FRIENDSTATE_FLAG_RECEIVEDREQUEST        0x80000000

#define XONLINE_FRIENDSTATE_ENUM_ONLINE            0x00000000
#define XONLINE_FRIENDSTATE_ENUM_AWAY              0x00010000
#define XONLINE_FRIENDSTATE_ENUM_BUSY              0x00020000
#define XONLINE_FRIENDSTATE_MASK_USER_STATE        0x000F0000
#define XONLINE_FRIENDSTATE_ENUM_CONSOLE_XBOX1     0x00000000
#define XONLINE_FRIENDSTATE_ENUM_CONSOLE_XBOX360   0x00001000
#define XONLINE_FRIENDSTATE_ENUM_CONSOLE_WINPC     0x00002000
#define XONLINE_FRIENDSTATE_MASK_CONSOLE_TYPE      0x00007000

#define XOnlineIsUserAway(dwState) ((dwState & XONLINE_FRIENDSTATE_FLAG_ONLINE) && \
                                    ((dwState & XONLINE_FRIENDSTATE_MASK_USER_STATE) == \
                                    XONLINE_FRIENDSTATE_ENUM_AWAY))

#define XOnlineIsUserBusy(dwState) ((dwState & XONLINE_FRIENDSTATE_FLAG_ONLINE) && \
                                    ((dwState & XONLINE_FRIENDSTATE_MASK_USER_STATE) == \
                                    XONLINE_FRIENDSTATE_ENUM_BUSY))

#define XOnlineGetGuests(dwState) ((dwState & XONLINE_FRIENDSTATE_MASK_GUESTS) >> 5)

#define XOnlineSetGuests(dwState, bGuests) (dwState &= (bGuests << 5) & XONLINE_FRIENDSTATE_MASK_GUESTS)

// TODO check if 22 is wrong.
#define MAX_RICHPRESENCE_SIZE 22
//#define MAX_RICHPRESENCE_SIZE                            64

#define MAX_FRIENDS                             100

#define MAX_PRESENCE                100

#define XPRESENCE_MAX_TITLE_SUBS                         400
#define XPRESENCE_MAX_SYS_SUBS                           400
#define XPRESENCE_MIN_SYS_SUBS_PER_USER                  100
#define XPRESENCE_SYNC_SUBS_FLAG_CHANGELIST              0x00000001

#define X_ATTRIBUTE_DATATYPE_NULL                        0x00F00000
#define X_ATTRIBUTE_ID_MASK                              0x0000FFFF
#define X_ATTRIBUTE_SCOPE_TITLE_SPECIFIC                 0x00000000
#define STRING_MAX_RESULTS                               100
#define MAX_STRINGSVR_STRING_LEN                         512
#define PQUEUE_TRANSIENT_MSGS                            5
#define PQUEUE_INVALID                                   8
#define PLIST_BUDDY_NOP                                  0
#define PLIST_BUDDY_ADD                                  1
#define PLIST_BUDDY_ADDED                                2
#define PLIST_BUDDY_ACCEPT                               3
#define PLIST_BUDDY_REJECT                               4
#define PLIST_BUDDY_DELETE                               5
#define MAX_PEER_SUBSCRIPTIONS                           1000
#define XHTTP_REQUEST_MAX_HOST                           512
#define XHTTP_REQUEST_MAX_URL                            512
#define XHTTP_REQUEST_MAX_EXTRA_HEADERS                  256
#define XHTTP_REQUEST_TYPE_GET                           0x0000000
#define XHTTP_REQUEST_TYPE_POST                          0x0000001
#define XHTTP_REQUEST_TYPE_MASK                          0x0000001
#define ACCOUNT_PIN_LENGTH                               4
#define VOUCHER_LENGTH                                   25
#define MAX_VOUCHER_SIZE                                 VOUCHER_LENGTH+1
#define MAX_DASH_LANGUAGE_ID                             9
#define XONLINE_PASSPORT_PASSWORD_MAX_LEN                32
#define XONLINE_PASSPORT_SECRET_ANSWER_MIN_LEN           5
#define XONLINE_PASSPORT_SECRET_ANSWER_MAX_LEN           64
#define XONLINE_PASSPORT_SESSION_TOKEN_LEN               28
#define XONLINE_PASSPORT_PUBLIC_KEY_DIGEST_LEN           20
#define XONLINE_PASSPORT_LOGIN_NONCE_MIN_LEN             32
#define XONLINE_PASSPORT_LOGIN_LEGACY_SERVICE_ID         0
#define XONLINE_XBOS_MAX_TITLE_CATEGORY_LENGTH           60
#define XONLINE_ASSET_SIGNATURE_SIZE                     256
#define XMARKETPLACE_MAX_ASSETS_CONSUMED                 100
#define XACTIVATION_SIGNATURE_SIZE                       256
#define XONLINE_QUERY_MAX_PAGE                           32
#define XONLINE_QUERY_MAX_PAGE_SIZE                      32
#define XONLINE_MAX_STATS_RATING_COUNT                   101
#define XUSER_PRIVATE_DATA_TYPE_INT32                    1
#define XUSER_PRIVATE_DATA_TYPE_INT64                    2
#define XUSER_PRIVATE_DATA_TYPE_DOUBLE                   3
#define XUSER_PRIVATE_DATA_TYPE_FLOAT                    5
#define XCONTENT_PATH_MAX_SIZE                           256
#define XMSG_MAX_RECIPIENTS                              100
#define XMSG_MAX_GAMERTAG_LENGTH                         15
#define XMSG_MAX_SUBJECT_SIZE                            20
#define XMSG_DELETE_MESSAGE_FLAG_BLOCK_SENDER            0x00000001
#define MAX_AFFILIATES_ADD                               16
#define X_AFFILIATE_ENCOUNTERED_INDEX                    0
#define X_AFFILIATE_COMPLETED_GAME_INDEX                 1
#define X_AFFILIATE_POSITIVE_FEEDBACK_INDEX              2
#define X_AFFILIATE_NEGATIVE_FEEDBACK_INDEX              3
#define X_AFFILIATE_AVOIDING_ME_INDEX                    4
#define NUM_STORED_AFFILIATE_LISTS                       5
#define X_AFFILIATE_FRIEND_INDEX                         16
#define X_AFFILIATE_BLOCK_INDEX                          17
#define X_AFFILIATE_MUTE_INDEX                           18
#define X_AFFILIATE_INDEX_ERROR                          0xFFFFFFFF
#define X_AFFILIATE_FLAGS_IN_USE                         0x0007001F
#define X_ADD_AFFILIATE_MAX                              16
#define XONLINE_SIGNATURE_DIGEST_SIZE                    20
#define XSIGNATURE_CONSOLE_ID_LEN                        5
#define XSIGNATURE_USER_PUIDS                            4
#define XSIGNATURE_DIGEST_LEN                            20
#define RSAPUB_2048_SIZE                                 272
#define XONLINE_MAX_PATHNAME_LENGTH                      255
#define XSTORAGE_MAX_MEMORY_BUFFER_SIZE                  100000000
#define XSTORAGE_CONTENT_TYPE_PACKAGE                    0
#define XSTORAGE_CONTENT_TYPE_BLOB                       1
#define XSTORAGE_COMPRESSION_TYPE_NONE                   0
#define XSTORAGE_ACCESS_TOKEN_VERSION                    1
#define XSTORAGE_OPERATION_UPLOAD                        1
#define XSTORAGE_OPERATION_DOWNLOAD                      2
#define XSTORAGE_OPERATION_DELETEFILE                    3
#define XSTORAGE_OPERATION_ENUMERATE                     4
#define XSTORAGE_MAX_RESULTS_TO_RETURN                   256
#define XSTORAGE_DOWNLOAD_TO_MEMORY_EXTRA_OBJECT_SIZE    448
#define XSTORAGE_UPLOAD_FROM_MEMORY_EXTRA_OBJECT_SIZE    48
#define XSTORAGE_ENUMERATE_EXTRA_OBJECT_SIZE             48
#define XSTORAGE_DELETE_EXTRA_OBJECT_SIZE                48
#define XINVITE_SEND_MESSAGE_EXTRA_OBJECT_SIZE           1500
#define XCONTENT_REFERRAL_SYM_KEY_SIZE                   16
#define XCONTENT_REFERRAL_PUB_KEY_SIZE                   284
#define XCONTENT_DOWNLOAD_NO_NOTIFICATION                0x00000001
#define XCONTENT_DOWNLOAD_NO_UIPOPUP                     0x00000002
#define XCONTENT_DOWNLOAD_UPDATE_REDIRECT                0x00000004
#define REVOCATION_LIST_CLIENT_NONCE_SIZE                16
#define REVOCATION_LIST_SIGNATURE_SIZE                   256
#define REVOCATION_LIST_SERVER_NONCE_SIZE                16
#define REVOCATION_LIST_ENTRY_DIGEST_SIZE                20
#define XWMDRM_DEVICE_PUBLIC_KEY                         40
#define XARB_PROTOCOL_VERSION                            1
#define FB_REVIEW_PREFER                                 0
#define FB_REVIEW_AVOID_TRASH_TALK                       1
#define FB_REVIEW_AVOID_LANGUAGE                         2
#define FB_REVIEW_AVOID_DISRUPTIVE                       3
#define FB_REVIEW_AVOID_AGGRESSIVE                       4
#define FB_REVIEW_AVOID_UNSPORTING                       5
#define FB_REVIEW_AVOID_DISCONNECT                       6
#define FB_REVIEW_AVOID_UNSKILLED                        7
#define FB_REVIEW_AVOID_TOO_GOOD                         8
#define FB_REVIEW_AVOID_UNFAMILIAR                       9
#define FB_COMPLAINT_OFFENSIVE_GAMERTAG                  0
#define FB_COMPLAINT_OFFENSIVE_MOTTO                     1
#define FB_COMPLAINT_OFFENSIVE_TEXT                      2
#define FB_COMPLAINT_OFFENSIVE_VOICE_MAIL                3
#define FB_COMPLAINT_OFFENSIVE_VOICE_CHAT                4
#define FB_COMPLAINT_OFFENSIVE_IN_GAME                   5
#define FB_COMPLAINT_TAMPERING_FEEDBACK                  6
#define FB_COMPLAINT_TAMPERING_SYSTEM                    7
#define FB_COMPLAINT_TAMPERING_CHEATING                  8
#define FB_COMPLAINT_OFFENSIVE_CONTENT                   9
#define FB_COMPLAINT_OFFENSIVE_VIDEOMESSAGE              10
#define FB_COMPLAINT_OFFENSIVE_VIDEOCHAT                 11
#define FB_COMPLAINT_OFFENSIVE_GAMERPICTURE              12
#define FB_COMPLAINT_OFFENSIVE_INGAME_VIDEO              13
#define FB_COMPLAINT_OFFENSIVE_PHOTOMESSAGE              14
#define FB_COMPLAINT_MAX_TYPE                            15

#define FACILITY_XONLINE                                21

//
// Country Codes
//
#define XONLINE_COUNTRY_UNITED_ARAB_EMIRATES 1
#define XONLINE_COUNTRY_ALBANIA              2
#define XONLINE_COUNTRY_ARMENIA              3
#define XONLINE_COUNTRY_ARGENTINA            4
#define XONLINE_COUNTRY_AUSTRIA              5
#define XONLINE_COUNTRY_AUSTRALIA            6
#define XONLINE_COUNTRY_AZERBAIJAN           7
#define XONLINE_COUNTRY_BELGIUM              8
#define XONLINE_COUNTRY_BULGARIA             9
#define XONLINE_COUNTRY_BAHRAIN              10
#define XONLINE_COUNTRY_BRUNEI_DARUSSALAM    11
#define XONLINE_COUNTRY_BOLIVIA              12
#define XONLINE_COUNTRY_BRAZIL               13
#define XONLINE_COUNTRY_BELARUS              14
#define XONLINE_COUNTRY_BELIZE               15
#define XONLINE_COUNTRY_CANADA               16
#define XONLINE_COUNTRY_SWITZERLAND          18
#define XONLINE_COUNTRY_CHILE                19
#define XONLINE_COUNTRY_CHINA                20
#define XONLINE_COUNTRY_COLOMBIA             21
#define XONLINE_COUNTRY_COSTA_RICA           22
#define XONLINE_COUNTRY_CZECH_REPUBLIC       23
#define XONLINE_COUNTRY_GERMANY              24
#define XONLINE_COUNTRY_DENMARK              25
#define XONLINE_COUNTRY_DOMINICAN_REPUBLIC   26
#define XONLINE_COUNTRY_ALGERIA              27
#define XONLINE_COUNTRY_ECUADOR              28
#define XONLINE_COUNTRY_ESTONIA              29
#define XONLINE_COUNTRY_EGYPT                30
#define XONLINE_COUNTRY_SPAIN                31
#define XONLINE_COUNTRY_FINLAND              32
#define XONLINE_COUNTRY_FAROE_ISLANDS        33
#define XONLINE_COUNTRY_FRANCE               34
#define XONLINE_COUNTRY_GREAT_BRITAIN        35
#define XONLINE_COUNTRY_GEORGIA              36
#define XONLINE_COUNTRY_GREECE               37
#define XONLINE_COUNTRY_GUATEMALA            38
#define XONLINE_COUNTRY_HONG_KONG            39
#define XONLINE_COUNTRY_HONDURAS             40
#define XONLINE_COUNTRY_CROATIA              41
#define XONLINE_COUNTRY_HUNGARY              42
#define XONLINE_COUNTRY_INDONESIA            43
#define XONLINE_COUNTRY_IRELAND              44
#define XONLINE_COUNTRY_ISRAEL               45
#define XONLINE_COUNTRY_INDIA                46
#define XONLINE_COUNTRY_IRAQ                 47
#define XONLINE_COUNTRY_IRAN                 48
#define XONLINE_COUNTRY_ICELAND              49
#define XONLINE_COUNTRY_ITALY                50
#define XONLINE_COUNTRY_JAMAICA              51
#define XONLINE_COUNTRY_JORDAN               52
#define XONLINE_COUNTRY_JAPAN                53
#define XONLINE_COUNTRY_KENYA                54
#define XONLINE_COUNTRY_KYRGYZSTAN           55
#define XONLINE_COUNTRY_KOREA                56
#define XONLINE_COUNTRY_KUWAIT               57
#define XONLINE_COUNTRY_KAZAKHSTAN           58
#define XONLINE_COUNTRY_LEBANON              59
#define XONLINE_COUNTRY_LIECHTENSTEIN        60
#define XONLINE_COUNTRY_LITHUANIA            61
#define XONLINE_COUNTRY_LUXEMBOURG           62
#define XONLINE_COUNTRY_LATVIA               63
#define XONLINE_COUNTRY_LIBYA                64
#define XONLINE_COUNTRY_MOROCCO              65
#define XONLINE_COUNTRY_MONACO               66
#define XONLINE_COUNTRY_MACEDONIA            67
#define XONLINE_COUNTRY_MONGOLIA             68
#define XONLINE_COUNTRY_MACAU                69
#define XONLINE_COUNTRY_MALDIVES             70
#define XONLINE_COUNTRY_MEXICO               71
#define XONLINE_COUNTRY_MALAYSIA             72
#define XONLINE_COUNTRY_NICARAGUA            73
#define XONLINE_COUNTRY_NETHERLANDS          74
#define XONLINE_COUNTRY_NORWAY               75
#define XONLINE_COUNTRY_NEW_ZEALAND          76
#define XONLINE_COUNTRY_OMAN                 77
#define XONLINE_COUNTRY_PANAMA               78
#define XONLINE_COUNTRY_PERU                 79
#define XONLINE_COUNTRY_PHILIPPINES          80
#define XONLINE_COUNTRY_PAKISTAN             81
#define XONLINE_COUNTRY_POLAND               82
#define XONLINE_COUNTRY_PUERTO_RICO          83
#define XONLINE_COUNTRY_PORTUGAL             84
#define XONLINE_COUNTRY_PARAGUAY             85
#define XONLINE_COUNTRY_QATAR                86
#define XONLINE_COUNTRY_ROMANIA              87
#define XONLINE_COUNTRY_RUSSIAN_FEDERATION   88
#define XONLINE_COUNTRY_SAUDI_ARABIA         89
#define XONLINE_COUNTRY_SWEDEN               90
#define XONLINE_COUNTRY_SINGAPORE            91
#define XONLINE_COUNTRY_SLOVENIA             92
#define XONLINE_COUNTRY_SLOVAK_REPUBLIC      93
#define XONLINE_COUNTRY_EL_SALVADOR          95
#define XONLINE_COUNTRY_SYRIA                96
#define XONLINE_COUNTRY_THAILAND             97
#define XONLINE_COUNTRY_TUNISIA              98
#define XONLINE_COUNTRY_TURKEY               99
#define XONLINE_COUNTRY_TRINIDAD_AND_TOBAGO  100
#define XONLINE_COUNTRY_TAIWAN               101
#define XONLINE_COUNTRY_UKRAINE              102
#define XONLINE_COUNTRY_UNITED_STATES        103
#define XONLINE_COUNTRY_URUGUAY              104
#define XONLINE_COUNTRY_UZBEKISTAN           105
#define XONLINE_COUNTRY_VENEZUELA            106
#define XONLINE_COUNTRY_VIET_NAM             107
#define XONLINE_COUNTRY_YEMEN                108
#define XONLINE_COUNTRY_SOUTH_AFRICA         109
#define XONLINE_COUNTRY_ZIMBABWE             110


#define XNET_XNKID_MASK                 0xF0    // Mask of flag bits in first byte of XNKID
#define XNET_XNKID_SYSTEM_LINK          0x00    // Peer to peer system link session
#define XNET_XNKID_SYSTEM_LINK_XPLAT    0x40    // Peer to peer system link session for cross-platform
#define XNET_XNKID_ONLINE_PEER          0x80    // Peer to peer online session
#define XNET_XNKID_ONLINE_SERVER        0xC0    // Client to server online session
#define XNET_XNKID_ONLINE_TITLESERVER   0xE0    // Client to title server online session

#define XNetXnKidFlag(xnkid)       ((xnkid)->ab[0] & 0xE0)
#define XNetXnKidIsSystemLinkXbox(xnkid)       (((xnkid)->ab[0] & 0xE0) == XNET_XNKID_SYSTEM_LINK)
#define XNetXnKidIsSystemLinkXPlat(xnkid)      (((xnkid)->ab[0] & 0xE0) == XNET_XNKID_SYSTEM_LINK_XPLAT)
#define XNetXnKidIsSystemLink(xnkid)           (XNetXnKidIsSystemLinkXbox(xnkid) || XNetXnKidIsSystemLinkXPlat(xnkid))
#define XNetXnKidIsOnlinePeer(xnkid)           (((xnkid)->ab[0] & 0xE0) == XNET_XNKID_ONLINE_PEER)
#define XNetXnKidIsOnlineServer(xnkid)         (((xnkid)->ab[0] & 0xE0) == XNET_XNKID_ONLINE_SERVER)
#define XNetXnKidIsOnlineTitleServer(xnkid)    (((xnkid)->ab[0] & 0xE0) == XNET_XNKID_ONLINE_TITLESERVER)

#define XTITLE_SERVER_MAX_LSP_INFO 1000
#define XTITLE_SERVER_MAX_SERVER_INFO_LEN 199
#define XTITLE_SERVER_MAX_SERVER_INFO_SIZE 200

#define XNET_XNQOSINFO_COMPLETE         0x01    // Qos has finished processing this entry
#define XNET_XNQOSINFO_TARGET_CONTACTED 0x02    // Target host was successfully contacted
#define XNET_XNQOSINFO_TARGET_DISABLED  0x04    // Target host has disabled its Qos listener
#define XNET_XNQOSINFO_DATA_RECEIVED    0x08    // Target host supplied Qos data
#define XNET_XNQOSINFO_PARTIAL_COMPLETE 0x10    // Qos has unfinished estimates for this entry
#define XNET_XNQOSINFO_UNK	0x11

#define XNET_QOS_LISTEN_ENABLE              0x00000001 // Responds to queries on the given XNKID
#define XNET_QOS_LISTEN_DISABLE             0x00000002 // Rejects queries on the given XNKID
#define XNET_QOS_LISTEN_SET_DATA            0x00000004 // Sets the block of data to send back to queriers
#define XNET_QOS_LISTEN_SET_BITSPERSEC      0x00000008 // Sets max bandwidth that query reponses may consume
#define XNET_QOS_LISTEN_RELEASE             0x00000010 // Stops listening on given XNKID and releases memory

#define XNET_QOS_LOOKUP_RESERVED            0x00000000 // No flags defined yet for XNetQosLookup

#define XNET_QOS_SERVICE_LOOKUP_RESERVED    0x00000000 // No flags defined yet for XNetQosServiceLookup

#define XNET_XNADDR_PLATFORM_XBOX1          0x00000000 // Platform type is original Xbox
#define XNET_XNADDR_PLATFORM_XBOX360        0x00000001 // Platform type is Xbox 360
#define XNET_XNADDR_PLATFORM_WINPC          0x00000002 // Platform type is Windows PC

#define XNET_CONNECT_STATUS_IDLE            0x00000000 // Connection not started; use XNetConnect or send packet
#define XNET_CONNECT_STATUS_PENDING         0x00000001 // Connecting in progress; not complete yet
#define XNET_CONNECT_STATUS_CONNECTED       0x00000002 // Connection is established
#define XNET_CONNECT_STATUS_LOST            0x00000003 // Connection was lost

#define XNET_GET_XNADDR_PENDING             0x00000000 // Address acquisition is not yet complete
#define XNET_GET_XNADDR_NONE                0x00000001 // XNet is uninitialized or no debugger found
#define XNET_GET_XNADDR_ETHERNET            0x00000002 // Host has ethernet address (no IP address)
#define XNET_GET_XNADDR_STATIC              0x00000004 // Host has statically assigned IP address
#define XNET_GET_XNADDR_DHCP                0x00000008 // Host has DHCP assigned IP address
#define XNET_GET_XNADDR_PPPOE               0x00000010 // Host has PPPoE assigned IP address
#define XNET_GET_XNADDR_GATEWAY             0x00000020 // Host has one or more gateways configured
#define XNET_GET_XNADDR_DNS                 0x00000040 // Host has one or more DNS servers configured
#define XNET_GET_XNADDR_ONLINE              0x00000080 // Host is currently connected to online service
#define XNET_GET_XNADDR_TROUBLESHOOT        0x00008000 // Network configuration requires troubleshooting

#define XNET_ETHERNET_LINK_INACTIVE           0x00000000
#define XNET_ETHERNET_LINK_ACTIVE           0x00000001 // Ethernet cable is connected and active
#define XNET_ETHERNET_LINK_100MBPS          0x00000002 // Ethernet link is set to 100 Mbps
#define XNET_ETHERNET_LINK_10MBPS           0x00000004 // Ethernet link is set to 10 Mbps
#define XNET_ETHERNET_LINK_FULL_DUPLEX      0x00000008 // Ethernet link is in full duplex mode
#define XNET_ETHERNET_LINK_HALF_DUPLEX      0x00000010 // Ethernet link is in half duplex mode
#define XNET_ETHERNET_LINK_WIRELESS         0x00000020 // Ethernet link is wireless (802.11 based)

#define XNET_BROADCAST_VERSION_OLDER        0x00000001 // Got broadcast packet(s) from incompatible older version of title
#define XNET_BROADCAST_VERSION_NEWER        0x00000002 // Got broadcast packet(s) from incompatible newer version of title

//
// Value = XNetStartupParams
// Get   = Returns the XNetStartupParams values that were used at
//         initialization time.
// Set   = Not allowed.
//
#define XNET_OPTID_STARTUP_PARAMS                   1

//
// Value = uint64_t
// Get   = Returns total number of bytes sent by the NIC hardware since system
//         boot, including sizes of all protocol headers.
// Set   = Not allowed.
//
#define XNET_OPTID_NIC_XMIT_BYTES                   2

//
// Value = uint32_t
// Get   = Returns total number of frames sent by the NIC hardware since system
//         boot.
// Set   = Not allowed.
//
#define XNET_OPTID_NIC_XMIT_FRAMES                  3

//
// Value = uint64_t
// Get   = Returns total number of bytes received by the NIC hardware since
//         system boot, including sizes of all protocol headers.
// Set   = Not allowed.
//
#define XNET_OPTID_NIC_RECV_BYTES                   4

//
// Value = uint32_t
// Get   = Returns total number of frames received by the NIC hardware since
//         system boot.
// Set   = Not allowed.
//
#define XNET_OPTID_NIC_RECV_FRAMES                  5

//
// Value = uint64_t
// Get   = Returns the number of bytes sent by the caller since XNetStartup/
//         WSAStartup, including sizes of all protocol headers.
// Set   = Not allowed.
//
#define XNET_OPTID_CALLER_XMIT_BYTES                6

//
// Value = uint32_t
// Get   = Returns total number of frames sent by the caller since XNetStartup/
//         WSAStartup.
// Set   = Not allowed.
//
#define XNET_OPTID_CALLER_XMIT_FRAMES               7

//
// Value = uint64_t
// Get   = Returns total number of bytes received by the caller since
//         XNetStartup/WSAStartup, including sizes of all protocol headers.
// Set   = Not allowed.
//
#define XNET_OPTID_CALLER_RECV_BYTES                8

//
// Value = uint32_t
// Get   = Returns total number of frames received by the caller since
//         XNetStartup/WSAStartup.
// Set   = Not allowed.
//
#define XNET_OPTID_CALLER_RECV_FRAMES               9


#define XUSER_NAME_SIZE                 16
#define XUSER_MAX_NAME_LENGTH           (XUSER_NAME_SIZE - 1)

// Retrieves only the LIVE-enabled XUID (or INVALID_XUID if not available).
#define XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY      0x00000001
// Retrieves only the offline XUID.
#define XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY     0x00000002

#define XUSER_GET_SIGNIN_INFO_UNKNOWN_XUID_ONLY      0x00000004

#define XUSER_INFO_FLAG_LIVE_ENABLED    0x00000001
#define XUSER_INFO_FLAG_GUEST           0x00000002

typedef enum _XPRIVILEGE_TYPE
{
	XPRIVILEGE_MULTIPLAYER_SESSIONS = 254, // on|off

	XPRIVILEGE_COMMUNICATIONS = 252, // on (communicate w/everyone) | off (check _FO)
	XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY = 251, // on (communicate w/friends only) | off (blocked)

	XPRIVILEGE_PROFILE_VIEWING = 249, // on (viewing allowed) | off (check _FO)
	XPRIVILEGE_PROFILE_VIEWING_FRIENDS_ONLY = 248, // on (view friend's only) | off (no details)

	XPRIVILEGE_USER_CREATED_CONTENT = 247, // on (allow viewing of UCC) | off (check _FO)
	XPRIVILEGE_USER_CREATED_CONTENT_FRIENDS_ONLY = 246, // on (view UCC from friends only) | off (blocked)

	XPRIVILEGE_PURCHASE_CONTENT = 245, // on (allow purchase) | off (blocked)

	XPRIVILEGE_PRESENCE = 244, // on (share presence info) | off (check _FO)
	XPRIVILEGE_PRESENCE_FRIENDS_ONLY = 243, // on (share w/friends only | off (don't share)

	XPRIVILEGE_TRADE_CONTENT = 238, // on (allow trading) | off (blocked)

	XPRIVILEGE_VIDEO_COMMUNICATIONS = 235, // on (communicate w/everyone) | off (check _FO)
	XPRIVILEGE_VIDEO_COMMUNICATIONS_FRIENDS_ONLY = 234, // on (communicate w/friends only) | off (blocked)

	XPRIVILEGE_MULTIPLAYER_DEDICATED_SERVER = 226, // on (allow) | off (disallow)
} XPRIVILEGE_TYPE;

#define XUSER_DATA_TYPE_CONTEXT     ((uint8_t)0)
#define XUSER_DATA_TYPE_INT32       ((uint8_t)1)
#define XUSER_DATA_TYPE_INT64       ((uint8_t)2)
#define XUSER_DATA_TYPE_DOUBLE      ((uint8_t)3)
#define XUSER_DATA_TYPE_UNICODE     ((uint8_t)4)
#define XUSER_DATA_TYPE_FLOAT       ((uint8_t)5)
#define XUSER_DATA_TYPE_BINARY      ((uint8_t)6)
#define XUSER_DATA_TYPE_DATETIME    ((uint8_t)7)
#define XUSER_DATA_TYPE_NULL        ((uint8_t)0xFF)

#define XPROFILE_SETTING_MAX_SIZE               1000
#define XPROFILE_SETTING_MAX_PICTURE_KEY_PATH   100
#define XPROFILE_SETTING_MAX_GAMERCARD_MOTTO    (22 * sizeof(wchar_t))

#define XPROFILEID(type, size, id)              (((type & 0xf) << 28) | ((size & 0xfff) << 16) | (id & 0x3fff))

//                                                                      type                        size                    id

// These settings are readable and writable by titles

// Specifies whether controller vibration is enabled.
#define XPROFILE_OPTION_CONTROLLER_VIBRATION                XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          3)

// Title-specific settings are both readable and writable. These settings may also be accessed by both offline and LIVE-enabled, online profiles. The memory limit is 1 KB of binary data per setting.
#define XPROFILE_TITLE_SPECIFIC1                            XPROFILEID( XUSER_DATA_TYPE_BINARY,     XPROFILE_SETTING_MAX_SIZE,  0x3FFF)
#define XPROFILE_TITLE_SPECIFIC2                            XPROFILEID( XUSER_DATA_TYPE_BINARY,     XPROFILE_SETTING_MAX_SIZE,  0x3FFE)
#define XPROFILE_TITLE_SPECIFIC3                            XPROFILEID( XUSER_DATA_TYPE_BINARY,     XPROFILE_SETTING_MAX_SIZE,  0x3FFD)

// These settings are readable by titles (but not writable)

// Used to turn on or off y-axis inversion for the controller.
#define XPROFILE_GAMER_YAXIS_INVERSION                      XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          2)
// Preferred gamer zone. Zones are used to provide different kinds of play experience. This setting can be used to modify the behavior of your title. If you modify this setting, keep the following in mind:
// - Matchmaking does not filter available sessions based on gamer zones. It sorts them based only on the gamer zone that is specified.
// - Gamers can change zones at any time by using the Games for Windows - LIVE Guide.
// - This setting ID is applied on a per-profile basis, not a per-title basis.
#define XPROFILE_GAMERCARD_ZONE                             XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          4)
// Region of the gamer's profile, which can be used to set up matchmaking. Values specified as XONLINE_COUNTRY_*.
#define XPROFILE_GAMERCARD_REGION                           XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          5)
// Amount of gamercred that is earned by the gamer. The XPROFILE_GAMERCARD_TITLE_CRED_EARNED setting specifies this value per title.
#define XPROFILE_GAMERCARD_CRED                             XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          6)
// Current reputation rating of the gamer.
#define XPROFILE_GAMERCARD_REP                              XPROFILEID( XUSER_DATA_TYPE_FLOAT,      sizeof(DWORD),          11)

// Voice is muted.
#define XPROFILE_OPTION_VOICE_MUTED                         XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          12)
// LIVE gamer voices, output through main speakers.
#define XPROFILE_OPTION_VOICE_THRU_SPEAKERS                 XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          13)
// Volume level for LIVE gamer voices.
#define XPROFILE_OPTION_VOICE_VOLUME                        XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          14)

// Key of the gamer's Gamer Picture.
#define XPROFILE_GAMERCARD_PICTURE_KEY                      XPROFILEID( XUSER_DATA_TYPE_UNICODE,    XPROFILE_SETTING_MAX_PICTURE_KEY_PATH,  15)
// Motto of the gamer.
#define XPROFILE_GAMERCARD_MOTTO                            XPROFILEID( XUSER_DATA_TYPE_UNICODE,    XPROFILE_SETTING_MAX_GAMERCARD_MOTTO,   17)

// Number of titles that the gamer has played.
#define XPROFILE_GAMERCARD_TITLES_PLAYED                    XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          18)
// Total achievements that are earned by the gamer. The XPROFILE_GAMERCARD_TITLE_ACHIEVEMENTS_EARNED setting specifies this value per title.
#define XPROFILE_GAMERCARD_ACHIEVEMENTS_EARNED              XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          19)

// Sets the difficulty level of the game.
#define XPROFILE_GAMER_DIFFICULTY                           XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          21)
// Sets the sensitivity of the controller.
#define XPROFILE_GAMER_CONTROL_SENSITIVITY                  XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          24)
// Sets the first predominant color of the gamer's character model.
#define XPROFILE_GAMER_PREFERRED_COLOR_FIRST                XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          29)
// Sets the second predominant color of the gamer's character model.
#define XPROFILE_GAMER_PREFERRED_COLOR_SECOND               XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          30)
// Sets whether the gamer's aim is manual, or controlled automatically by the title.
#define XPROFILE_GAMER_ACTION_AUTO_AIM                      XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          34)
// Sets whether the view centers automatically when the gamer moves.
#define XPROFILE_GAMER_ACTION_AUTO_CENTER                   XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          35)
// Sets the control to use for movement.
#define XPROFILE_GAMER_ACTION_MOVEMENT_CONTROL              XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          36)
// Sets the vehicle transmission type.
#define XPROFILE_GAMER_RACE_TRANSMISSION                    XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          38)
// Sets the title's camera angle.
#define XPROFILE_GAMER_RACE_CAMERA_LOCATION                 XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          39)
// Sets the title's braking control.
#define XPROFILE_GAMER_RACE_BRAKE_CONTROL                   XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          40)
// Sets the title's acceleration control.
#define XPROFILE_GAMER_RACE_ACCELERATOR_CONTROL             XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          41)

// Number of credits earned by the gamer for this title.
#define XPROFILE_GAMERCARD_TITLE_CRED_EARNED                XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          56)
// Number of achievements that are earned by the gamer in the current title.
#define XPROFILE_GAMERCARD_TITLE_ACHIEVEMENTS_EARNED        XPROFILEID( XUSER_DATA_TYPE_INT32,      sizeof(DWORD),          57)

#define XUserGetProfileSettingMaxSize(dwSettingId)  ((dwSettingId & 0x0fff0000) >> 16)
#define XUserGetProfileSettingType(dwSettingId)     ((uint8_t)((dwSettingId & 0xf0000000) >> 28))

typedef enum _XPROFILE_GAMERCARD_ZONE_OPTIONS
{
    XPROFILE_GAMERCARD_ZONE_XBOX_1 = 0, // A gamer on an original Xbox.
    XPROFILE_GAMERCARD_ZONE_RR, // The gamer prefers casual competition and does not have to win to enjoy the experience. This is the default value.
    XPROFILE_GAMERCARD_ZONE_PRO, // The gamer prefers fierce competition and plays to win.
    XPROFILE_GAMERCARD_ZONE_FAMILY, // The gamer prefers a friendly environment (without mature language or trash-talking).
    XPROFILE_GAMERCARD_ZONE_UNDERGROUND // The gamer prefers competition with other hard-core gamers where dirty play and trash-talking are the standard.
} XPROFILE_GAMERCARD_ZONE_OPTIONS;

typedef enum _XPROFILE_GAMER_DIFFICULTY_OPTIONS
{
    XPROFILE_GAMER_DIFFICULTY_NORMAL = 0, // Standard difficulty setting of the game. This is the default value.
    XPROFILE_GAMER_DIFFICULTY_EASY, // Easiest difficulty setting of the game.
    XPROFILE_GAMER_DIFFICULTY_HARD // Hardest difficulty setting of the game.
} XPROFILE_GAMER_DIFFICULTY_OPTIONS;

typedef enum _XPROFILE_CONTROL_SENSITIVITY_OPTIONS
{
    XPROFILE_CONTROL_SENSITIVITY_MEDIUM = 0, // Sensitivity of the controller is normal. This is the default value.
    XPROFILE_CONTROL_SENSITIVITY_LOW, // Sensitivity of the controller is lower than normal.
    XPROFILE_CONTROL_SENSITIVITY_HIGH // Sensitivity of the controller is higher than normal.
} XPROFILE_CONTROL_SENSITIVITY_OPTIONS;

typedef enum _XPROFILE_PREFERRED_COLOR_OPTIONS
{
    XPROFILE_PREFERRED_COLOR_NONE = 0, // No color is set.
    XPROFILE_PREFERRED_COLOR_BLACK,
    XPROFILE_PREFERRED_COLOR_WHITE,
    XPROFILE_PREFERRED_COLOR_YELLOW,
    XPROFILE_PREFERRED_COLOR_ORANGE,
    XPROFILE_PREFERRED_COLOR_PINK,
    XPROFILE_PREFERRED_COLOR_RED,
    XPROFILE_PREFERRED_COLOR_PURPLE,
    XPROFILE_PREFERRED_COLOR_BLUE,
    XPROFILE_PREFERRED_COLOR_GREEN,
    XPROFILE_PREFERRED_COLOR_BROWN,
    XPROFILE_PREFERRED_COLOR_SILVER
} XPROFILE_PREFERRED_COLOR_OPTIONS;

typedef enum _XPROFILE_ACTION_AUTO_AIM_OPTIONS
{
    XPROFILE_ACTION_AUTO_AIM_OFF = 0, // The gamer controls aim manually. This is the default value.
    XPROFILE_ACTION_AUTO_AIM_ON // The title automatically corrects the gamer's aim.
} XPROFILE_ACTION_AUTO_AIM_OPTIONS;

typedef enum _XPROFILE_ACTION_AUTO_CENTER_OPTIONS
{
    XPROFILE_ACTION_AUTO_CENTER_OFF = 0, // The view does not automatically center when the gamer moves. This is the default value.
    XPROFILE_ACTION_AUTO_CENTER_ON // The view automatically centers when the gamer moves.
} XPROFILE_ACTION_AUTO_CENTER_OPTIONS;

typedef enum _XPROFILE_ACTION_MOVEMENT_CONTROL_OPTIONS
{
    XPROFILE_ACTION_MOVEMENT_CONTROL_L_THUMBSTICK = 0, // Left thumbstick controls movement. This is the default value.
    XPROFILE_ACTION_MOVEMENT_CONTROL_R_THUMBSTICK // Right thumbstick controls movement.
} XPROFILE_ACTION_MOVEMENT_CONTROL_OPTIONS;

typedef enum _XPROFILE_RACE_TRANSMISSION_OPTIONS
{
    XPROFILE_RACE_TRANSMISSION_AUTO = 0, // Vehicle has an automatic transmission. This is the default value.
    XPROFILE_RACE_TRANSMISSION_MANUAL // Vehicle has a manual transmission.
} XPROFILE_RACE_TRANSMISSION_OPTIONS;

typedef enum _XPROFILE_RACE_CAMERA_LOCATION_OPTIONS
{
    XPROFILE_RACE_CAMERA_LOCATION_BEHIND = 0, // Camera angle is from behind and outside the car. This is the default value.
    XPROFILE_RACE_CAMERA_LOCATION_IN_FRONT, // Camera angle is from the front and outside the car.
    XPROFILE_RACE_CAMERA_LOCATION_INSIDE // Camera angle is from inside the car.
} XPROFILE_RACE_CAMERA_LOCATION_OPTIONS;

typedef enum _XPROFILE_RACE_BRAKE_CONTROL_OPTIONS
{
    XPROFILE_RACE_BRAKE_CONTROL_TRIGGER = 0, // Pull a controller trigger to brake. This is the default value.
    XPROFILE_RACE_BRAKE_CONTROL_BUTTON // Press a controller button to brake.

} XPROFILE_RACE_BRAKE_CONTROL_OPTIONS;

typedef enum _XPROFILE_RACE_ACCELERATOR_CONTROL_OPTIONS
{
    XPROFILE_RACE_ACCELERATOR_CONTROL_TRIGGER = 0, // Pull a controller trigger to accelerate. This is the default value.
    XPROFILE_RACE_ACCELERATOR_CONTROL_BUTTON // Press a controller button to accelerate.
} XPROFILE_RACE_ACCELERATOR_CONTROL_OPTIONS;

typedef enum _XPROFILE_GAMER_YAXIS_INVERSION_OPTIONS
{
    XPROFILE_YAXIS_INVERSION_OFF = 0, // Disables y-axis inversion. This is the default value.
    XPROFILE_YAXIS_INVERSION_ON // Enables y-axis inversion.
} XPROFILE_GAMER_YAXIS_INVERSION_OPTIONS;

typedef enum _XPROFILE_CONTROLLER_VIBRATION_OPTIONS
{
    XPROFILE_CONTROLLER_VIBRATION_OFF = 0, // Controller vibration is off. This is the default setting.
    XPROFILE_CONTROLLER_VIBRATION_ON = 3 // Controller vibration is on.
} XPROFILE_CONTROLLER_VIBRATION_OPTIONS;

typedef enum _XPROFILE_VOICE_THRU_SPEAKERS_OPTIONS
{
    XPROFILE_VOICE_THRU_SPEAKERS_OFF = 0, // Voice through speakers is off: voice communication will only be heard from the headset. This is the default setting.
    XPROFILE_VOICE_THRU_SPEAKERS_ON, // Voice through speakers is on: voice communication will not be heard from the headset.
    XPROFILE_VOICE_THRU_SPEAKERS_BOTH // Voice communication is heard both from the speakers and in the headset.
} XPROFILE_VOICE_THRU_SPEAKERS_OPTIONS;

typedef enum {
	XSOURCE_NO_VALUE = 0, // There is no value to read.
	XSOURCE_DEFAULT, // The value was read from the defaults. It has not been overwritten by the user at a system, or title, level.
	XSOURCE_TITLE, // 	The value was overwritten by the title and was read from the stored profile.
	XSOURCE_PERMISSION_DENIED // The requesting user does not have permission to read this value.
} XUSER_PROFILE_SOURCE;


#define XLIVE_LICENSE_INFO_VERSION 1

//TODO Check these guesses
//#define XLIVE_LICENSE_ID_SIZE 20
#define XLIVE_LICENSE_ID_SIZE 28

//TODO Check this guess
#define XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_CONTENT_TYPES 0x2
//TODO Check this guess
#define XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_TITLES 0x4

#define XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_USERS 0x00000001

#define XLIVE_CONTENT_FLAG_RETRIEVE_BY_XUID 0x00000008

#define XLIVE_CONTENT_API_VERSION 1

// XContent

// Content types
#define XCONTENTTYPE_SAVEDGAME                      0x00000001
#define XCONTENTTYPE_MARKETPLACE                    0x00000002
#define XCONTENTTYPE_PUBLISHER                      0x00000003

#define XCONTENTTYPE_GAMEDEMO                       0x00080000
#define XCONTENTTYPE_ARCADE                         0x000D0000

//  Content creation/open flags
#define XCONTENTFLAG_NONE                           0x00000000
#define XCONTENTFLAG_CREATENEW                      CREATE_NEW
#define XCONTENTFLAG_CREATEALWAYS                   CREATE_ALWAYS
#define XCONTENTFLAG_OPENEXISTING                   OPEN_EXISTING
#define XCONTENTFLAG_OPENALWAYS                     OPEN_ALWAYS
#define XCONTENTFLAG_TRUNCATEEXISTING               TRUNCATE_EXISTING

//  Content attributes
#define XCONTENTFLAG_NOPROFILE_TRANSFER             0x00000010
#define XCONTENTFLAG_NODEVICE_TRANSFER              0x00000020
#define XCONTENTFLAG_STRONG_SIGNED                  0x00000040
#define XCONTENTFLAG_ALLOWPROFILE_TRANSFER          0x00000080
#define XCONTENTFLAG_MOVEONLY_TRANSFER              0x00000800

//  Device selector flags
#define XCONTENTFLAG_MANAGESTORAGE                  0x00000100
#define XCONTENTFLAG_FORCE_SHOW_UI                  0x00000200

//  Enumeration scoping
#define XCONTENTFLAG_ENUM_EXCLUDECOMMON             0x00001000


#define XCONTENT_MAX_DISPLAYNAME_LENGTH 128
#define XCONTENT_MAX_FILENAME_LENGTH    42

#define XLIVE_CONTENT_ID_SIZE 20

#define XCONTENT_NONE                   0x00000000
#define XCONTENT_CREATED_NEW            0x00000001
#define XCONTENT_OPENED_EXISTING        0x00000002

typedef enum _XLIVE_CONTENT_INSTALL_NOTIFICATION
{
	XLIVE_CONTENT_INSTALL_NOTIFY_TICKS_REQUIRED = 0,
	XLIVE_CONTENT_INSTALL_NOTIFY_STARTCOPY,
	XLIVE_CONTENT_INSTALL_NOTIFY_ENDCOPY,
	XLIVE_CONTENT_INSTALL_NOTIFY_STARTLICENSEVERIFY,
	XLIVE_CONTENT_INSTALL_NOTIFY_ENDLICENSEVERIFY,
	XLIVE_CONTENT_INSTALL_NOTIFY_STARTDELETE,
	XLIVE_CONTENT_INSTALL_NOTIFY_ENDDELETE,
	XLIVE_CONTENT_INSTALL_NOTIFY_STARTMOVE,
	XLIVE_CONTENT_INSTALL_NOTIFY_ENDMOVE,
	XLIVE_CONTENT_INSTALL_NOTIFY_STARTFILEVERIFY,
	XLIVE_CONTENT_INSTALL_NOTIFY_ENDFILEVERIFY
} XLIVE_CONTENT_INSTALL_NOTIFICATION, *PXLIVE_CONTENT_INSTALL_NOTIFICATION;

typedef HRESULT(*XLIVE_CONTENT_INSTALL_CALLBACK)(
		void* pContext,
		XLIVE_CONTENT_INSTALL_NOTIFICATION Notification,
		DWORD dwCurrentTick,
		UINT_PTR Param1,
		UINT_PTR Param2,
		HRESULT hrCurrentStatus
	);

typedef enum _XSESSION_STATE
{
    XSESSION_STATE_LOBBY = 0,              // Session is newly created
    XSESSION_STATE_REGISTRATION,           // Session is registering with arbitration
    XSESSION_STATE_INGAME,                 // Start has been called
    XSESSION_STATE_REPORTING,              // End has been called, uploading stats
    XSESSION_STATE_DELETED                 // Delete has been called, session is invalid
} XSESSION_STATE;

#define XSESSION_MEMBER_FLAGS_PRIVATE_SLOT      0x00000001
#define XSESSION_MEMBER_FLAGS_ZOMBIE            0x00000002

typedef enum _XSTORAGE_FACILITY
{
    XSTORAGE_FACILITY_GAME_CLIP      = 1,
    XSTORAGE_FACILITY_PER_TITLE      = 2,
    XSTORAGE_FACILITY_PER_USER_TITLE = 3,
} XSTORAGE_FACILITY;

#define XMARKETPLACE_IMAGE_URL_MINIMUM_WCHARCOUNT   55
//#define XMARKETPLACE_MAX_OFFERIDS                   6
#define XMARKETPLACE_MAX_OFFERIDS 20

#define XMARKETPLACE_MAX_OFFER_PRICE_TEXT_LENGTH        50
#define XMARKETPLACE_MAX_OFFER_NAME_LENGTH              100
#define XMARKETPLACE_MAX_OFFER_SELL_TEXT_LENGTH         1000
#define XMARKETPLACE_MAX_TITLE_NAME_LENGTH              100
#define XMARKETPLACE_MAX_OFFERS_ENUMERATED              100

#define XMARKETPLACE_CONTENT_ID_LEN                     20

// idk real macro name
#define XMARKETPLACE_ASSET_ID_MAX 0x7FFFFFFF
#define XMARKETPLACE_ASSET_MAX_ENUM_SIZE     100
#define XMARKETPLACE_ASSET_SIGNATURE_SIZE    256

typedef enum {
    XMARKETPLACE_OFFERING_TYPE_CONTENT              = 0x00000002,
    XMARKETPLACE_OFFERING_TYPE_GAME_DEMO            = 0x00000020,
    XMARKETPLACE_OFFERING_TYPE_GAME_TRAILER         = 0x00000040,
    XMARKETPLACE_OFFERING_TYPE_THEME                = 0x00000080,
    XMARKETPLACE_OFFERING_TYPE_TILE                 = 0x00000800,
    XMARKETPLACE_OFFERING_TYPE_ARCADE               = 0x00002000,
    XMARKETPLACE_OFFERING_TYPE_VIDEO                = 0x00004000,
    XMARKETPLACE_OFFERING_TYPE_CONSUMABLE           = 0x00010000,
} XMARKETPLACE_OFFERING_TYPE;

typedef enum _XSHOWMARKETPLACEUI_ENTRYPOINTS {
	XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST,
	XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM,
	XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPLIST,
	XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPITEM,
	XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST_BACKGROUND,
	XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM_BACKGROUND,
	XSHOWMARKETPLACEUI_ENTRYPOINT_MAX
} XSHOWMARKETPLACEUI_ENTRYPOINTS;

#define MPDI_E_CANCELLED            _HRESULT_TYPEDEF_(0x8057F001)
#define MPDI_E_INVALIDARG           _HRESULT_TYPEDEF_(0x8057F002)
#define MPDI_E_OPERATION_FAILED     _HRESULT_TYPEDEF_(0x8057F003)

typedef enum _XSHOWMARKETPLACEDOWNLOADITEMSUI_ENTRYPOINTS {
	XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_FREEITEMS = 1000,
	XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_PAIDITEMS,
	XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_MAX
} XSHOWMARKETPLACEDOWNLOADITEMSUI_ENTRYPOINTS;


#define XLIVE_MAX_RETURNED_OFFER_INFO 50
#define XLIVE_OFFER_INFO_VERSION 2
// including null terminator.
#define XLIVE_OFFER_INFO_TITLE_LENGTH 50
// including null terminator.
#define XLIVE_OFFER_INFO_DESCRIPTION_LENGTH 500
// including null terminator.
#define XLIVE_OFFER_INFO_IMAGEURL_LENGTH 1024


// XShowSiginUI flags.
#define XSSUI_FLAGS_LOCALSIGNINONLY                 0x00000001
#define XSSUI_FLAGS_SHOWONLYONLINEENABLED           0x00000002
#define XSSUI_FLAGS_ALLOW_SIGNOUT                   0x00000004
#define XSSUI_FLAGS_NUI                             0x00000008
#define XSSUI_FLAGS_DISALLOW_PLAYAS                 0x00000010
#define XSSUI_FLAGS_ADDUSER                         0x00010000
#define XSSUI_FLAGS_COMPLETESIGNIN                  0x00020000
#define XSSUI_FLAGS_SHOWONLYPARENTALLYCONTROLLED    0x00040000
#define XSSUI_FLAGS_ENABLE_GUEST                    0x00080000
#define XSSUI_FLAGS_DISALLOWRELOAD                  0x00100000
#define XSSUI_FLAGS_CONVERTOFFLINETOGUEST           0x00400000
#define XSSUI_FLAGS_DISALLOW_GUEST                  0x01000000


//------------------------------------------------------------------------------
// Notification apis
//------------------------------------------------------------------------------

//  Notification ids are 32 bit values layed out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +-+-----------+-----------------+-------------------------------+
//  |R|    Area   |    Version      |            Index              |
//  +-+-----+-----+-----------------+-------------------------------+
//
//  where
//
//      R - is a reserved bit (internal)
//
//      Area - is the area ranging from 0 - 63 (6 bits)
//
//      Version - is used to ensure that new notifications are not sent to
//          titles built to previous XDKs
//
//      Index - is the id of the notification within the area.  Each area can
//           have up to 65535 notifications starting at 1 (zero being invalid).
//

#define XNID(Version, Area, Index)      (DWORD)( (uint16_t)(Area) << 25 | (uint16_t)(Version) << 16 | (uint16_t)(Index))
#define XNID_VERSION(msgid)             (((msgid) >> 16) & 0x1FF)
#define XNID_AREA(msgid)                (((msgid) >> 25) & 0x3F)
#define XNID_INDEX(msgid)               ((msgid) & 0xFFFF)

// TODO remove. what version are those notifications below that use this? actually 0 not +1?
#define XNID_CURRENTVERSION

//
// Notification Areas
//

#define XNOTIFY_SYSTEM                  (0x00000001)
#define XNOTIFY_LIVE                    (0x00000002)
#define XNOTIFY_FRIENDS                 (0x00000004)
#define XNOTIFY_CUSTOM                  (0x00000008)
#define XNOTIFY_XMP                     (0x00000020)
#define XNOTIFY_MSGR                    (0x00000040)
#define XNOTIFY_PARTY                   (0x00000080)
#define XNOTIFY_ALL                     (XNOTIFY_SYSTEM | XNOTIFY_LIVE | XNOTIFY_FRIENDS | XNOTIFY_CUSTOM | XNOTIFY_XMP | XNOTIFY_MSGR | XNOTIFY_PARTY)

//
// Bit numbers of each area (bit 0 is the least significant bit)
//

#define _XNAREA_SYSTEM                  (0)
#define _XNAREA_LIVE                    (1)
#define _XNAREA_FRIENDS                 (2)
#define _XNAREA_CUSTOM                  (3)
#define _XNAREA_XMP                     (5)
#define _XNAREA_MSGR                    (6)
#define _XNAREA_PARTY                   (7)

//
// System notifications
//

#define XN_SYS_FIRST                    XNID(0, _XNAREA_SYSTEM, 0x0001)
#define XN_SYS_UI                       XNID(0, _XNAREA_SYSTEM, 0x0009)
#define XN_SYS_SIGNINCHANGED            XNID(0, _XNAREA_SYSTEM, 0x000a)
#define XN_SYS_STORAGEDEVICESCHANGED    XNID(0, _XNAREA_SYSTEM, 0x000b)
#define XN_SYS_PROFILESETTINGCHANGED    XNID(0, _XNAREA_SYSTEM, 0x000e)
#define XN_SYS_MUTELISTCHANGED          XNID(0, _XNAREA_SYSTEM, 0x0011)
#define XN_SYS_INPUTDEVICESCHANGED      XNID(0, _XNAREA_SYSTEM, 0x0012)
#define XN_SYS_XLIVETITLEUPDATE         XNID(0, _XNAREA_SYSTEM, 0x0015)
#define XN_SYS_XLIVESYSTEMUPDATE        XNID(0, _XNAREA_SYSTEM, 0x0016)
#define XN_SYS_INPUTDEVICECONFIGCHANGED XNID(1, _XNAREA_SYSTEM, 0x0013)
#define XN_SYS_PLAYTIMERNOTICE          XNID(3, _XNAREA_SYSTEM, 0x0015)
#define XN_SYS_AVATARCHANGED            XNID(4, _XNAREA_SYSTEM, 0x0017)
#define XN_SYS_NUIHARDWARESTATUSCHANGED XNID(6, _XNAREA_SYSTEM, 0x0019)
#define XN_SYS_NUIPAUSE                 XNID(6, _XNAREA_SYSTEM, 0x001a)
#define XN_SYS_NUIUIAPPROACH            XNID(6, _XNAREA_SYSTEM, 0x001b)
#define XN_SYS_DEVICEREMAP              XNID(6, _XNAREA_SYSTEM, 0x001c)
#define XN_SYS_NUIBINDINGCHANGED        XNID(6, _XNAREA_SYSTEM, 0x001d)
#define XN_SYS_AUDIOLATENCYCHANGED      XNID(8, _XNAREA_SYSTEM, 0x001e)
#define XN_SYS_NUICHATBINDINGCHANGED    XNID(8, _XNAREA_SYSTEM, 0x001f)
#define XN_SYS_INPUTACTIVITYCHANGED     XNID(9, _XNAREA_SYSTEM, 0x0020)
#define XN_SYS_LAST                     XNID(0, _XNAREA_SYSTEM, 0x0023)


//
// Live notifications
//

#define XN_LIVE_FIRST                   XNID(0, _XNAREA_LIVE, 0x0001)
#define XN_LIVE_CONNECTIONCHANGED       XNID(0, _XNAREA_LIVE, 0x0001)
#define XN_LIVE_INVITE_ACCEPTED         XNID(0, _XNAREA_LIVE, 0x0002)
#define XN_LIVE_LINK_STATE_CHANGED      XNID(0, _XNAREA_LIVE, 0x0003)
#define XN_LIVE_CONTENT_INSTALLED       XNID(0, _XNAREA_LIVE, 0x0007)
#define XN_LIVE_MEMBERSHIP_PURCHASED    XNID(0, _XNAREA_LIVE, 0x0008)
#define XN_LIVE_VOICECHAT_AWAY          XNID(0, _XNAREA_LIVE, 0x0009)
#define XN_LIVE_PRESENCE_CHANGED        XNID(0, _XNAREA_LIVE, 0x000A)
#define XN_LIVE_LAST                    XNID(XNID_CURRENTVERSION+1, _XNAREA_LIVE, 0x0014)
// TODO
#define XN_LIVE_CONTENT_CHANGED ?

//
// Friends notifications
//

#define XN_FRIENDS_FIRST                XNID(0, _XNAREA_FRIENDS, 0x0001)
#define XN_FRIENDS_PRESENCE_CHANGED     XNID(0, _XNAREA_FRIENDS, 0x0001)
#define XN_FRIENDS_FRIEND_ADDED         XNID(0, _XNAREA_FRIENDS, 0x0002)
#define XN_FRIENDS_FRIEND_REMOVED       XNID(0, _XNAREA_FRIENDS, 0x0003)
#define XN_FRIENDS_LAST                 XNID(XNID_CURRENTVERSION+1, _XNAREA_FRIENDS, 0x0009)

//
// Custom notifications
//

#define XN_CUSTOM_FIRST                 XNID(0, _XNAREA_CUSTOM, 0x0001)
#define XN_CUSTOM_ACTIONPRESSED         XNID(0, _XNAREA_CUSTOM, 0x0003)
#define XN_CUSTOM_GAMERCARD             XNID(1, _XNAREA_CUSTOM, 0x0004)
#define XN_CUSTOM_LAST                  XNID(XNID_CURRENTVERSION+1, _XNAREA_CUSTOM, 0x0005)


//
// XMP notifications
//

#define XN_XMP_FIRST                                     XNID(0, _XNAREA_XMP, 0x0001)
#define XN_XMP_STATECHANGED                              XNID(0, _XNAREA_XMP, 0x0001)
#define XN_XMP_PLAYBACKBEHAVIORCHANGED                   XNID(0, _XNAREA_XMP, 0x0002)
#define XN_XMP_PLAYBACKCONTROLLERCHANGED                 XNID(0, _XNAREA_XMP, 0x0003)
#define XN_XMP_LAST                                      XNID(XNID_CURRENTVERSION+1, _XNAREA_XMP, 0x000D)


//
// Party notifications
//

#define XN_PARTY_FIRST                                   XNID(0, _XNAREA_PARTY, 0x0001)
#define XN_PARTY_MEMBERS_CHANGED                         XNID(4, _XNAREA_PARTY, 0x0002)
#define XN_PARTY_LAST                                    XNID(XNID_CURRENTVERSION+1, _XNAREA_PARTY, 0x0006)


//
// Popup Notifications
//
#define XNOTIFYUI_POS_CENTER		0
#define XNOTIFYUI_POS_TOPCENTER		0b0001
#define XNOTIFYUI_POS_BOTTOMCENTER	0b0010
#define XNOTIFYUI_POS_CENTERLEFT	0b0100
#define XNOTIFYUI_POS_CENTERRIGHT	0b1000
#define XNOTIFYUI_POS_TOPLEFT		(XNOTIFYUI_POS_TOPCENTER | XNOTIFYUI_POS_CENTERLEFT)
#define XNOTIFYUI_POS_TOPRIGHT		(XNOTIFYUI_POS_TOPCENTER | XNOTIFYUI_POS_CENTERRIGHT)
#define XNOTIFYUI_POS_BOTTOMLEFT	(XNOTIFYUI_POS_BOTTOMCENTER | XNOTIFYUI_POS_CENTERLEFT)
#define XNOTIFYUI_POS_BOTTOMRIGHT	(XNOTIFYUI_POS_BOTTOMCENTER | XNOTIFYUI_POS_CENTERRIGHT)
#define XNOTIFYUI_POS_MASK			0b1111


// Deprecated. Use XCUSTOMACTION_FLAG_CLOSES_GUIDE instead.
#define CUSTOMACTION_FLAG_CLOSESUI      1

typedef enum
{
	XMSG_FLAG_DISABLE_EDIT_RECIPIENTS = 0x00000001
} XMSG_FLAGS;

#define XMSG_MAX_CUSTOM_IMAGE_SIZE      (36*1024)

typedef enum
{
	XCUSTOMACTION_FLAG_CLOSES_GUIDE = 0x00000001,
	XCUSTOMACTION_FLAG_DELETES_MESSAGE = 0x00000002
} XCUSTOMACTION_FLAGS;

#define XMSG_MAX_CUSTOMACTION_TRANSLATIONS      11


#define XCUSTOMACTION_MAX_PAYLOAD_SIZE  1024

#define XPLAYERLIST_CUSTOMTEXT_MAX_LENGTH   31
#define XPLAYERLIST_TITLE_MAX_LENGTH        36
#define XPLAYERLIST_DESCRIPTION_MAX_LENGTH  83
#define XPLAYERLIST_IMAGE_MAX_SIZE          36864
#define XPLAYERLIST_MAX_PLAYERS             100
#define XPLAYERLIST_BUTTONTEXT_MAX_LENGTH   23

typedef enum
{
    XPLAYERLIST_BUTTON_TYPE_TITLECUSTOM          = 0,
    XPLAYERLIST_BUTTON_TYPE_PLAYERREVIEW, // The button says "Submit Player Review." The button is handled by the system and UI is not dismissed.
    XPLAYERLIST_BUTTON_TYPE_GAMEINVITE, // The button says "Invite to Game." The button press is handled by the system and UI is not dismissed.
    XPLAYERLIST_BUTTON_TYPE_MESSAGE, // The button says "Send Message." The button press is handled by the system and UI is not dismissed. 
    XPLAYERLIST_BUTTON_TYPE_FRIENDREQUEST, // The button says "Send Friend Request." The button press is handled by the system and UI is not dismissed.
    XPLAYERLIST_BUTTON_TYPE_TITLECUSTOMGLOBAL, // The title is specifying an action applying to all users in the custom player list. The title specifies its own button text and handles the button pressed action.
    XPLAYERLIST_BUTTON_TYPE_TITLECUSTOMINDIVIDUAL, // The title is specifying an action applying to a single users in the custom player list. The title specifies its own button text and handles the button pressed action.
} XPLAYERLIST_BUTTON_TYPE;

typedef enum
{
	XPLAYERLIST_FLAG_CUSTOMTEXT = 0x00000001,
} XPLAYERLIST_FLAGS;


// API to show error and informational messages

#define XMB_NOICON                      0x00000000
#define XMB_ERRORICON                   0x00000001
#define XMB_WARNINGICON                 0x00000002
#define XMB_ALERTICON                   0x00000003
#define XMB_ICON_MASK                   0x00000003

#define XMB_PASSCODEMODE                0x00010000
#define XMB_VERIFYPASSCODEMODE          0x00020000
#define XMB_MODE_MASK                   0x000F0000

#define XMB_MAXBUTTONS                  3
#define XMB_OK 1
#define XMB_CANCEL 2
#define XMB_CANCELID                    -1

#define VKBD_DEFAULT                    0x00000000 // Character set selected in the Guide.
#define VKBD_LATIN_FULL                 0x00000001
#define VKBD_LATIN_EMAIL                0x00000002
#define VKBD_LATIN_GAMERTAG             0x00000004
#define VKBD_LATIN_PHONE                0x00000008
#define VKBD_LATIN_IP_ADDRESS           0x00000010
#define VKBD_LATIN_NUMERIC              0x00000020
#define VKBD_LATIN_ALPHABET             0x00000040
#define VKBD_LATIN_PASSWORD             0x00000080 // Subset of Latin character set appropriate for valid passwords.
#define VKBD_LATIN_SUBSCRIPTION         0x00000100
#define VKBD_JAPANESE_FULL              0x00001000
#define VKBD_KOREAN_FULL                0x00002000
#define VKBD_TCH_FULL                   0x00004000
#define VKBD_RUSSIAN_FULL               0x00008000
// VKBD_LATIN_EXTENDED provides support for Eastern and Central European
// characters
#define VKBD_LATIN_EXTENDED             0x00010000
#define VKBD_SCH_FULL                   0x00020000
#define VKBD_JAPANESE_HIRAGANA          0x00040000
#define VKBD_JAPANESE_KATAKANA          0x00080000
// VKBD_GREEK_FULL and VKBD_CSH_FULL are LiveSignup-Only keyboards
#define VKBD_GREEK_FULL                 0x00100000
#define VKBD_CSH_FULL                   0x00200000 // Czech, Slovak, Hungarian

#define VKBD_SELECT_OK                  0x10000000
#define VKBD_HIGHLIGHT_TEXT             0x20000000 // When set, outputs highlighted characters.
#define VKBD_MULTILINE                  0x40000000 // When set, enables multi-line mode. When not set, the return key is considered the OK button.
#define VKBD_ENABLEIME                  0x80000000 // When set, enables the input method editor on the keyboard.


// Column ids for skill leaderboards (STATS_VIEW_SKILL_* views)
#define X_STATS_COLUMN_SKILL_SKILL              61
#define X_STATS_COLUMN_SKILL_GAMESPLAYED        62
#define X_STATS_COLUMN_SKILL_MU                 63
#define X_STATS_COLUMN_SKILL_SIGMA              64

#define X_STATS_SKILL_SKILL_DEFAULT             1
#define X_STATS_SKILL_MU_DEFAULT                3.0
#define X_STATS_SKILL_SIGMA_DEFAULT             1.0

#define X_STATS_COLUMN_ATTACHMENT_SIZE          ((uint16_t)0xFFFA)

// View id used to write to skill leaderboard
#define X_STATS_VIEW_SKILL                      0xFFFF0000

#define X_STATS_MAX_VIEWS                       64
#define X_STATS_MAX_PROPERTIES_IN_VIEW          64
#define X_STATS_MAX_USER_COUNT                  101
#define X_STATS_MAX_ROW_COUNT                   100

#define XUSER_STATS_ATTRS_IN_SPEC       64

#define MAX_FIRSTNAME_SIZE                               64
#define MAX_LASTNAME_SIZE                                64
#define MAX_EMAIL_SIZE                                   129
#define MAX_STREET_SIZE                                  128
#define MAX_CITY_SIZE                                    64
#define MAX_DISTRICT_SIZE                                64
#define MAX_STATE_SIZE                                   64
#define MAX_COUNTRYCODE_SIZE                             2
#define MAX_POSTALCODE_SIZE                              16
#define MAX_CC_TYPE_SIZE                                 32
#define MAX_CC_EXPIRATION_SIZE                           6

// System-defined contexts and properties

#define X_PROPERTY_TYPE_MASK            0xF0000000
#define X_PROPERTY_SCOPE_MASK           0x00008000
#define X_PROPERTY_ID_MASK              0x00007FFF

#define XPROPERTYID(global, type, id)   ((global ? X_PROPERTY_SCOPE_MASK : 0) | ((type << 28) & X_PROPERTY_TYPE_MASK) | (id & X_PROPERTY_ID_MASK))
#define XCONTEXTID(global, id)          XPROPERTYID(global, XUSER_DATA_TYPE_CONTEXT, id)
#define XPROPERTYTYPEFROMID(id)         ((id >> 28) & 0xf)
#define XISSYSTEMPROPERTY(id)           (id & X_PROPERTY_SCOPE_MASK)

// Predefined contexts
#define X_CONTEXT_PRESENCE              XCONTEXTID(1, 0x1)
#define X_CONTEXT_GAME_TYPE             XCONTEXTID(1, 0xA)
#define X_CONTEXT_GAME_MODE             XCONTEXTID(1, 0xB)
#define X_CONTEXT_SESSION_JOINABLE      XCONTEXTID(1, 0xC)

#define X_PROPERTY_RANK                 XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0x1)
#define X_PROPERTY_GAMERNAME            XPROPERTYID(1, XUSER_DATA_TYPE_UNICODE, 0x2)
#define X_PROPERTY_SESSION_ID           XPROPERTYID(1, XUSER_DATA_TYPE_INT64,   0x3)

// System attributes used in matchmaking queries
#define X_PROPERTY_GAMER_ZONE           XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0x101)
#define X_PROPERTY_GAMER_COUNTRY        XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0x102)
#define X_PROPERTY_GAMER_LANGUAGE       XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0x103)
#define X_PROPERTY_GAMER_RATING         XPROPERTYID(1, XUSER_DATA_TYPE_FLOAT,   0x104)
#define X_PROPERTY_GAMER_MU             XPROPERTYID(1, XUSER_DATA_TYPE_DOUBLE,  0x105)
#define X_PROPERTY_GAMER_SIGMA          XPROPERTYID(1, XUSER_DATA_TYPE_DOUBLE,  0x106)
#define X_PROPERTY_GAMER_PUID           XPROPERTYID(1, XUSER_DATA_TYPE_INT64,   0x107)
#define X_PROPERTY_AFFILIATE_SCORE      XPROPERTYID(1, XUSER_DATA_TYPE_INT64,   0x108)
#define X_PROPERTY_GAMER_HOSTNAME       XPROPERTYID(1, XUSER_DATA_TYPE_UNICODE, 0x109)

// Properties used to write to skill leaderboards
#define X_PROPERTY_RELATIVE_SCORE                   XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0xA)
#define X_PROPERTY_SESSION_TEAM                     XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0xB)

// Properties written at the session level to override TrueSkill parameters
#define X_PROPERTY_PLAYER_PARTIAL_PLAY_PERCENTAGE   XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0xC)
#define X_PROPERTY_PLAYER_SKILL_UPDATE_WEIGHTING_FACTOR XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0xD)
#define X_PROPERTY_SESSION_SKILL_BETA               XPROPERTYID(1, XUSER_DATA_TYPE_DOUBLE,  0xE)
#define X_PROPERTY_SESSION_SKILL_TAU                XPROPERTYID(1, XUSER_DATA_TYPE_DOUBLE,  0xF)
#define X_PROPERTY_SESSION_SKILL_DRAW_PROBABILITY   XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0x10)

// Attachment size is written to a leaderboard when the entry qualifies for
// a gamerclip.  The rating can be retrieved via XUserEstimateRankForRating.
#define X_PROPERTY_ATTACHMENT_SIZE                  XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0x11)

// Values for X_CONTEXT_GAME_TYPE
#define X_CONTEXT_GAME_TYPE_RANKED      0
#define X_CONTEXT_GAME_TYPE_STANDARD    1



#define XLOCATOR_DEDICATEDSERVER_PROPERTY_START     0x200

// These properties are used for search only.
// The search result header should already contains the information, and the query should not request these properties again.
#define X_PROPERTY_DEDICATEDSERVER_IDENTITY                 XPROPERTYID(1, XUSER_DATA_TYPE_INT64,  XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_ServerIdentity)   // server id. supports '=' operator onl$
#define X_PROPERTY_DEDICATEDSERVER_TYPE                     XPROPERTYID(1, XUSER_DATA_TYPE_INT32,  XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_ServerType)
#define X_PROPERTY_DEDICATEDSERVER_MAX_PUBLIC_SLOTS         XPROPERTYID(1, XUSER_DATA_TYPE_INT32,  XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_MaxPublicSlots)
#define X_PROPERTY_DEDICATEDSERVER_MAX_PRIVATE_SLOTS        XPROPERTYID(1, XUSER_DATA_TYPE_INT32,  XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_MaxPrivateSlots)
#define X_PROPERTY_DEDICATEDSERVER_AVAILABLE_PUBLIC_SLOTS   XPROPERTYID(1, XUSER_DATA_TYPE_INT32,  XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_AvailablePublicSlots)
#define X_PROPERTY_DEDICATEDSERVER_AVAILABLE_PRIVATE_SLOTS  XPROPERTYID(1, XUSER_DATA_TYPE_INT32,  XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_AvailablePrivateSlots)
#define X_PROPERTY_DEDICATEDSERVER_FILLED_PUBLIC_SLOTS      XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_FilledPublicSlots)
#define X_PROPERTY_DEDICATEDSERVER_FILLED_PRIVATE_SLOTS     XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_FilledPrivateSlots)


// the following properties only support XTS_FILTER_COMPARE_OPERATOR_Equals operator
#define X_PROPERTY_DEDICATEDSERVER_OWNER_XUID           XPROPERTYID(1, XUSER_DATA_TYPE_INT64,   XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_OwnerXuid)
#define X_PROPERTY_DEDICATEDSERVER_OWNER_GAMERTAG       XPROPERTYID(1, XUSER_DATA_TYPE_UNICODE,   XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_OwnerGamerTag)
#define X_PROPERTY_DEDICATEDSERVER_REGIONID             XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_RegionID)
#define X_PROPERTY_DEDICATEDSERVER_LANGUAGEID           XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_LanguageID)

#define XUSER_PROPERTY_PLATFORMTYPE		0x10008201
#define XUSER_PROPERTY_PLATFORMLOCK		0x10008202
#define XUSER_PROPERTY_USER_INT				0x10008202
#define XUSER_PROPERTY_MAP_ID				0x10008207
#define XUSER_PROPERTY_UNKNOWN_INT32_1		0x10008208
#define XUSER_PROPERTY_GAMETYPE_ID			0x10008209
#define XUSER_PROPERTY_UNKNOWN_INT32_2		0x1000820B
#define XUSER_PROPERTY_UNKNOWN_INT32_3		0x1000820C
#define XUSER_PROPERTY_UNKNOWN_INT32_4		0x1000820D
#define XUSER_PROPERTY_PARTY_PRIVACY		0x10008210
#define XUSER_PROPERTY_UNKNOWN_INT32_6		0x10008212
#define XUSER_PROPERTY_UNKNOWN_INT32_7		0x10008213
#define XUSER_PROPERTY_MAP_ID_2				0x1000820A
#define XUSER_PROPERTY_VERSION_1			0x1000820E
#define XUSER_PROPERTY_VERSION_2			0x1000820F
#define XUSER_PROPERTY_GAME_STATUS			0x10008211
#define XUSER_PROPERTY_SERVER_DESC			0x40008225
#define XUSER_PROPERTY_MAP_NAME				0x40008226
#define XUSER_PROPERTY_GAMETYPE_NAME		0x40008228
#define XUSER_PROPERTY_UNKNOWN_INT64		0x2000821B
#define XUSER_PROPERTY_MAP_HASH_1			0x40008227
#define XUSER_PROPERTY_MAP_NAME_2			0x40008229
#define XUSER_PROPERTY_MAP_HASH_2			0x4000822A
#define XUSER_PROPERTY_GAMETYPE_NAME_2		0x4000822B
#define XUSER_PROPERTY_USERNAME_2			0x4000822C
#define XUSER_PROPERTY_SERVER_NAME			0x40008230


//Achievements

#define XACHIEVEMENT_MAX_COUNT 200

// These lengths include the NULL-terminator
#define XACHIEVEMENT_MAX_LABEL_LENGTH   32
#define XACHIEVEMENT_MAX_DESC_LENGTH    100
#define XACHIEVEMENT_MAX_UNACH_LENGTH   100

#define XACHIEVEMENT_SIZE_BASE          (sizeof(XACHIEVEMENT_DETAILS))
#define XACHIEVEMENT_SIZE_STRINGS       (sizeof(wchar_t) * (XACHIEVEMENT_MAX_LABEL_LENGTH  + XACHIEVEMENT_MAX_DESC_LENGTH + XACHIEVEMENT_MAX_UNACH_LENGTH))
#define XACHIEVEMENT_SIZE_FULL          (XACHIEVEMENT_SIZE_BASE + XACHIEVEMENT_SIZE_STRINGS)

#define XACHIEVEMENT_INVALID_ID         ((DWORD)0xFFFFFFFF)

// XACHIEVEMENT_DETAILS::dwFlags can be manipulated with these defines and macros
#define XACHIEVEMENT_DETAILS_MASK_TYPE          0x00000007
#define XACHIEVEMENT_DETAILS_SHOWUNACHIEVED     0x00000008
#define XACHIEVEMENT_DETAILS_ACHIEVED_ONLINE    0x00010000
#define XACHIEVEMENT_DETAILS_ACHIEVED           0x00020000

#define AchievementType(dwFlags)           (dwFlags & XACHIEVEMENT_DETAILS_MASK_TYPE)
#define AchievementShowUnachieved(dwFlags) (dwFlags & XACHIEVEMENT_DETAILS_SHOWUNACHIEVED ? TRUE : FALSE)
#define AchievementEarnedOnline(dwFlags)   (dwFlags & XACHIEVEMENT_DETAILS_ACHIEVED_ONLINE ? TRUE : FALSE)
#define AchievementEarned(dwFlags)         (dwFlags & XACHIEVEMENT_DETAILS_ACHIEVED ? TRUE : FALSE)

// Types returned from AchievementType macro

#define XACHIEVEMENT_TYPE_COMPLETION            1
#define XACHIEVEMENT_TYPE_LEVELING              2
#define XACHIEVEMENT_TYPE_UNLOCK                3
#define XACHIEVEMENT_TYPE_EVENT                 4
#define XACHIEVEMENT_TYPE_TOURNAMENT            5
#define XACHIEVEMENT_TYPE_CHECKPOINT            6
#define XACHIEVEMENT_TYPE_OTHER                 7

#define XACHIEVEMENT_DETAILS_ALL                0xFFFFFFFF
#define XACHIEVEMENT_DETAILS_LABEL              0x00000001
#define XACHIEVEMENT_DETAILS_DESCRIPTION        0x00000002
#define XACHIEVEMENT_DETAILS_UNACHIEVED         0x00000004
#define XACHIEVEMENT_DETAILS_TFC                0x00000020

#define XSTRING_MAX_LENGTH 512
#define XSTRING_MAX_STRINGS 10


#define XFRIENDS_MAX_C_RESULT 100


// Predefined dedicated server types
#define XLOCATOR_SERVERTYPE_PUBLIC          0   // dedicated server is for all players.
#define XLOCATOR_SERVERTYPE_GOLD_ONLY       1   // dedicated server is for Gold players only.
#define XLOCATOR_SERVERTYPE_PEER_HOSTED     2   // dedicated server is a peer-hosted game server.
#define XLOCATOR_SERVERTYPE_PEER_HOSTED_GOLD_ONLY   3   // dedicated server is a peer-hosted game server (gold only).
#define XLOCATOR_SERVICESTATUS_PROPERTY_START     0x100

#define XLMGRCREDS_FLAG_SAVE 1
#define XLMGRCREDS_FLAG_DELETE 2

#define XLSIGNIN_FLAG_SAVECREDS 1
#define XLSIGNIN_FLAG_ALLOWTITLEUPDATES 2
#define XLSIGNIN_FLAG_ALLOWSYSTEMUPDATES 4


#define XAUDIOFXID_MIN          0
#define XAUDIOFXID_MAX          0xFF

#define XAUDIOFXID_STATICMIN    XAUDIOFXID_MIN
#define XAUDIOFXID_STATICMAX    0x7F
#define XAUDIOFXID_STATICCOUNT  (XAUDIOFXID_STATICMAX - XAUDIOFXID_STATICMIN + 1)

#define XAUDIOFXID_DYNMIN       0x80
#define XAUDIOFXID_DYNMAX       XAUDIOFXID_MAX
#define XAUDIOFXID_DYNCOUNT     (XAUDIOFXID_DYNMAX - XAUDIOFXID_DYNMIN + 1)

//
// The XHV worker thread will initially run on this processor
//

#define XHV_WORKER_THREAD_PROCESSOR             (1)
#define XHV_STACK_SIZE                          (0x10000)   // 64KB

#define XHV_MAX_REMOTE_TALKERS                  (30)
//#define XHV_MAX_LOCAL_TALKERS                   (1) // GFWL
//#define XHV_MAX_LOCAL_TALKERS                   (4) // XBOX
#define XHV_MAX_LOCAL_TALKERS                   (XLIVE_LOCAL_USER_COUNT)

//
// At the moment, you may only have up to this many enabled processing modes
// for local and remote talkers (separately).
//

#define XHV_MAX_PROCESSING_MODES                (2)

//
// When setting playback priorities, talker pairs between
// XHV_PLAYBACK_PRIORITY_MAX and XHV_PLAYBACK_PRIORITY_MIN, inclusive, will be
// heard, while any other value will result in muting that pair.
//

#define XHV_PLAYBACK_PRIORITY_MAX               (0)
#define XHV_PLAYBACK_PRIORITY_MIN               (0xFFFF)
#define XHV_PLAYBACK_PRIORITY_NEVER             (0xFFFFFFFF)

//
// Each packet reported by voice chat mode is the following size (including the
// XHV_CODEC_HEADER)
//

//#define XHV_VOICECHAT_MODE_PACKET_SIZE          ?
#define XHV_VOICECHAT_MODE_PACKET_SIZE_OLD      (10)
#define XHV_VOICECHAT_MODE_PACKET_SIZE_NEW      (42)

//
// When supplying a buffer to GetLocalChatData, you won't have to supply a
// buffer any larger than the following number of packets (or
// XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE bytes)
//

#define XHV_MAX_VOICECHAT_PACKETS               (10)

//
// XHV's final output submix voice consists of one channel per local talker.
// Each channel contains the mixed voice for its user index.  The data can be
// mapped to speakers or headsets.
//

#define XHV_XAUDIO_OUTPUT_CHANNEL_COUNT         (XHV_MAX_LOCAL_TALKERS)

//
// XHV uses three stages of submix voices.  Hence, XAudio's maximum stage count
// must be set to at least this value.
//

#define XHV_XAUDIO_SUBMIX_STAGE_COUNT           (3)

//
// XHV requires the use of a custom XAudio effect.  This effect has the
// following Effect ID.  Title effects should not use this ID.
//

#define XHV_XAUDIO_OUTPUT_EFFECTID              (XAUDIOFXID_STATICMAX)

//
// The number of effects in any chain passed into XHV is limited to this many.
//

#define XHV_MAX_FXCHAIN_EFFECTS                 (1)

//
// The microphone callback is given PCM data in this format.  Note that XAudio
// effect chains will process the data in the native XAudio format (see XAudio
// for details).
//

#define XHV_PCM_BYTES_PER_SAMPLE                (2)
#define XHV_PCM_SAMPLE_RATE                     (16000)

//
// Data Ready Flags.  These flags are set when there is local data waiting to be
// consumed (e.g. through GetLocalChatData).  GetLocalDataFlags() allows you to
// get the current state of these flags without entering XHV's critical section.
// Each mask is 4 bits, one for each local talker.  The least significant bit in
// each section indicates data is available for user index 0, while the most
// significant bit indicates user index 3.
//

#define XHV_VOICECHAT_DATA_READY_MASK           (0xF)
#define XHV_VOICECHAT_DATA_READY_OFFSET         (0)

//
// Typedefs, Enums and Structures
//

//typedef const void*                            XHV_PROCESSING_MODE, *PXHV_PROCESSING_MODE;
typedef DWORD                                   XHV_PROCESSING_MODE, *PXHV_PROCESSING_MODE;

typedef DWORD                                   XHV_PLAYBACK_PRIORITY;
// FIXME need to confirm if it is stdcall and not something like cdecl and if the data is meant to be in big or little-endian format (currently little).
typedef void (__stdcall* PFNMICRAWDATAREADY)(
	uint32_t user_index
	, void* pcm_data
	, size_t pcm_data_size
	, BOOL* voice_detected
);

typedef DWORD                                   XHV_LOCK_TYPE;

#define XHV_LOCK_TYPE_LOCK                      0
#define XHV_LOCK_TYPE_TRYLOCK                   1
#define XHV_LOCK_TYPE_UNLOCK                    2
#define XHV_LOCK_TYPE_COUNT                     3

typedef void* XAUDIOVOICEFXCHAIN;
typedef void* XAUDIOSUBMIXVOICE;

//
// Supported processing modes
//

//extern  const void*                            _xhv_loopback_mode;
//extern  const void*                            _xhv_voicechat_mode;

//#define XHV_LOOPBACK_MODE                       _xhv_loopback_mode
//#define XHV_VOICECHAT_MODE                      _xhv_voicechat_mode

#define XHV_LOOPBACK_MODE                       1
#define XHV_VOICECHAT_MODE                      2


#define XLIVE_INITFLAG_USE_ADAPTER_NAME 1
#define XLIVE_INITFLAG_NO_AUTO_LOGON 2
#define XLIVE_INITFLAG_USE_XLIVEINPUT ?

typedef struct _XLIVE_INITIALIZE_INFO {
	UINT cbSize;
	DWORD dwFlags;
	IUnknown* pD3D;
	void* pD3DPP;
	LANGID langID;
	uint16_t wReserved1;
	char* pszAdapterName;
	uint16_t wLivePortOverride;
	uint16_t wReserved2;
} XLIVE_INITIALIZE_INFO;

typedef enum _XLIVE_DEBUG_LEVEL
{
	XLIVE_DEBUG_LEVEL_OFF = 0, // No debug output.
	XLIVE_DEBUG_LEVEL_ERROR, // Error only debug output.
	XLIVE_DEBUG_LEVEL_WARNING, // Warning and error debug output.
	XLIVE_DEBUG_LEVEL_INFO, // Information, warning and error debug output.
	XLIVE_DEBUG_LEVEL_DEFAULT // Default level of debug output, set as the current registry value.
} XLIVE_DEBUG_LEVEL;

typedef struct _XLIVE_INPUT_INFO {
	UINT cbSize;
	HWND hWnd;
	UINT uMSG;
	WPARAM wParam;
	LPARAM lParam;
	BOOL fHandled;
	LRESULT lRet;
} XLIVE_INPUT_INFO;

typedef struct _XLIVEUPDATE_INFORMATION {
	DWORD cbSize;
	BOOL bSystemUpdate;
	DWORD dwFromVersion;
	DWORD dwToVersion;
	wchar_t szUpdateDownloadPath[MAX_PATH];
} XLIVEUPDATE_INFORMATION, *PXLIVEUPDATE_INFORMATION;

#pragma pack(push, 1) // Save then set byte alignment setting.

typedef struct {
	size_t dwSize;
	// bData is first element of array of length/size dwSize.
	uint8_t bData;
} XLIVE_PROTECTED_BUFFER;

#pragma pack(pop) // Return to original alignment setting.

typedef struct _XLIVE_PROTECTED_DATA_INFORMATION {
	size_t cbSize;
	uint32_t dwFlags;
} XLIVE_PROTECTED_DATA_INFORMATION, *PXLIVE_PROTECTED_DATA_INFORMATION;

typedef struct
{

	//
	// Must be set to sizeof(XNetStartupParams).  There is no default.
	//
	uint8_t        cfgSizeOfStruct;

	//
	// One or more of the XNET_STARTUP_xxx flags OR'd together.
	//
	// The default is 0 (no flags specified).
	uint8_t        cfgFlags;

	//
	// The maximum number of SOCK_DGRAM (UDP or VDP) sockets that can be
	// opened at once.
	//
	// The default is 8 sockets.
	//
	uint8_t        cfgSockMaxDgramSockets;

	//
	// The maximum number of SOCK_STREAM (TCP) sockets that can be opened at
	// once, including those sockets created as a result of incoming connection
	// requests.  Remember that a TCP socket may not be closed immediately
	// after 'closesocket' is called, depending on the linger options in place
	// (by default a TCP socket will linger).
	//
	// The default is 32 sockets.
	//
	uint8_t        cfgSockMaxStreamSockets;

	//
	// The default receive buffer size for a socket, in units of K (1024 bytes).
	//
	// The default is 16 units (16K).
	//
	uint8_t        cfgSockDefaultRecvBufsizeInK;

	//
	// The default send buffer size for a socket, in units of K (1024 bytes).
	//
	// The default is 16 units (16K).
	//
	uint8_t        cfgSockDefaultSendBufsizeInK;

	//
	// The maximum number of XNKID / XNKEY pairs that can be registered at the
	// same time by calling XNetRegisterKey.
	//
	// The default is 8 key pair registrations.
	//
	uint8_t        cfgKeyRegMax;

	//
	// The maximum number of security associations that can be registered at
	// the same time.  Security associations are created for each unique
	// XNADDR / XNKID pair passed to XNetXnAddrToInAddr.  Security associations
	// are also implicitly created for each secure host that establishes an
	// incoming connection with this host on a given registered XNKID.  Note
	// that there will only be one security association between a pair of hosts
	// on a given XNKID no matter how many sockets are actively communicating
	// on that secure connection.
	//
	// The default is 32 security associations.
	//
	uint8_t        cfgSecRegMax;

	//
	// The maximum amount of QoS data, in units of DWORD (4 bytes), that can be
	// supplied to a call to XNetQosListen or returned in the result set of a
	// call to XNetQosLookup.
	//
	// The default is 64 (256 bytes).
	//
	uint8_t        cfgQosDataLimitDiv4;

	//
	// The amount of time to wait for a response after sending a QoS packet
	// before sending it again (or giving up).  This should be set to the same
	// value on clients (XNetQosLookup callers) and servers (XNetQosListen
	// callers).
	//
	// The default is 2 seconds.
	//
	uint8_t        cfgQosProbeTimeoutInSeconds;

	//
	// The maximum number of times to retry a given QoS packet when no response
	// is received.  This should be set to the same value on clients
	// (XNetQosLookup callers) and servers (XNetQosListen callers).
	//
	// The default is 3 retries.
	//
	uint8_t        cfgQosProbeRetries;

	//
	// The maximum number of simultaneous QoS lookup responses that a QoS
	// listener supports.  Note that the bandwidth throttling parameter passed
	// to XNetQosListen may impact the number of responses queued, and thus
	// affects how quickly this limit is reached.
	//
	// The default is 8 responses.
	//
	uint8_t        cfgQosSrvMaxSimultaneousResponses;

	//
	// The maximum amount of time for QoS listeners to wait for the second
	// packet in a packet pair.
	//
	// The default is 2 seconds.
	//
	uint8_t        cfgQosPairWaitTimeInSeconds;

} XNetStartupParams;

typedef struct
{
	IN_ADDR     ina;                            // IP address (zero if not static/DHCP)
	IN_ADDR     inaOnline;                      // Secure Addr. Online IP address (zero if not online)
	uint16_t        wPortOnline;                    // Online port
	uint8_t        abEnet[6];                      // Ethernet MAC address
	uint8_t        abOnline[20];                   // Online identification
} XNADDR;

typedef struct
{
	uint8_t        ab[8];                          // xbox to xbox key identifier
} XNKID;

typedef XNADDR TSADDR;

typedef enum : uint32_t
{
	XONLINE_NAT_OPEN = 1,
	XONLINE_NAT_MODERATE,
	XONLINE_NAT_STRICT
} XONLINE_NAT_TYPE;

#pragma pack(push, 4)
typedef struct _XONLINE_SERVICE_INFO
{
    uint32_t          dwServiceID;
    IN_ADDR        serviceIP;
    uint16_t           wServicePort;
    uint16_t           wReserved;
} XONLINE_SERVICE_INFO, *PXONLINE_SERVICE_INFO;
#pragma pack(pop)

typedef enum {
	XONLINE_NOTIFICATION_EVENT_SERVICE,
	NUM_XONLINE_NOTIFICATION_EVENT_TYPES
} XONLINE_NOTIFICATION_EVENT_TYPE;

typedef struct {
	XONLINE_NOTIFICATION_EVENT_TYPE     type;
	union {
		struct {
			DWORD                       dwCode;
		} service;
	} info;
} XONLINE_NOTIFICATION_MSG, *PXONLINE_NOTIFICATION_MSG;

#pragma pack(push, 1)

typedef struct _XONLINE_FRIEND
{
    XUID                    xuid;
    char                    szGamertag[XUSER_NAME_SIZE];
    DWORD                   dwFriendState;
    XNKID                   sessionID;
    DWORD                   dwTitleID;
    FILETIME                ftUserTime;
    XNKID                   xnkidInvite;
    FILETIME                gameinviteTime;
    DWORD                   cchRichPresence;
    wchar_t                   wszRichPresence[MAX_RICHPRESENCE_SIZE];
} XONLINE_FRIEND, *PXONLINE_FRIEND;

typedef struct _XONLINE_PRESENCE
{
    XUID                    xuid;
    DWORD                   dwState;
    XNKID                   sessionID;
    DWORD                   dwTitleID;
    FILETIME                ftUserTime;
    DWORD                   cchRichPresence;
    wchar_t                   wszRichPresence[MAX_RICHPRESENCE_SIZE];
} XONLINE_PRESENCE, *PXONLINE_PRESENCE;

#pragma pack(pop)

typedef struct _XTITLE_SERVER_INFO {
	// Actual IP address.
	IN_ADDR inaServer;
	DWORD dwFlags;
	char szServerInfo[XTITLE_SERVER_MAX_SERVER_INFO_SIZE];
} XTITLE_SERVER_INFO, *PXTITLE_SERVER_INFO;

typedef struct
{
	uint8_t        ab[16];                         // xbox to xbox key exchange key
} XNKEY;

typedef struct
{
	INT         iStatus;                        // WSAEINPROGRESS if pending; 0 if success; error if failed
	UINT        cina;                           // Count of IP addresses for the given host
	IN_ADDR     aina[8];                        // Vector of IP addresses for the given host
} XNDNS;

typedef struct
{
	uint8_t        bFlags;                         // See XNET_XNQOSINFO_*
	uint8_t        bReserved;                      // Reserved
	uint16_t        cProbesXmit;                    // Count of Qos probes transmitted
	uint16_t        cProbesRecv;                    // Count of Qos probes successfully received
	uint16_t        cbData;                         // Size of Qos data supplied by target (may be zero)
	uint8_t*       pbData;                         // Qos data supplied by target (may be NULL)
	uint16_t        wRttMinInMsecs;                 // Minimum round-trip time in milliseconds
	uint16_t        wRttMedInMsecs;                 // Median round-trip time in milliseconds
	DWORD       dwUpBitsPerSec;                 // Upstream bandwidth in bits per second
	DWORD       dwDnBitsPerSec;                 // Downstream bandwidth in bits per second
} XNQOSINFO;

typedef struct
{
	size_t        cxnqos;                         // Count of items in axnqosinfo[] array
	size_t        cxnqosPending;                  // Count of items still pending
	XNQOSINFO   axnqosinfo[1];                  // Vector of Qos results
} XNQOS;

typedef struct
{
	DWORD       dwSizeOfStruct;                 // Structure size, must be set prior to calling XNetQosGetListenStats
	DWORD       dwNumDataRequestsReceived;      // Number of client data request probes received
	DWORD       dwNumProbesReceived;            // Number of client probe requests received
	DWORD       dwNumSlotsFullDiscards;         // Number of client requests discarded because all slots are full
	DWORD       dwNumDataRepliesSent;           // Number of data replies sent
	DWORD       dwNumDataReplyBytesSent;        // Number of data reply bytes sent
	DWORD       dwNumProbeRepliesSent;          // Number of probe replies sent
} XNQOSLISTENSTATS;

typedef struct _XUSER_DATA
{
	uint8_t                                type;

	union
	{
		LONG                            nData;     // XUSER_DATA_TYPE_INT32
		LONGLONG                        i64Data;   // XUSER_DATA_TYPE_INT64
		double                          dblData;   // XUSER_DATA_TYPE_DOUBLE
		struct                                     // XUSER_DATA_TYPE_UNICODE
		{
			size_t                       cbData;    // Includes null-terminator
			LPWSTR                      pwszData;
		} string;
		FLOAT                           fData;     // XUSER_DATA_TYPE_FLOAT
		struct                                     // XUSER_DATA_TYPE_BINARY
		{
			size_t                       cbData;
			PBYTE                       pbData;
		} binary;
		FILETIME                        ftData;    // XUSER_DATA_TYPE_DATETIME
	};
} XUSER_DATA, *PXUSER_DATA;

typedef struct _XUSER_PROPERTY
{
	DWORD                               dwPropertyId;
	XUSER_DATA                          value;
} XUSER_PROPERTY, *PXUSER_PROPERTY;

typedef enum _XUSER_SIGNIN_STATE : uint32_t
{
	eXUserSigninState_NotSignedIn,
	eXUserSigninState_SignedInLocally,
	eXUserSigninState_SignedInToLive
} XUSER_SIGNIN_STATE;

typedef struct {
	XUID                 xuid;
	DWORD                dwInfoFlags;
	XUSER_SIGNIN_STATE   UserSigninState;
	DWORD                dwGuestNumber;
	DWORD                dwSponsorUserIndex;
	char                 szUserName[XUSER_NAME_SIZE];
} XUSER_SIGNIN_INFO, *PXUSER_SIGNIN_INFO;

typedef struct _FIND_USER_INFO {
	XUID qwUserId;
	char szGamerTag;
} FIND_USER_INFO;

typedef struct _FIND_USERS_RESPONSE {
	size_t dwResults;
	FIND_USER_INFO* pUsers;
} FIND_USERS_RESPONSE;

typedef void (WINAPI* PXOVERLAPPED_COMPLETION_ROUTINE)(
	size_t dwErrorCode
	, size_t dwNumberOfBytesTransfered
	, struct _XOVERLAPPED* pOverlapped
);

typedef struct _XOVERLAPPED {
	// Main error code returned in XGetOverlappedResult.
	size_t                            InternalLow;
	// Typically length of data returned. Optional value returned in XGetOverlappedResult.
	size_t                            InternalHigh;
	// Internal use by xlive only.
	size_t                            InternalContext;
	// Signalable event for thread synchronisation.
	HANDLE                              hEvent;
	// Function pointer that is to be executed on task completion.
	PXOVERLAPPED_COMPLETION_ROUTINE     pCompletionRoutine;
	// Title use only to hold contextual memory for completion routine.
	size_t*                           dwCompletionContext;
	// Secondary error returned in XGetOverlappedExtendedError.
	size_t                            dwExtendedError;
} XOVERLAPPED, *PXOVERLAPPED;

#define XHasOverlappedIoCompleted(lpOverlapped) (*((volatile ULONG_PTR*)(&(lpOverlapped)->InternalLow)) != ERROR_IO_PENDING)

typedef struct _XUSER_PROFILE_SETTING
{
    XUSER_PROFILE_SOURCE    source;

    union                                   // Used only on read, ignored for write
    {
        DWORD               dwUserIndex;    // Valid only after local read
        XUID                xuid;           // Valid only after online read
    } user;

    DWORD                   dwSettingId;

    XUSER_DATA              data;
} XUSER_PROFILE_SETTING, *PXUSER_PROFILE_SETTING;

typedef struct _XUSER_READ_PROFILE_SETTING_RESULT
{
    size_t                  dwSettingsLen;
    XUSER_PROFILE_SETTING* pSettings;
} XUSER_READ_PROFILE_SETTING_RESULT, *PXUSER_READ_PROFILE_SETTING_RESULT;

#pragma pack(push, 1) // Save then set byte alignment setting.
typedef struct _XUSER_CONTEXT
{
	DWORD                               dwContextId;
	DWORD                               dwValue;
} XUSER_CONTEXT, *PXUSER_CONTEXT;
#pragma pack(pop) // Return to original alignment setting.

#pragma pack(push, 1) // Save then set byte alignment setting.
typedef struct {
	unsigned int id : 16;
	//unsigned int id : 14;
	//unsigned int unknown : 2; // These bits are not used afaik. Probably part of id. Could also be an 8 and 8 devide with some other important setting.
	unsigned int data_size : 12; // Is the exact data size, or the max (UNICODE, BINARY).
	unsigned int data_type : 4; // XUSER_DATA_TYPE_
} SETTING_ID;
#pragma pack(pop) // Return to original alignment setting.

typedef struct _XCONTENT_DATA {
	DWORD ContentNum;
	DWORD TitleId;
	DWORD ContentPackageType;
	uint8_t ContentId[20];
} XCONTENT_DATA, *PXCONTENT_DATA;

typedef struct _XLIVE_CONTENT_INFO {
	DWORD dwContentAPIVersion;
	DWORD dwTitleID;
	DWORD dwContentType;
	uint8_t abContentID[XLIVE_CONTENT_ID_SIZE];
} XLIVE_CONTENT_INFO, *PXLIVE_CONTENT_INFO;

typedef struct _XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS {
	DWORD cbSize;
	XLIVE_CONTENT_INSTALL_CALLBACK* pInstallCallback;
	void* pInstallCallbackContext;
	// idk function signature.
	void* pInstallCallbackEx;
} XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS, *PXLIVE_CONTENT_INSTALL_CALLBACK_PARAMS, XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V2;

typedef struct {
	DWORD cbSize;
	XLIVE_CONTENT_INSTALL_CALLBACK* pInstallCallback;
	void* pInstallCallbackContext;
} XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V1;

typedef struct _XSESSION_INFO
{
    XNKID sessionID;                // 8 bytes
    XNADDR hostAddress;             // 36 bytes
    XNKEY keyExchangeKey;           // 16 bytes
} XSESSION_INFO, *PXSESSION_INFO;

typedef struct _XSESSION_SEARCHRESULT
{
    XSESSION_INFO   info;
	size_t           dwOpenPublicSlots;
	size_t           dwOpenPrivateSlots;
	size_t           dwFilledPublicSlots;
	size_t           dwFilledPrivateSlots;
    DWORD           cProperties;
    DWORD           cContexts;
    PXUSER_PROPERTY pProperties;
    PXUSER_CONTEXT  pContexts;
} XSESSION_SEARCHRESULT, *PXSESSION_SEARCHRESULT;

typedef struct _XSESSION_SEARCHRESULT_HEADER
{
    size_t dwSearchResults;
    XSESSION_SEARCHRESULT* pResults;
} XSESSION_SEARCHRESULT_HEADER, *PXSESSION_SEARCHRESULT_HEADER;

typedef struct _XSESSION_REGISTRANT
{
    uint64_t qwMachineID;
    DWORD bTrustworthiness;
    DWORD bNumUsers;
    XUID* rgUsers;

} XSESSION_REGISTRANT;

typedef struct _XSESSION_REGISTRATION_RESULTS
{
    DWORD wNumRegistrants;
    XSESSION_REGISTRANT* rgRegistrants;
} XSESSION_REGISTRATION_RESULTS, *PXSESSION_REGISTRATION_RESULTS;

typedef struct _XSESSION_VIEW_PROPERTIES
{
    uint32_t                   dwViewId;
    size_t                   dwNumProperties;
    XUSER_PROPERTY*         pProperties;
} XSESSION_VIEW_PROPERTIES;

typedef struct _XSESSION_MEMBER
{
    XUID                xuidOnline;
    uint32_t               dwUserIndex;
	// XSESSION_MEMBER_FLAGS_
    uint32_t               dwFlags;
} XSESSION_MEMBER;

typedef struct {
	// Only valid if the host is local to the console.
	uint32_t dwUserIndexHost;
	uint32_t dwGameType;
	uint32_t dwGameMode;
	uint32_t dwFlags;
	size_t dwMaxPublicSlots;
	size_t dwMaxPrivateSlots;
	size_t dwAvailablePublicSlots;
	size_t dwAvailablePrivateSlots;
	size_t dwActualMemberCount;
	size_t dwReturnedMemberCount;
	XSESSION_STATE eState;
	uint64_t qwNonce;
	XSESSION_INFO sessionInfo;
	XNKID xnkidArbitration;
	XSESSION_MEMBER* pSessionMembers;
} XSESSION_LOCAL_DETAILS, *PXSESSION_LOCAL_DETAILS;

typedef struct _XSTORAGE_FACILITY_INFO_GAME_CLIP
{
    uint32_t   dwLeaderboardID;
} XSTORAGE_FACILITY_INFO_GAME_CLIP;

#pragma pack(push, 1)

typedef struct _STRING_DATA
{
    uint16_t wStringSize;
    wchar_t* pszString;
} STRING_DATA;

typedef struct _STRING_VERIFY_RESPONSE
{
    uint16_t wNumStrings;
    HRESULT* pStringResult;
} STRING_VERIFY_RESPONSE;

typedef struct _XUSER_RANK_REQUEST
{
    DWORD dwViewId;
    LONGLONG i64Rating;
} XUSER_RANK_REQUEST;

typedef struct _XUSER_ESTIMATE_RANK_RESULTS
{
    size_t dwNumRanks;
    DWORD* pdwRanks;
} XUSER_ESTIMATE_RANK_RESULTS;

typedef struct _XINVITE_INFO
{
    XUID xuidInvitee;
    XUID xuidInviter;
    DWORD dwTitleID;
    XSESSION_INFO hostInfo;
    BOOL fFromGameInvite;
} XINVITE_INFO, *PXINVITE_INFO;

typedef struct _XSTORAGE_FILE_INFO
{
    DWORD dwTitleID;
    DWORD dwTitleVersion;
    XUID qwOwnerPUID;
    uint8_t bCountryID;
    uint64_t qwReserved;
    DWORD dwContentType;
    DWORD dwStorageSize;
    DWORD dwInstalledSize;
    FILETIME ftCreated;
    FILETIME ftLastModified;
    uint16_t wAttributesSize;
    uint16_t cchPathName;
    wchar_t* pwszPathName;
    uint8_t* pbAttributes;
} XSTORAGE_FILE_INFO, *PXSTORAGE_FILE_INFO;

typedef struct _XSTORAGE_ENUMERATE_RESULTS
{
    DWORD dwTotalNumItems;
    DWORD dwNumItemsReturned;
    XSTORAGE_FILE_INFO* pItems;
} XSTORAGE_ENUMERATE_RESULTS;

typedef struct _XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS
{
    DWORD dwBytesTotal;
    XUID xuidOwner;
    FILETIME ftCreated;
} XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS;

#pragma pack(pop)

typedef struct _XOFFERING_CONTENTAVAILABLE_RESULT
{
    DWORD                               dwNewOffers;
    DWORD                               dwTotalOffers;
} XOFFERING_CONTENTAVAILABLE_RESULT;
typedef struct _XMARKETPLACE_CONTENTOFFER_INFO
{
    uint64_t                       qwOfferID;
    uint64_t                       qwPreviewOfferID;
    DWORD                           dwOfferNameLength;
    wchar_t*                          wszOfferName;
    DWORD                           dwOfferType;
    uint8_t                            contentId[XMARKETPLACE_CONTENT_ID_LEN];
    BOOL                            fIsUnrestrictedLicense;
    DWORD                           dwLicenseMask;
    DWORD                           dwTitleID;
    DWORD                           dwContentCategory;
    DWORD                           dwTitleNameLength;
    wchar_t*                          wszTitleName;
    BOOL                            fUserHasPurchased;
    DWORD                           dwPackageSize;
    DWORD                           dwInstallSize;
    DWORD                           dwSellTextLength;
    wchar_t*                          wszSellText;
    DWORD                           dwAssetID;
    DWORD                           dwPurchaseQuantity;     
    DWORD                           dwPointsPrice;                    
} XMARKETPLACE_CONTENTOFFER_INFO, *PXMARKETPLACE_CONTENTOFFER_INFO;

typedef struct _XMARKETPLACE_ASSET
{
    DWORD                               dwAssetID;
    DWORD                               dwQuantity;
} XMARKETPLACE_ASSET, *PXMARKETPLACE_ASSET;

//sizeof(XMARKETPLACE_ASSET_PACKAGE) + ((cAssets - 1) * sizeof(XMARKETPLACE_ASSET));
typedef struct _XMARKETPLACE_ASSET_PACKAGE
{
    FILETIME                            ftEnumerate;
    DWORD                               cAssets;
    DWORD                               cTotalAssets;
    XMARKETPLACE_ASSET                  aAssets[1];         // this array contains cAssets number of entries... not 1.
} XMARKETPLACE_ASSET_PACKAGE, *PXMARKETPLACE_ASSET_PACKAGE;

typedef struct _XMARKETPLACE_ASSET_ENUMERATE_REPLY
{
    uint8_t                                signature[XMARKETPLACE_ASSET_SIGNATURE_SIZE];
    XMARKETPLACE_ASSET_PACKAGE          assetPackage;
} XMARKETPLACE_ASSET_ENUMERATE_REPLY, *PXMARKETPLACE_ASSET_ENUMERATE_REPLY;

typedef struct _XLIVE_OFFER_INFO {
	uint64_t qwOfferID;
	wchar_t pszName[XLIVE_OFFER_INFO_TITLE_LENGTH];
	wchar_t pszDescription[XLIVE_OFFER_INFO_DESCRIPTION_LENGTH];
	wchar_t pszImageUrl[XLIVE_OFFER_INFO_IMAGEURL_LENGTH];
	DWORD dwPointsCost;
	wchar_t pszGameTitle[XLIVE_OFFER_INFO_TITLE_LENGTH];
	wchar_t pszMediaType[XLIVE_OFFER_INFO_TITLE_LENGTH];
} XLIVE_OFFER_INFO, *PXLIVE_OFFER_INFO, XLIVE_OFFER_INFO_V2;

typedef struct {
	uint64_t qwOfferID;
	wchar_t pszName[XLIVE_OFFER_INFO_TITLE_LENGTH];
	wchar_t pszDescription[XLIVE_OFFER_INFO_DESCRIPTION_LENGTH];
	wchar_t pszImageUrl[XLIVE_OFFER_INFO_IMAGEURL_LENGTH];
	DWORD dwPointsCost;
	wchar_t pszGameTitle[XLIVE_OFFER_INFO_TITLE_LENGTH];
} XLIVE_OFFER_INFO_V1;

typedef struct
{
    XUID        xuid;
    wchar_t       wszCustomText[XPLAYERLIST_CUSTOMTEXT_MAX_LENGTH];
} XPLAYERLIST_USER;

typedef struct
{
    XUID        xuidSelected;
    DWORD       dwKeyCode;
} XPLAYERLIST_RESULT;

typedef struct
{
    uint32_t dwType;
    wchar_t wszCustomText[XPLAYERLIST_BUTTONTEXT_MAX_LENGTH];
} XPLAYERLIST_BUTTON;

typedef struct _MESSAGEBOX_RESULT
{
    union
    {
        uint32_t                           dwButtonPressed;
        uint16_t                            rgwPasscode[4];
    };
} MESSAGEBOX_RESULT, *PMESSAGEBOX_RESULT;

typedef struct _XINPUT_KEYSTROKE
{
    uint16_t                                VirtualKey;
    wchar_t                               Unicode;
    uint16_t                                Flags;
    uint8_t                                UserIndex;
    uint8_t                                HidCode;
} XINPUT_KEYSTROKE, *PXINPUT_KEYSTROKE;

typedef struct
{
    DWORD                               dwActionId;
    wchar_t                               wszEnActionText[23];
    uint16_t                                wReserved;
    DWORD                               dwFlags;
    struct
    {
        DWORD                           dwLanguageId;
        wchar_t                           wszActionText[23];
        uint16_t                            wReserved;
    } rgTranslations[XMSG_MAX_CUSTOMACTION_TRANSLATIONS];
} XMSG_CUSTOMACTION;

typedef struct
{
	uint16_t                                wActionId;
	wchar_t                               wszActionText[23];
	DWORD                               dwFlags;
} XCUSTOMACTION;

typedef struct _XUSER_STATS_COLUMN
{
	uint16_t                                wColumnId;
	XUSER_DATA                          Value;
} XUSER_STATS_COLUMN, *PXUSER_STATS_COLUMN;

typedef struct _XUSER_STATS_ROW
{
	XUID                                xuid;
	DWORD                               dwRank;
	LONGLONG                            i64Rating;
	char                                szGamertag[XUSER_NAME_SIZE];
	DWORD                               dwNumColumns;
	PXUSER_STATS_COLUMN                 pColumns;
} XUSER_STATS_ROW, *PXUSER_STATS_ROW;

typedef struct _XUSER_STATS_VIEW
{
	DWORD                               dwViewId;
	size_t                               dwTotalViewRows;
	DWORD                               dwNumRows;
	PXUSER_STATS_ROW                    pRows;
} XUSER_STATS_VIEW, *PXUSER_STATS_VIEW;

typedef struct _XUSER_STATS_READ_RESULTS
{
	size_t                               dwNumViews;
	PXUSER_STATS_VIEW                   pViews;
} XUSER_STATS_READ_RESULTS, *PXUSER_STATS_READ_RESULTS;

typedef struct _XUSER_STATS_SPEC
{
	DWORD                               dwViewId;
	DWORD                               dwNumColumnIds;
	uint16_t                                rgwColumnIds[XUSER_STATS_ATTRS_IN_SPEC];
} XUSER_STATS_SPEC, *PXUSER_STATS_SPEC;

#pragma pack(push, 1)

typedef struct _ADDRESS_INFO
{
    uint16_t wStreet1Length;
    wchar_t* wszStreet1;
    uint16_t wStreet2Length;
    wchar_t* wszStreet2;
    uint16_t wCityLength;
    wchar_t* wszCity;
    uint16_t wDistrictLength;
    wchar_t* wszDistrict;
    uint16_t wStateLength;
    wchar_t* wszState;
    uint16_t wPostalCodeLength;
    wchar_t* wszPostalCode;
} ADDRESS_INFO;

typedef struct _GET_USER_INFO_RESPONSE
{
    uint16_t wFirstNameLength;
    wchar_t* wszFirstName;
    uint16_t wLastNameLength;
    wchar_t* wszLastName;
    ADDRESS_INFO addressInfo;
    uint16_t wEmailLength;
    wchar_t* wszEmail;
    uint16_t wLanguageId;
    uint8_t bCountryId;
    uint8_t bMsftOptIn;
    uint8_t bParterOptIn;
    uint8_t bAge;
} GET_USER_INFO_RESPONSE;

#pragma pack(pop)

// note: street1 + street2.
#define XAccountGetUserInfoResponseSize() (\
	sizeof(GET_USER_INFO_RESPONSE) +\
	(MAX_FIRSTNAME_SIZE * sizeof(wchar_t)) +\
	(MAX_LASTNAME_SIZE * sizeof(wchar_t)) +\
	(MAX_EMAIL_SIZE * sizeof(wchar_t)) +\
	(MAX_STREET_SIZE * sizeof(wchar_t)) +\
	(MAX_STREET_SIZE * sizeof(wchar_t)) +\
	(MAX_CITY_SIZE * sizeof(wchar_t)) +\
	(MAX_DISTRICT_SIZE * sizeof(wchar_t)) +\
	(MAX_STATE_SIZE * sizeof(wchar_t)) +\
	(MAX_POSTALCODE_SIZE * sizeof(wchar_t))\
)

typedef struct _XUSER_ACHIEVEMENT {
	DWORD dwUserIndex;
	DWORD dwAchievementId;
} XUSER_ACHIEVEMENT, *PXUSER_ACHIEVEMENT;

typedef struct
{
	DWORD                               dwId;
	LPWSTR                              pwszLabel;
	LPWSTR                              pwszDescription;
	LPWSTR                              pwszUnachieved;
	DWORD                               dwImageId;
	DWORD                               dwCred;
	FILETIME                            ftAchieved;
	DWORD                               dwFlags;
} XACHIEVEMENT_DETAILS, *PXACHIEVEMENT_DETAILS;

typedef struct _XLOCATOR_SEARCHRESULT {
	XUID serverID;                     // the ID of dedicated server
	DWORD dwServerType;                     // see XLOCATOR_SERVERTYPE_PUBLIC, etc
	XNADDR serverAddress;                   // the address dedicated server
	XNKID xnkid;
	XNKEY xnkey;
	size_t dwMaxPublicSlots;
	size_t dwMaxPrivateSlots;
	size_t dwFilledPublicSlots;
	size_t dwFilledPrivateSlots;
	DWORD cProperties;                      // number of custom properties.
	PXUSER_PROPERTY pProperties;            // an array of custom properties.

} XLOCATOR_SEARCHRESULT, *PXLOCATOR_SEARCHRESULT;

typedef struct {
	uint32_t unk1;
	void* unk2;
} XLOCATOR_FILTER_GROUP;

typedef struct {
	uint32_t unknownFlag;
	void* unk2;
} XLOCATOR_SORTER;

typedef struct _XLOCATOR_INIT_INFO {
	DWORD ukn1;
	DWORD ukn2;
	uint16_t ukn3;
	char ukn4[0x22];
} XLOCATOR_INIT_INFO;

typedef struct _XLIVE_CONTENT_RETRIEVAL_INFO {
	DWORD dwContentAPIVersion;
	DWORD dwRetrievalMask;
	DWORD dwUserIndex;
	XUID xuidUser;
	DWORD dwTitleID;
	DWORD dwContentType;
	uint8_t abContentID[XLIVE_LICENSE_ID_SIZE];
} XLIVE_CONTENT_RETRIEVAL_INFO, *PXLIVE_CONTENT_RETRIEVAL_INFO;


// You must specify the following initialization parameters at creation time.
typedef struct XHV_INIT_PARAMS
{
	DWORD                                       dwMaxRemoteTalkers;
	DWORD                                       dwMaxLocalTalkers;
	PXHV_PROCESSING_MODE                        localTalkerEnabledModes;
	DWORD                                       dwNumLocalTalkerEnabledModes;
	PXHV_PROCESSING_MODE                        remoteTalkerEnabledModes;
	DWORD                                       dwNumRemoteTalkerEnabledModes;
	BOOL                                        bCustomVADProvided;
	BOOL                                        bRelaxPrivileges;
	PFNMICRAWDATAREADY                          pfnMicrophoneRawDataReady;
	// These existed for the xbox version.
	//LPCXAUDIOVOICEFXCHAIN                       pfxDefaultRemoteTalkerFX;
	//LPCXAUDIOVOICEFXCHAIN                       pfxDefaultTalkerPairFX;
	//LPCXAUDIOVOICEFXCHAIN                       pfxOutputFX;
	HWND                                        hwndFocus;
} XHV_INIT_PARAMS, *PXHV_INIT_PARAMS;

#pragma pack(push, 1)
// This header appears at the beginning of each blob of data reported by voice chat mode.
typedef struct XHV_CODEC_HEADER
{
    uint16_t                                        bMsgNo :  4;
    uint16_t                                        wSeqNo : 11;
    uint16_t                                        bFriendsOnly : 1;
} XHV_CODEC_HEADER, *PXHV_CODEC_HEADER;
#pragma pack (pop)

#pragma pack(push, 4)
class IXHVEngine
{
	public:
		virtual LONG __stdcall AddRef() = 0;
		virtual LONG __stdcall Release() = 0;
		virtual HRESULT __stdcall Lock(XHV_LOCK_TYPE lock_type) = 0;
		virtual HRESULT __stdcall StartLocalProcessingModes(uint32_t user_index, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count) = 0;
		virtual HRESULT __stdcall StopLocalProcessingModes(uint32_t user_index, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count) = 0;
		virtual HRESULT __stdcall StartRemoteProcessingModes(XUID xuid_remote_talker, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count) = 0;
		virtual HRESULT __stdcall StopRemoteProcessingModes(XUID xuid_remote_talker, const XHV_PROCESSING_MODE* processing_modes, size_t processing_mode_count) = 0;
		virtual HRESULT __stdcall SetMaxDecodePackets(size_t max_decode_packets) = 0;
		virtual HRESULT __stdcall RegisterLocalTalker(uint32_t user_index) = 0;
		virtual HRESULT __stdcall UnregisterLocalTalker(uint32_t user_index) = 0;
		virtual HRESULT __stdcall RegisterRemoteTalker(XUID xuid_remote_talker, XAUDIOVOICEFXCHAIN* remote_talker_fx, XAUDIOVOICEFXCHAIN* talker_pair_fx, XAUDIOSUBMIXVOICE* output_voice) = 0;
		virtual HRESULT __stdcall UnregisterRemoteTalker(XUID xuid_remote_talker) = 0;
		virtual HRESULT __stdcall GetRemoteTalkers(uint32_t* remote_talker_count, XUID* remote_talkers) = 0;
		virtual BOOL __stdcall IsHeadsetPresent(uint32_t user_index) = 0;
		virtual BOOL __stdcall IsLocalTalking(uint32_t user_index) = 0;
		virtual BOOL __stdcall isRemoteTalking(XUID xuid_remote_talker) = 0;
		virtual uint32_t __stdcall GetDataReadyFlags() = 0;
		virtual HRESULT __stdcall GetLocalChatData(uint32_t user_index, uint8_t* chat_data, size_t* chat_data_size, size_t* result_packet_count) = 0;
		virtual HRESULT __stdcall SetPlaybackPriority(XUID xuid_remote_talker, uint32_t user_index, XHV_PLAYBACK_PRIORITY playback_priority) = 0;
		virtual HRESULT __stdcall SubmitIncomingChatData(XUID xuid_remote_talker, const uint8_t* chat_data, size_t* chat_data_size) = 0;
};
typedef IXHVEngine *LPIXHVENGINE, *PIXHVENGINE;
#pragma pack (pop)

#pragma pack(pop) // Return to original alignment setting.
