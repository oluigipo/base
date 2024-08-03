#include "common.hpp"
#include "api.h"
#include "api_os.h"

DisableWarnings(); // NOTE(ljre): -Wdeprecated-declarations -Winvalid-offsetof
#include <steam/steam_api.h>

struct SteamCallbacks_
{
	STEAM_CALLBACK(SteamCallbacks_, LobbyGameCreated, LobbyGameCreated_t);
	STEAM_CALLBACK(SteamCallbacks_, GameLobbyJoinRequested, GameLobbyJoinRequested_t);
	STEAM_CALLBACK(SteamCallbacks_, GameRichPresenceJoinRequested, GameRichPresenceJoinRequested_t);
	STEAM_CALLBACK(SteamCallbacks_, AvatarImageLoaded, AvatarImageLoaded_t);
	STEAM_CALLBACK(SteamCallbacks_, UserStatsReceived, UserStatsReceived_t);

	CCallResult<SteamCallbacks_, LobbyCreated_t> result_LobbyCreated;
	CCallResult<SteamCallbacks_, GlobalAchievementPercentagesReady_t> result_GlobalAchievementPercentagesReady;
	
	inline void LobbyCreated(LobbyCreated_t* param, bool io_failure);
	inline void GlobalAchievementPercentagesReady(GlobalAchievementPercentagesReady_t* param, bool io_failure);
};
ReenableWarnings();

extern void* operator new(size_t size, void* ptr);

struct
{
	bool initialized;

	ISteamClient* client;
	ISteamUser* user;
	ISteamFriends* friends;
	ISteamMatchmaking* matchmaking;
	ISteamNetworkingSockets* netsockets;
	ISteamNetworkingUtils* netutils;
	ISteamUtils* utils;
	ISteamUserStats* stats;
	SteamCallbacks_* callbacks;

	CSteamID user_id;

	Allocator pollevents_allocator;
	STM_Event** pollevents_event_list_tail;

	alignas(SteamCallbacks_) uint8 callbacks_mem[sizeof(SteamCallbacks_)];
}
static g_steam;

static STM_Result
SteamResultToResult_(EResult r)
{
	switch (r)
	{
		default: return STM_Result_Failed;
		case k_EResultOK:            return STM_Result_Ok;
		case k_EResultInvalidParam:  return STM_Result_InvalidParam;
		case k_EResultIgnored:       return STM_Result_Ignored;
		case k_EResultLimitExceeded: return STM_Result_LimitExceeded;
	}
}

API STM_Result
STM_Init(uint64 appid, uint32 flags)
{
	Trace();
	Assert(appid <= UINT32_MAX);

	if (appid)
	{
		if (SteamAPI_RestartAppIfNecessary((uint32)appid))
			return STM_Result_ShouldReopenThroughSteam;
	}

	if (!SteamAPI_Init())
		return STM_Result_Failed;

	g_steam.initialized = true;
	g_steam.client = SteamClient();
	g_steam.user = SteamUser();
	g_steam.friends = SteamFriends();
	g_steam.matchmaking = SteamMatchmaking();
	g_steam.netsockets = SteamNetworkingSockets();
	g_steam.netutils = SteamNetworkingUtils();
	g_steam.utils = SteamUtils();
	g_steam.stats = SteamUserStats();
	g_steam.user_id = g_steam.user->GetSteamID();
	g_steam.callbacks = new(g_steam.callbacks_mem) SteamCallbacks_();

	if (flags & STM_InitFlag_InitRelayNetworkAccess)
		g_steam.netutils->InitRelayNetworkAccess();

	return STM_Result_Ok;
}

API void
STM_Deinit()
{
	Trace();
	if (g_steam.initialized)
	{
		SteamAPI_Shutdown();
		MemoryZero(&g_steam, sizeof(g_steam));
	}
}

API bool
STM_IsInitialized()
{
	Trace();
	return g_steam.initialized;
}

API String
STM_GetUserNickname()
{
	Trace();
	return {};
}

API uint64
STM_GetUserId()
{
	Trace();
	uint64 result = 0;
	if (g_steam.initialized)
		result = g_steam.user_id.ConvertToUint64();
	return result;
}

API STM_Event*
STM_PollEvents(Allocator allocator)
{
	Trace();
	STM_Event* result = NULL;
	if (g_steam.initialized)
	{
		g_steam.pollevents_allocator = allocator;
		g_steam.pollevents_event_list_tail = &result;
		SteamAPI_RunCallbacks();
		g_steam.pollevents_allocator = NullAllocator();
		g_steam.pollevents_event_list_tail = NULL;
	}
	return result;
}

