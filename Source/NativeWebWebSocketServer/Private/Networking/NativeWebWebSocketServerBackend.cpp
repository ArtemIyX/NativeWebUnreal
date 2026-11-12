#include "Networking/NativeWebWebSocketServerBackend.h"

#include "Logging/NativeWebWebSocketLog.h"
#include "Networking/NativeWebWebSocketNetwork.h"

#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include "libwebsockets.h"
THIRD_PARTY_INCLUDES_END
#undef UI

enum class ENativeWebWebSocketFragmentationState : uint8
{
	BeginFrame,
	MessageFrame,
};

struct FNativeWebWebSocketPerSessionDataServer
{
	FNativeWebWebSocketNetwork* Socket = nullptr;
	TArray<uint8> FrameBuffer;
	ENativeWebWebSocketFragmentationState FragmentationState = ENativeWebWebSocketFragmentationState::BeginFrame;
	bool bBinaryFrame = false;
};

static int NativeWebWebSocketNetworkingServerCallback(struct lws* InWsi, enum lws_callback_reasons InReason, void* InUser, void* In, size_t InLen)
{
	struct lws_context* LwsContext = lws_get_context(InWsi);
	FNativeWebWebSocketPerSessionDataServer* BufferInfo = static_cast<FNativeWebWebSocketPerSessionDataServer*>(InUser);
	FNativeWebWebSocketServerBackend* Server = static_cast<FNativeWebWebSocketServerBackend*>(lws_context_user(LwsContext));

	switch (InReason)
	{
	case LWS_CALLBACK_ESTABLISHED:
		{
			BufferInfo->Socket = new FNativeWebWebSocketNetwork(LwsContext, InWsi);
			BufferInfo->FragmentationState = ENativeWebWebSocketFragmentationState::BeginFrame;
			Server->ConnectedCallback.ExecuteIfBound(BufferInfo->Socket);
			lws_set_timeout(InWsi, NO_PENDING_TIMEOUT, 0);
		}
		break;
	case LWS_CALLBACK_RECEIVE:
		if (BufferInfo->Socket && BufferInfo->Socket->Context == LwsContext)
		{
			if (BufferInfo->FragmentationState == ENativeWebWebSocketFragmentationState::BeginFrame)
			{
				BufferInfo->FragmentationState = ENativeWebWebSocketFragmentationState::MessageFrame;
				BufferInfo->FrameBuffer.Reset();
				BufferInfo->bBinaryFrame = lws_frame_is_binary(InWsi) != 0;
			}

			BufferInfo->FrameBuffer.Append(static_cast<const uint8*>(In), static_cast<int32>(InLen));
			if (lws_is_final_fragment(InWsi))
			{
				BufferInfo->FragmentationState = ENativeWebWebSocketFragmentationState::BeginFrame;
				BufferInfo->Socket->OnReceive(BufferInfo->FrameBuffer.GetData(), static_cast<uint32>(BufferInfo->FrameBuffer.Num()), BufferInfo->bBinaryFrame);
			}
		}
		lws_set_timeout(InWsi, NO_PENDING_TIMEOUT, 0);
		break;
	case LWS_CALLBACK_SERVER_WRITEABLE:
		if (BufferInfo->Socket && BufferInfo->Socket->Context == LwsContext)
		{
			BufferInfo->Socket->OnRawWebSocketWritable(InWsi);
			lws_set_timeout(InWsi, NO_PENDING_TIMEOUT, 0);
		}
		break;
	case LWS_CALLBACK_CLOSED:
		if (Server && BufferInfo->Socket)
		{
			const bool bShuttingDown = Server->Context == nullptr;
			if (!bShuttingDown)
			{
				Server->DisconnectedCallback.ExecuteIfBound(BufferInfo->Socket);
				BufferInfo->Socket->OnClose();
			}
			delete BufferInfo->Socket;
			BufferInfo->Socket = nullptr;
		}
		break;
	default:
		break;
	}

	return 0;
}

FNativeWebWebSocketServerBackend::~FNativeWebWebSocketServerBackend()
{
	if (Context)
	{
		struct lws_context* ExistingContext = Context;
		Context = nullptr;
		lws_context_destroy(ExistingContext);
	}
	delete[] Protocols;
	Protocols = nullptr;
}

bool FNativeWebWebSocketServerBackend::Init(uint32 InPort, FNativeWebWebSocketNetworkClientConnectedCallback InOnConnected, FNativeWebWebSocketNetworkClientDisconnectedCallback InOnDisconnected)
{
	Protocols = new lws_protocols[2];
	FMemory::Memzero(Protocols, sizeof(lws_protocols) * 2);

	Protocols[0].name = "binary";
	Protocols[0].callback = NativeWebWebSocketNetworkingServerCallback;
	Protocols[0].per_session_data_size = sizeof(FNativeWebWebSocketPerSessionDataServer);
#if PLATFORM_WINDOWS
	Protocols[0].rx_buffer_size = 10 * 1024 * 1024;
#else
	Protocols[0].rx_buffer_size = 1 * 1024 * 1024;
#endif
	Protocols[1].name = nullptr;
	Protocols[1].callback = nullptr;
	Protocols[1].per_session_data_size = 0;

	struct lws_context_creation_info Info;
	FMemory::Memzero(&Info, sizeof(lws_context_creation_info));
	Info.port = InPort;
	Info.iface = nullptr;
	Info.protocols = Protocols;
	Info.extensions = nullptr;
	Info.gid = -1;
	Info.uid = -1;
	Info.options = LWS_SERVER_OPTION_DISABLE_IPV6;
	Info.user = this;

	Context = lws_create_context(&Info);
	if (!Context)
	{
		delete[] Protocols;
		Protocols = nullptr;
		ServerPort = 0;
		return false;
	}

	ConnectedCallback = InOnConnected;
	DisconnectedCallback = InOnDisconnected;
	ServerPort = InPort;
	return true;
}

void FNativeWebWebSocketServerBackend::Tick()
{
	if (!Context)
	{
		return;
	}
	lws_service(Context, 0);
	lws_callback_on_writable_all_protocol(Context, &Protocols[0]);
}

FString FNativeWebWebSocketServerBackend::Info() const
{
	if (!Context)
	{
		return FString(TEXT("ws-null"));
	}
	return FString::Printf(TEXT("%s:%u"), ANSI_TO_TCHAR(lws_canonical_hostname(Context)), ServerPort);
}
