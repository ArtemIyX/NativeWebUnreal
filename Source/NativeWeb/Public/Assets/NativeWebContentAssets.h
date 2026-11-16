#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NativeWebContentAssets.generated.h"

UCLASS(Abstract, BlueprintType)
class NATIVEWEB_API UNativeWebContentAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	const TArray<uint8>& GetBytes() const { return Bytes; }
	const FString& GetImportedFilename() const { return ImportedFilename; }
	virtual FName GetAuthoringKind() const PURE_VIRTUAL(UNativeWebContentAsset::GetAuthoringKind, return NAME_None;);
	static bool DecodeTextPayload(const TArray<uint8>& Bytes, FString& OutText, FString& OutReason);

#if WITH_EDITOR
	void SetImportedContent(TArray<uint8>&& InBytes, FString InFilename);
#endif

private:
	UPROPERTY(VisibleAnywhere, Category="Imported Content", meta=(AllowPrivateAccess="true", HideInDetailPanel))
	TArray<uint8> Bytes;

	UPROPERTY(VisibleAnywhere, Category="Imported Content", meta=(AllowPrivateAccess="true", HideInDetailPanel))
	FString ImportedFilename;
};

UCLASS(BlueprintType)
class NATIVEWEB_API UNativeWebHtmlAsset : public UNativeWebContentAsset
{
	GENERATED_BODY()
public:
	virtual FName GetAuthoringKind() const override { return TEXT("HTML"); }
};

UCLASS(BlueprintType)
class NATIVEWEB_API UNativeWebCssAsset : public UNativeWebContentAsset
{
	GENERATED_BODY()
public:
	virtual FName GetAuthoringKind() const override { return TEXT("CSS"); }
};

UCLASS(BlueprintType)
class NATIVEWEB_API UNativeWebJavaScriptAsset : public UNativeWebContentAsset
{
	GENERATED_BODY()
public:
	virtual FName GetAuthoringKind() const override { return TEXT("JavaScript"); }
};

UCLASS(BlueprintType)
class NATIVEWEB_API UNativeWebFontAsset : public UNativeWebContentAsset
{
	GENERATED_BODY()
public:
	virtual FName GetAuthoringKind() const override { return TEXT("Font"); }
	static bool ValidatePayload(const FString& Filename, TConstArrayView<uint8> Bytes, FString& OutReason);
};