void
SteamCallbacks_::LobbyGameCreated(LobbyGameCreated_t* param)
{
	Trace();
	OS_LogErr("[INFO] steam: Callback: LobbyGameCreated\n");
	AllocatorError err;
	STM_Event* event = AllocatorNew<STM_Event>(&g_steam.pollevents_allocator, &err);
	if (event)
	{
		*g_steam.pollevents_event_list_tail = event;
		g_steam.pollevents_event_list_tail = &event->next;
		event->kind = STM_EventKind_LobbyGameCreated;
		event->lobby_game_created.lobby_id = param->m_ulSteamIDLobby;
		event->lobby_game_created.game_server_id = param->m_ulSteamIDGameServer;
		event->lobby_game_created.ip = param->m_unIP;
		event->lobby_game_created.port = param->m_usPort;
	}
}

void
SteamCallbacks_::GameLobbyJoinRequested(GameLobbyJoinRequested_t* param)
{
	Trace();
	OS_LogErr("[INFO] steam: Callback: GameLobbyJoinRequested\n");
	AllocatorError err;
	STM_Event* event = AllocatorNew<STM_Event>(&g_steam.pollevents_allocator, &err);
	if (event)
	{
		*g_steam.pollevents_event_list_tail = event;
		g_steam.pollevents_event_list_tail = &event->next;
		event->kind = STM_EventKind_GameLobbyJoinRequest;
		event->game_lobby_join_request.lobby_id = param->m_steamIDLobby.ConvertToUint64();
		event->game_lobby_join_request.friend_id = param->m_steamIDFriend.ConvertToUint64();
	}
}

void
SteamCallbacks_::GameRichPresenceJoinRequested(GameRichPresenceJoinRequested_t* param)
{
	Trace();
	OS_LogErr("[INFO] steam: Callback: GameRichPresenceJoinRequested\n");
	AllocatorError err;
	STM_Event* event = AllocatorNew<STM_Event>(&g_steam.pollevents_allocator, &err);
	if (event)
	{
		*g_steam.pollevents_event_list_tail = event;
		g_steam.pollevents_event_list_tail = &event->next;
		event->kind = STM_EventKind_GameRichPresenceJoinRequested;
		event->game_rich_presence_join_request.friend_id = param->m_steamIDFriend.ConvertToUint64();
		static_assert(sizeof(event->game_rich_presence_join_request.connect) == sizeof(param->m_rgchConnect), "must be the same");
		MemoryCopy(
			&event->game_rich_presence_join_request.connect[0],
			&param->m_rgchConnect[0],
			sizeof(param->m_rgchConnect));
	}
}

void
SteamCallbacks_::AvatarImageLoaded(AvatarImageLoaded_t* param)
{
	Trace();
	OS_LogErr("[INFO] launcher_steam: Callback: AvatarImageLoaded\n");
	AllocatorError err;
	STM_Event* event = AllocatorNew<STM_Event>(&g_steam.pollevents_allocator, &err);
	if (event)
	{
		*g_steam.pollevents_event_list_tail = event;
		g_steam.pollevents_event_list_tail = &event->next;
		event->kind = STM_EventKind_AvatarImageLoaded;
		event->avatar_image_loaded.user_id = param->m_steamID.ConvertToUint64();
		event->avatar_image_loaded.image_id = param->m_iImage;
		event->avatar_image_loaded.width = param->m_iWide;
		event->avatar_image_loaded.height = param->m_iTall;
	}
}

void
SteamCallbacks_::UserStatsReceived(UserStatsReceived_t* param)
{
	Trace();
	OS_LogErr("[INFO] launcher_steam: Callback: UserStatsReceived\n");
	
	SteamAPICall_t call = g_steam.stats->RequestGlobalAchievementPercentages();
	g_steam.callbacks->result_GlobalAchievementPercentagesReady.Set(call, g_steam.callbacks, &SteamCallbacks_::GlobalAchievementPercentagesReady);
}

inline void
SteamCallbacks_::LobbyCreated(LobbyCreated_t* param, bool io_failure)
{
	Trace();
	OS_LogErr("[INFO] launcher_steam: Result Callback: LobbyCreated\n");
}

