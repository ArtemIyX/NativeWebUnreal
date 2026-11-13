/**
 * @file NativeWebWebSocketServer\\Private\\Tests\\NativeWebWebSocketGameThreadEventTestTypes.h
 * @brief Declares test-only WebSocket server and client types for event queue automation tests.
 */
#pragma once

#include "CoreMinimal.h"
#include "Server/NativeWebWebSocketClientBase.h"
#include "Server/NativeWebWebSocketServerBase.h"
#include "NativeWebWebSocketGameThreadEventTestTypes.generated.h"

/**
 * @brief Declares the u native web web socket game thread event test server type and its operations.
 * @ingroup NativeWebTests
 */
class UNativeWebWebSocketGameThreadEventTestServer;

/** @brief Test client that records downstream byte and text callbacks. */
UCLASS()
class UNativeWebWebSocketGameThreadEventTestClient : public UNativeWebWebSocketClientBase
{
	GENERATED_BODY()

public:
	static UNativeWebWebSocketGameThreadEventTestServer* ActiveServer;

/**
 * @brief Handles bytes from client.
* @param InData Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleBytesFromClient(InData);
 * @endcode
 */

	virtual void HandleBytesFromClient(const TArray<uint8>& InData) override;
/**
 * @brief Handles string from client.
* @param InMessage Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleStringFromClient(InMessage);
 * @endcode
 */

	virtual void HandleStringFromClient(const FString& InMessage) override;
/**
 * @brief Declares the u native web web socket game thread event test server type and its operations.
 * @ingroup NativeWebTests
 */
};

/** @brief Test server that records event order, payloads, and callback thread affinity. */
UCLASS()
class UNativeWebWebSocketGameThreadEventTestServer : public UNativeWebWebSocketServerBase
{
	GENERATED_BODY()

public:
	TArray<FString> Records;
	TArray<uint32> CallbackThreadIds;
	TArray<int64> MessageIds;
	TArray<TArray<uint8>> MessagePayloads;
	TArray<bool> MessageBinaryFlags;
	TArray<FString> MessageTexts;
	TArray<bool> ConnectedClientVisibleInCallbacks;
	TArray<bool> DisconnectedClientVisibleInCallbacks;
	bool bStopOnConnected = false;

/**
 * @brief Performs the reset records operation.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ResetRecords();
 * @endcode
 */

	void ResetRecords();
/**
 * @brief Performs the record client bytes operation.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RecordClientBytes();
 * @endcode
 */

	void RecordClientBytes();
/**
 * @brief Performs the record client string operation.
* @param InMessage Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RecordClientString(InMessage);
 * @endcode
 */

	void RecordClientString(const FString& InMessage);

/**
 * @brief Performs the record connected delegate operation.
* @param InClientInfo Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RecordConnectedDelegate(InClientInfo);
 * @endcode
 */

	UFUNCTION()
	void RecordConnectedDelegate(const FNativeWebWebSocketClientInfo& InClientInfo);
/**
 * @brief Performs the record disconnected delegate operation.
* @param InClientId Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InReason Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RecordDisconnectedDelegate(InClientId, InReason);
 * @endcode
 */

	UFUNCTION()
	void RecordDisconnectedDelegate(int64 InClientId, ENativeWebWebSocketCloseReason InReason);
/**
 * @brief Performs the record server error delegate operation.
* @param InServerName Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InErrorCode Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InMessage Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RecordServerErrorDelegate(InServerName, InErrorCode, InMessage);
 * @endcode
 */

	UFUNCTION()
	void RecordServerErrorDelegate(FName InServerName, ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage);
/**
 * @brief Performs the record client error delegate operation.
* @param InClientId Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InErrorCode Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InMessage Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RecordClientErrorDelegate(InClientId, InErrorCode, InMessage);
 * @endcode
 */

	UFUNCTION()
	void RecordClientErrorDelegate(int64 InClientId, ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage);

protected:
/**
 * @brief Handles client connected.
* @param InClientInfo Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleClientConnected(InClientInfo);
 * @endcode
 */

	virtual void HandleClientConnected(const FNativeWebWebSocketClientInfo& InClientInfo) override;
/**
 * @brief Handles client disconnected.
* @param InClientId Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InReason Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleClientDisconnected(InClientId, InReason);
 * @endcode
 */

	virtual void HandleClientDisconnected(int64 InClientId, ENativeWebWebSocketCloseReason InReason) override;
/**
 * @brief Handles server error.
* @param InErrorCode Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InMessage Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleServerError(InErrorCode, InMessage);
 * @endcode
 */

	virtual void HandleServerError(ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage) override;
/**
 * @brief Handles client error.
* @param InClientId Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InErrorCode Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InMessage Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleClientError(InClientId, InErrorCode, InMessage);
 * @endcode
 */

	virtual void HandleClientError(int64 InClientId, ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage) override;
/**
 * @brief Handles client message.
* @param InClientId Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InPayload Opaque payload bytes; this operation does not impose an application schema.
* @param bInBinary Boolean option controlling this operation.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleClientMessage(InClientId, InPayload, bInBinary);
 * @endcode
 */

	virtual void HandleClientMessage(int64 InClientId, const TArray<uint8>& InPayload, bool bInBinary) override;
/**
 * @brief Handles client bytes.
* @param InClient Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InData Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleClientBytes(InClient, InData);
 * @endcode
 */

	virtual void HandleClientBytes(UNativeWebWebSocketClientBase* InClient, const TArray<uint8>& InData) override;
/**
 * @brief Handles client string.
* @param InClient Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InMessage Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleClientString(InClient, InMessage);
 * @endcode
 */

	virtual void HandleClientString(UNativeWebWebSocketClientBase* InClient, const FString& InMessage) override;
/**
 * @brief Performs the server initialized operation.
* @param InNativeWebWebSocketSubsystem Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ServerInitialized(InNativeWebWebSocketSubsystem);
 * @endcode
 */

	virtual void ServerInitialized(class UNativeWebWebSocketSubsystem* InNativeWebWebSocketSubsystem) override;
};
