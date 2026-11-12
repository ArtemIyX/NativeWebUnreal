#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/CriticalSection.h"
#include "UObject/Object.h"
#include "NativeWebView.generated.h"

UENUM(BlueprintType)
enum class ENativeWebPayloadType : uint8 { String, Bytes };

USTRUCT(BlueprintType)
struct NATIVEWEB_API FNativeWebMessage
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") FName Channel = NAME_None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb") ENativeWebPayloadType Type = ENativeWebPayloadType::String;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") FString String;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") TArray<uint8> Bytes;
};

USTRUCT(BlueprintType)
struct NATIVEWEB_API FNativeWebViewOptions
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") FIntPoint PixelSize = FIntPoint(480, 240);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") float DpiScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") FColor ClearColor = FColor(0, 0, 0, 255);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") bool bCreateViewportOverlay = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") int32 ViewportZOrder = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") FString Html;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") FString Css;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") FString PackagedAssetRoot;
};

UENUM(BlueprintType)
enum class ENativeWebQueueResult : uint8 { Queued, Closed, QueueFull };

UENUM(BlueprintType)
enum class ENativeWebInputType : uint8 { PointerMove, PointerDown, PointerUp, Wheel, KeyDown, KeyUp, FocusGained, FocusLost };

UENUM(BlueprintType)
enum class ENativeWebInputKey : uint8 { None, Tab, Enter, Space };

USTRUCT(BlueprintType)
struct NATIVEWEB_API FNativeWebInputEvent
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") ENativeWebInputType Type = ENativeWebInputType::PointerMove;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") FVector2D NormalizedPosition = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") ENativeWebInputKey Key = ENativeWebInputKey::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") float WheelDelta = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb") bool bShift = false;
};

USTRUCT(BlueprintType)
struct NATIVEWEB_API FNativeWebError
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FName Code = NAME_None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FString Message;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FString SourceName;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FString ScriptStack;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FName ViewName = NAME_None;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FNativeWebMessageReceived, const FNativeWebMessage&);
DECLARE_MULTICAST_DELEGATE_OneParam(FNativeWebViewError, const FNativeWebError&);

class FAffineUiScriptInstance;
class SNativeWebView;
class UNativeWebLocalPlayerSubsystem;

UCLASS(BlueprintType)
class NATIVEWEB_API UNativeWebView final : public UObject
{
	GENERATED_BODY()
public:
	virtual void BeginDestroy() override;
	UFUNCTION(BlueprintCallable, Category="NativeWeb") bool Show();
	UFUNCTION(BlueprintCallable, Category="NativeWeb") void Hide();
	UFUNCTION(BlueprintPure, Category="NativeWeb") bool IsVisible() const { return bVisible; }
	UFUNCTION(BlueprintCallable, Category="NativeWeb") bool Resize(FIntPoint PixelSize, float DpiScale = 1.0f);
	UFUNCTION(BlueprintCallable, Category="NativeWeb") bool AttachToPlayerViewport(int32 ZOrder = 0);
	UFUNCTION(BlueprintCallable, Category="NativeWeb") void DetachFromPlayerViewport();
	UFUNCTION(BlueprintPure, Category="NativeWeb") UTextureRenderTarget2D* GetTexture() const;
	UFUNCTION(BlueprintCallable, Category="NativeWeb") bool ForwardInput(const FNativeWebInputEvent& Event);
	UFUNCTION(BlueprintCallable, Category="NativeWeb") bool SetDocument(const FString& Html, const FString& Css, FNativeWebError& OutError);
	UFUNCTION(BlueprintCallable, Category="NativeWeb") bool EvaluateScript(const FString& Source, const FString& SourceName, FNativeWebError& OutError);
	UFUNCTION(BlueprintCallable, Category="NativeWeb") bool LoadPackagedDocument(const FString& HtmlRelativePath, const FString& CssRelativePath, FNativeWebError& OutError);
	UFUNCTION(BlueprintCallable, Category="NativeWeb") bool EvaluatePackagedScript(const FString& ScriptRelativePath, FNativeWebError& OutError);
	ENativeWebQueueResult EnqueueStringToJavaScript(FName Channel, FString Value);
	ENativeWebQueueResult EnqueueBytesToJavaScript(FName Channel, TArray<uint8>&& Value);
	bool SendStringToJavaScript(FName Channel, FStringView Value);
	bool SendBytesToJavaScript(FName Channel, TConstArrayView<uint8> Value);
	FNativeWebMessageReceived& OnMessage() { return MessageReceived; }
	FNativeWebViewError& OnError() { return Error; }
	UFUNCTION(BlueprintCallable, Category="NativeWeb") void Close();
	UFUNCTION(BlueprintPure, Category="NativeWeb") bool IsClosed() const { return bClosed; }
private:
	friend class UNativeWebLocalPlayerSubsystem;
	friend class SNativeWebView;
	bool Initialize(FName InName, const FNativeWebViewOptions& Options, ULocalPlayer* Player, FNativeWebError& OutError);
	void Tick();
	void SetOutputTexture(UTextureRenderTarget2D* Texture);
	bool LoadDocumentFiles(const FString& HtmlPath, const FString& CssPath, FNativeWebError& OutError);
	bool EvaluateScriptFile(const FString& ScriptPath, FNativeWebError& OutError);
	FName ViewName = NAME_None;
	FString PackagedAssetRoot;
#if PLATFORM_WINDOWS
	TUniquePtr<FAffineUiScriptInstance> Script;
#endif
	TSharedPtr<class FAffineUiSurface, ESPMode::ThreadSafe> Surface;
	TSharedPtr<SNativeWebView> Attachment;
	TWeakObjectPtr<ULocalPlayer> OwningPlayer;
	FNativeWebMessageReceived MessageReceived;
	FNativeWebViewError Error;
	FCriticalSection ProducerMutex;
	TArray<FNativeWebMessage> ProducerMessages;
	int64 ProducerBytes = 0;
	bool bVisible = false;
	bool bClosed = false;
	bool bInitialized = false;
	uint64 Generation = 1;
};