inline void
SteamCallbacks_::GlobalAchievementPercentagesReady(GlobalAchievementPercentagesReady_t* param, bool io_failure)
{
	Trace();
	OS_LogErr("[INFO] launcher_steam: Result Callback: GlobalAchievementPercentagesReady\n");
}

API STM_ListenSocket
STM_CreateListenSocketP2P(uint64 steam_id_target, int32 virtual_port)
{
	Trace();
	HSteamListenSocket socket = g_steam.netsockets->CreateListenSocketP2P(virtual_port, 0, NULL);
	return { socket };
}

API STM_NetConnection
STM_ConnectP2P(STM_NetworkingIdentity const* identity_remote, int32 remote_virtual_port)
{
	Trace();
	SteamNetworkingIdentity identity = {};
	if (!identity.ParseString((char const*)identity_remote->str))
		return {};
	HSteamNetConnection conn = g_steam.netsockets->ConnectP2P(identity, remote_virtual_port, 0, NULL);
	return { conn };
}

API STM_Result
STM_CloseConnection(STM_NetConnection connection, int32 reason, String debug_str, bool linger)
{
	Trace();
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	char* debug_cstr = ArenaPushCString(scratch.arena, debug_str);
	bool ok = g_steam.netsockets->CloseConnection(connection.id, reason, debug_cstr, linger);
	ArenaRestore(scratch);
	if (!ok)
		return STM_Result_Failed;
	return STM_Result_Ok;
}

API STM_Result
STM_CloseListenSocket(STM_ListenSocket socket)
{
	Trace();
	if (!g_steam.netsockets->CloseListenSocket(socket.id))
		return STM_Result_Failed;
	return STM_Result_Ok;
}

API STM_Result
STM_FlushMessagesOnConnection(STM_NetConnection connection)
{
	Trace();
	if (!g_steam.netsockets->FlushMessagesOnConnection(connection.id))
		return STM_Result_Failed;
	return STM_Result_Ok;
}

API STM_Result
STM_SendMessageToConnection(STM_NetConnection connection, void const* data, intsize data_size, uint32 send_message_to_conn_flags, int64* out_message_id)
{
	Trace();
	EResult result = g_steam.netsockets->SendMessageToConnection(connection.id, data, data_size, send_message_to_conn_flags, out_message_id);
	return SteamResultToResult_(result);
}

API STM_Result
STM_SendMessages(intsize count, STM_NetworkingMessageData messages[])
{
	Trace();
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	SteamNetworkingMessage_t** smessages_ptrs = ArenaPushArray(scratch.arena, SteamNetworkingMessage_t*, count);

	for (intsize i = 0; i < count; ++i)
	{
		STM_NetworkingMessageData* message = &messages[i];
		SafeAssert(message->buffer_size > 0);

		SteamNetworkingMessage_t* smessage = g_steam.netutils->AllocateMessage(message->buffer_size);
		smessage->m_conn = message->connection.id;
		smessage->m_nFlags = message->send_message_to_conn_flags;
		// smessage->m_identityPeer.ParseString((char*)message->identity.str);
		MemoryCopy(smessage->m_pData, message->buffer, message->buffer_size);

		smessages_ptrs[i] = smessage;
	}

	g_steam.netsockets->SendMessages(count, smessages_ptrs, NULL);
	ArenaRestore(scratch);
	return STM_Result_Ok;
}

API intsize
STM_ReceiveMessagesOnConnection(STM_NetConnection connection, intsize count, STM_NetworkingMessage out_messages[])
{
	Trace();
	return g_steam.netsockets->ReceiveMessagesOnConnection(connection.id, (SteamNetworkingMessage_t**)out_messages, count);
}

API STM_NetworkingMessageData
STM_DataFromNetworkingMessage(STM_NetworkingMessage message)
{
	Trace();
	SteamNetworkingMessage_t* smessage = (SteamNetworkingMessage_t*)message.ptr;
	STM_NetworkingIdentity identity = {};
	smessage->m_identityPeer.ToString((char*)identity.str, ArrayLength(identity.str));
	return {
		.buffer = smessage->m_pData,
		.buffer_size = smessage->m_cbSize,
		.time_received = smessage->m_usecTimeReceived,
		.connection = { smessage->m_conn },
		.identity = identity,
		.send_message_to_conn_flags = (uint32)smessage->m_nFlags,
	};
}

API void
STM_ReleaseMessage(STM_NetworkingMessage message)
{
	Trace();
	SteamNetworkingMessage_t* smessage = (SteamNetworkingMessage_t*)message.ptr;
	smessage->Release();
}
