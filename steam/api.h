#ifndef API_STEAM_H
#define API_STEAM_H
#include "common.h"

enum STM_Result
{
    STM_Result_Ok = 0,
    STM_Result_Failed,
    STM_Result_ShouldReopenThroughSteam,
    STM_Result_InvalidParam,
    STM_Result_Ignored,
    STM_Result_LimitExceeded,
}
typedef STM_Result;

enum
{
    STM_InitFlag_InitRelayNetworkAccess = 0x0001,
};

API STM_Result STM_Init(uint64 appid, uint32 flags);
API void STM_Deinit(void);
API bool STM_IsInitialized(void);
API String STM_GetUserNickname(void);
API uint64 STM_GetUserId(void);

// ===========================================================================
// ===========================================================================
enum STM_EventKind
{
    STM_EventKind_Null = 0,
    
    STM_EventKind_AvatarImageLoaded,
    STM_EventKind_UserStatsReceived,

    STM_EventKind_LobbyGameCreated,
    STM_EventKind_GameLobbyJoinRequest,
    STM_EventKind_GameRichPresenceJoinRequested,

    STM_EventKind_ResultLobbyCreated,
    STM_EventKind_ResultGlobalAchievementPercentagesReady,
}
typedef STM_EventKind;

struct STM_Event typedef STM_Event;
struct STM_Event
{
    STM_Event* next;
    STM_EventKind kind;
    union
    {
        struct
        {
            uint64 user_id;
            uint32 image_id;
            int32 width;
            int32 height;
        } avatar_image_loaded;
        struct
        {
            uint64 game_id;
            uint64 user_id;
            STM_Result result;
        } user_stats_received;
        struct
        {
            uint64 lobby_id;
            uint64 game_server_id;
            uint32 ip;
            uint32 port;
        } lobby_game_created;
        struct
        {
            uint64 lobby_id;
            uint64 friend_id;
        } game_lobby_join_request;
        struct
        {
            uint64 friend_id;
            uint8 connect[256];
        } game_rich_presence_join_request;
    };
};

API STM_Event* STM_PollEvents(Allocator allocator);

// ===========================================================================
// ===========================================================================
struct STM_ListenSocket { uint32 id; } typedef STM_ListenSocket;
struct STM_NetConnection { uint32 id; } typedef STM_NetConnection;
struct STM_NetworkingIdentity { uint8 str[128]; } typedef STM_NetworkingIdentity;
struct STM_NetworkingMessage { void* ptr; } typedef STM_NetworkingMessage;

enum
{
	STM_SendMessageToConnectionFlag_Unreliable = 0x0000,
	STM_SendMessageToConnectionFlag_NoNagle = 0x0001,
	STM_SendMessageToConnectionFlag_UnreliableNoNagle = 0x0001,
	STM_SendMessageToConnectionFlag_NoDelay = 0x0004,
	STM_SendMessageToConnectionFlag_UnreliableNoDelay = 0x0005,
	STM_SendMessageToConnectionFlag_Reliable = 0x0008,
	STM_SendMessageToConnectionFlag_ReliableNoNagle = 0x0009,
	STM_SendMessageToConnectionFlag_UseCurrentThread = 0x0010,
	STM_SendMessageToConnectionFlag_AutoRestartBrokenSession = 0x0020,
};

struct STM_NetworkingMessageData
{
    void const* buffer;
    intsize buffer_size;
    int64 time_received;
    STM_NetConnection connection;
    STM_NetworkingIdentity identity;
    uint32 send_message_to_conn_flags;
}
typedef STM_NetworkingMessageData;

API STM_ListenSocket STM_CreateListenSocketP2P(uint64 steam_id_target, int32 virtual_port);
API STM_NetConnection STM_ConnectP2P(STM_NetworkingIdentity const* identity_remote, int32 remote_virtual_port);

API STM_Result STM_CloseConnection(STM_NetConnection connection, int32 reason, String debug_str, bool linger);
API STM_Result STM_CloseListenSocket(STM_ListenSocket socket);
API STM_Result STM_FlushMessagesOnConnection(STM_NetConnection connection);
API STM_Result STM_SendMessageToConnection(STM_NetConnection connection, void const* data, intsize data_size, uint32 send_message_to_conn_flags, int64* out_message_id);
API STM_Result STM_SendMessages(intsize count, STM_NetworkingMessageData messages[]);

API intsize STM_ReceiveMessagesOnConnection(STM_NetConnection connection, intsize count, STM_NetworkingMessage* out_messages);
API STM_NetworkingMessageData STM_DataFromNetworkingMessage(STM_NetworkingMessage message);
API void STM_ReleaseMessage(STM_NetworkingMessage message);

#endif