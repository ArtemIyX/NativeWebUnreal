#pragma once

#include "CoreMinimal.h"

#include "HAL/CriticalSection.h"
#include "UObject/Object.h"
#include "Data/Enums/NativeWebQueueResult.h"
#include "Data/Structs/NativeWebError.h"
#include "Data/Structs/NativeWebInputEvent.h"
#include "Data/Structs/NativeWebMessage.h"
#include "Data/Structs/NativeWebViewOptions.h"
#include "NativeWebDelegates.h"
#include "NativeWebView.generated.h"

class FAffineUiScriptInstance;
class SNativeWebView;
class UNativeWebLocalPlayerSubsystem;

UCLASS(BlueprintType)
class NATIVEWEB_API UNativeWebView final : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool Show();
	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	void Hide();
	UFUNCTION(BlueprintPure, Category="NativeWeb")
	bool IsVisible() const { return bVisible; }

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool Resize(FIntPoint PixelSize, float DpiScale = 1.0f);

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool AttachToPlayerViewport(int32 ZOrder = 0);

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	void DetachFromPlayerViewport();

	UFUNCTION(BlueprintPure, Category="NativeWeb")
	UTextureRenderTarget2D* GetTexture() const;

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool ForwardInput(const FNativeWebInputEvent& Event);

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool SetDocument(const FString& Html, const FString& Css, FNativeWebError& OutError);

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool EvaluateScript(const FString& Source, const FString& SourceName, FNativeWebError& OutError);

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool LoadPackagedDocument(const FString& HtmlRelativePath, const FString& CssRelativePath,
	                          FNativeWebError& OutError);
	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool EvaluatePackagedScript(const FString& ScriptRelativePath, FNativeWebError& OutError);

	ENativeWebQueueResult EnqueueStringToJavaScript(FName Channel, FString Value);
	ENativeWebQueueResult EnqueueBytesToJavaScript(FName Channel, TArray<uint8>&& Value);
	bool SendStringToJavaScript(FName Channel, FStringView Value);
	bool SendBytesToJavaScript(FName Channel, TConstArrayView<uint8> Value);
	FNativeWebMessageReceived& OnMessage() { return MessageReceived; }
	FNativeWebViewError& OnError() { return Error; }
	
	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	void Close();
	UFUNCTION(BlueprintPure, Category="NativeWeb")
	bool IsClosed() const { return bClosed; }

private:
	friend class UNativeWebLocalPlayerSubsystem;
	friend class SNativeWebView;
	bool Initialize(FName InName, const FNativeWebViewOptions& Options, ULocalPlayer* Player,
	                FNativeWebError& OutError);
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
