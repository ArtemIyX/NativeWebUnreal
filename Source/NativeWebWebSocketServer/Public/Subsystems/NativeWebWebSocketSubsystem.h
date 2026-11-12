/**
 * @file NativeWebWebSocketServer\Public\Subsystems\NativeWebWebSocketSubsystem.h
 * @brief Declares the game-instance subsystem that owns NativeWebWebSocket WebSocket servers.
 */
#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/NativeWebWebSocketStructs.h"
#include "Data/NativeWebWebSocketEnums.h"
#include "NativeWebWebSocketSubsystem.generated.h"

class UNativeWebWebSocketServerBase;
class UNativeWebWebSocketClientBase;
/**
 * @brief Declares the f native web web socket game thread event test access type and its operations.
 * @ingroup NativeWebWebSocketServer
 */
class FNativeWebWebSocketGameThreadEventTestAccess;

/** @brief Manages the lifetime and lookup of WebSocket server objects for a game instance. */
UCLASS()
class NATIVEWEBWEBSOCKETSERVER_API UNativeWebWebSocketSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UNativeWebWebSocketSubsystem();
#pragma region Lifecycle
	/** @brief Determines whether this subsystem is created for the owning game instance.
 * @param Outer Output value populated by this operation when applicable.
 * @return True when the named condition holds.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ShouldCreateSubsystem(Outer);
 * @endcode
 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	/** @brief Initializes server ownership and tick registration.
* @param Collection Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Initialize(Collection);
 * @endcode
 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** @brief Stops owned servers and unregisters ticking.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Deinitialize();
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */
	virtual void Deinitialize() override;
	/** @brief Dispatches a bounded round-robin batch of queued server events.
* @param InDeltaTime Elapsed time supplied by the owning engine tick.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Tick(InDeltaTime);
 * @endcode
 */
	virtual void Tick(float InDeltaTime) override;
	/** @brief Returns the stat identifier for subsystem event dispatch.
 * @return The value produced by this operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetStatId();
 * @endcode
 */
	virtual TStatId GetStatId() const override;
	/** @brief Returns whether the initialized game instance may tick this subsystem.
 * @return True when the named condition holds.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * IsTickable();
 * @endcode
 */
	virtual bool IsTickable() const override;
	/** @brief Keeps one global dispatch budget for the game instance.
 * @return The requested object/result, or null when unavailable.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetTickableGameObjectWorld();
 * @endcode
 */
	virtual UWorld* GetTickableGameObjectWorld() const override;
#pragma endregion

public:
#pragma region PublicApi
	/** @brief Creates a server or returns the existing server with the same identifier. @param InOptions Server creation options. @param InServerClass Server UObject class. @param InClientClass Client UObject class. @param OutServer Receives the created or existing server. @return Creation result.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * CreateOrGetServer(InOptions, InServerClass, InClientClass, OutServer);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	FNativeWebWebSocketServerCreateResult CreateOrGetServer(
		const FNativeWebWebSocketServerCreateOptions& InOptions,
		TSubclassOf<UNativeWebWebSocketServerBase> InServerClass,
		TSubclassOf<UNativeWebWebSocketClientBase> InClientClass,
		UNativeWebWebSocketServerBase*& OutServer);

	/** @brief Finds a server by logical identifier. @param InNameId Server identifier. @return Matching server, or nullptr.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetServer(InNameId);
 * @endcode
 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	UNativeWebWebSocketServerBase* GetServer(FName InNameId) const;

	/** @brief Stops and removes a server. @param InNameId Server identifier. @return true when the server was found and stopped.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * StopServer(InNameId);
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	bool StopServer(FName InNameId);

	/** @brief Stops and removes all servers owned by this game instance.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * StopAllServers();
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	void StopAllServers();

	/** @brief Returns all servers currently owned by this subsystem.
 * @return The requested object/result, or null when unavailable.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetAllServers();
 * @endcode
 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	TArray<UNativeWebWebSocketServerBase*> GetAllServers() const;
#pragma endregion

private:
#pragma region Internal
	/** @brief Grants the tests access to the non-public queue ownership seam. */
	friend class FNativeWebWebSocketGameThreadEventTestAccess;

	/** @brief Resolves the requested port and reports whether it was adjusted.
* @param InRequestedPort Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @param OutResolvedPort Output value populated by this operation when applicable.
 * @param bOutAdjusted Output value populated by this operation when applicable.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * TryResolvePort(InRequestedPort, OutResolvedPort, bOutAdjusted);
 * @endcode
 */
	bool TryResolvePort(int32 InRequestedPort, int32& OutResolvedPort, bool& bOutAdjusted) const;

	/** Servers owned by this game instance, keyed by logical identifier. */
	UPROPERTY(Transient)
	/** @brief Reflected data field `Servers`. */
	TMap<FName, TObjectPtr<UNativeWebWebSocketServerBase>> Servers;

	/** @brief Starting index for the next round-robin dispatch pass. */
	int32 RoundRobinStartIndex = 0;
	/** @brief True after the game-instance subsystem has initialized. */
	bool bInitialized = false;
#pragma endregion
};

