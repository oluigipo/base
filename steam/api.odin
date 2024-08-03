package steam

import C "src:common_odin"

Result :: enum
{
	Ok = 0,
	Failed,
	ShouldReopenThroughSteam,
	InvalidParam,
	Ignored,
	LimitExceeded,
}

InitFlag :: enum
{
	InitRelayNetworkAccess,
}
InitFlags :: bit_set[InitFlag; u32]

EventKind :: enum
{
	Null = 0,
	AvatarImageLoaded,
	UserStatsReceived,
	LobbyGameCreated,
	GameLobbyJoinRequest,
	GameRichPresenceJoinRequested,
	ResultLobbyCreated,
	ResultGlobalAchievementPercentagesReady,
}

Event :: struct
{
	next: ^Event,
	kind: EventKind,
	using _: struct #raw_union
	{
		avatar_image_loaded: struct
		{
			user_id: u64,
			image_id: u32,
			width: i32,
			height: i32,
		},
		user_stats_received: struct
		{
			game_id: u64,
			user_id: u64,
			result: Result,
		},
		lobby_game_created: struct
		{
			lobby_id: u64,
			game_server_id: u64,
			ip: u32,
			port: u32,
		},
		game_lobby_join_request: struct
		{
			lobby_id: u64,
			friend_id: u64,
		},
		game_rich_presence_join_request: struct
		{
			friend_id: u64,
			connect: [256]u8,
		},
	},
}

ListenSocket :: distinct u32
NetConnection :: distinct u32
NetworkingIdentity :: distinct [128]u8
NetworkingMessage :: distinct rawptr

SendMessageToConnectionFlag :: enum
{
	NoNagle = 0,
	NoDelay = 2,
	Reliable = 3,
	UseCurrentThread = 4,
	AutoRestartBrokenSession = 5,
}
SendMessageToConnectionFlags :: bit_set[SendMessageToConnectionFlag; u32]

NetworkingMessageData :: struct
{
	buffer: [^]u8,
	buffer_size: int,
	time_received: i64,
	connection: NetConnection,
	identity: NetworkingIdentity,
	flags: SendMessageToConnectionFlags,
}

@(default_calling_convention="c", link_prefix="STM_")
foreign {
	Init :: proc(appid: u64, flags: InitFlags) -> Result ---
	Deinit :: proc() ---
	IsInitialized :: proc() -> bool ---
	GetUserNickname :: proc() -> string ---
	GetUserId :: proc() -> u64 ---
	PollEvents :: proc(allocator: C.CAllocator) -> ^Event ---
	CreateListenSocketP2P :: proc(steam_id_target: u64, virtual_port: i32) -> ListenSocket ---
	ConnectP2P :: proc(#by_ptr identity_remote: NetworkingIdentity, remote_virtual_port: i32) ---
	CloseConnection :: proc(connection: NetConnection, reason: i32, debug_str: string, linger: bool) -> Result ---
	CloseListenSocket :: proc(socket: ListenSocket) -> Result ---
	FlushMessagesOnConnection :: proc(connection: NetConnection) -> Result ---
	SendMessageToConnection :: proc(connection: NetConnection, data: rawptr, data_size: int, flags: SendMessageToConnectionFlags, out_message_id: ^i64) -> Result ---
	SendMessages :: proc(count: int, messages: [^]NetworkingMessageData) ---
	ReceiveMessagesOnConnection :: proc(connection: NetConnection, count: int, out_messages: [^]NetworkingMessage) -> int ---
	DataFromNetworkingMessage :: proc(message: NetworkingMessage) -> NetworkingMessageData ---
	ReleaseMessage :: proc(message: NetworkingMessage) ---
}
