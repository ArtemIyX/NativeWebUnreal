#include "Assets/NativeWebContentAssets.h"

#include "Misc/Paths.h"
#include "Misc/Compression.h"
#include "UObject/ObjectSaveContext.h"

THIRD_PARTY_INCLUDES_START
#include <openssl/sha.h>
THIRD_PARTY_INCLUDES_END

namespace
{
	// Bound allocations when a serialized payload is corrupt or has been tampered with.
	constexpr int32 MaxCompressedPayloadSize = 256 * 1024 * 1024;

	FString HashImportedBytes(const TArray<uint8>& OriginalBytes)
	{
		if (OriginalBytes.IsEmpty()) return FString();
		uint8 Digest[SHA256_DIGEST_LENGTH];
		return SHA256(OriginalBytes.GetData(), static_cast<size_t>(OriginalBytes.Num()), Digest)
			? BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToLower() : FString();
	}
}

void UNativeWebContentAsset::PostLoad()
{
	Super::PostLoad();
	DecodedBytes.Reset();
	bDecodedBytesCached = false;
	RefreshImportedMetadata();
}

void UNativeWebContentAsset::RefreshImportedMetadata()
{
	ImportedSizeKB = double(FMath::Max(0, GetImportedSize())) / 1024.0;
	if (!HasStoredContent())
	{
		ContentHash.Reset();
		return;
	}
	if (!ContentHash.IsEmpty()) return;
	const TArray<uint8>* OriginalBytes = nullptr;
	FString Error;
	if (TryGetBytes(OriginalBytes, Error) && !OriginalBytes->IsEmpty())
	{
		ContentHash = HashImportedBytes(*OriginalBytes);
	}
	// Deriving old metadata must not retain an additional decompressed buffer.
	DecodedBytes.Reset();
	bDecodedBytesCached = false;
}

bool UNativeWebContentAsset::TryGetBytes(const TArray<uint8>*& OutBytes, FString& OutError) const
{
	OutBytes = nullptr;
	OutError.Reset();
	if (CompressedBytes.IsEmpty())
	{
		if (UncompressedSize != 0)
		{
			OutError = TEXT("Compressed content is missing.");
			return false;
		}
		OutBytes = &Bytes;
		return true;
	}
	if (!Bytes.IsEmpty() || UncompressedSize <= 0 || UncompressedSize > MaxCompressedPayloadSize)
	{
		OutError = TEXT("Compressed content has invalid storage metadata.");
		return false;
	}
	if (!bDecodedBytesCached)
	{
		TArray<uint8> Decoded;
		Decoded.SetNumUninitialized(UncompressedSize);
		if (!FCompression::UncompressMemory(NAME_Zlib, Decoded.GetData(), int64(UncompressedSize),
		                                    CompressedBytes.GetData(), int64(CompressedBytes.Num())))
		{
			OutError = TEXT("Could not decompress imported content.");
			return false;
		}
		DecodedBytes = MoveTemp(Decoded);
		bDecodedBytesCached = true;
	}
	OutBytes = &DecodedBytes;
	return true;
}

const TArray<uint8>& UNativeWebContentAsset::GetBytes() const
{
	const TArray<uint8>* Result = nullptr;
	FString Error;
	if (TryGetBytes(Result, Error)) return *Result;
	static const TArray<uint8> Empty;
	return Empty;
}

#if WITH_EDITOR
void UNativeWebContentAsset::SetImportedContent(TArray<uint8>&& InBytes, FString InFilename)
{
	FString NewHash = HashImportedBytes(InBytes);
	Bytes.Reset();
	CompressedBytes.Reset();
	DecodedBytes.Reset();
	bDecodedBytesCached = false;
	UncompressedSize = 0;
	if (!InBytes.IsEmpty() && InBytes.Num() <= MaxCompressedPayloadSize)
	{
		const int32 Bound = FCompression::CompressMemoryBound(NAME_Zlib, InBytes.Num());
		if (Bound > 0)
		{
			TArray<uint8> Compressed;
			Compressed.SetNumUninitialized(Bound);
			int32 CompressedSize = Bound;
			if (FCompression::CompressMemory(NAME_Zlib, Compressed.GetData(), CompressedSize,
			                                 InBytes.GetData(), InBytes.Num()) && CompressedSize < InBytes.Num())
			{
				Compressed.SetNum(CompressedSize);
				CompressedBytes = MoveTemp(Compressed);
				UncompressedSize = InBytes.Num();
			}
		}
	}
	if (CompressedBytes.IsEmpty()) Bytes = MoveTemp(InBytes);
	ImportedFilename = MoveTemp(InFilename);
	ContentHash = MoveTemp(NewHash);
	RefreshImportedMetadata();
}

void UNativeWebContentAsset::PostEditUndo()
{
	Super::PostEditUndo();
	DecodedBytes.Reset();
	bDecodedBytesCached = false;
	RefreshImportedMetadata();
}

void UNativeWebContentAsset::PreSave(FObjectPreSaveContext SaveContext)
{
	// Older assets keep their original Bytes property until they are next saved or cooked.
	if (!Bytes.IsEmpty() && CompressedBytes.IsEmpty() && UncompressedSize == 0)
	{
		TArray<uint8> LegacyBytes = MoveTemp(Bytes);
		SetImportedContent(MoveTemp(LegacyBytes), FString(ImportedFilename));
	}
	RefreshImportedMetadata();
	Super::PreSave(SaveContext);
}
#endif

bool UNativeWebContentAsset::DecodeTextPayload(const TArray<uint8>& Bytes, FString& OutText, FString& OutReason)
{
	const bool bHasBom = Bytes.Num() >= 3 && Bytes[0] == 0xEF && Bytes[1] == 0xBB && Bytes[2] == 0xBF;
	const int32 Offset = bHasBom ? 3 : 0;
	if (Bytes.Num() <= Offset)
	{
		OutReason = TEXT("Imported text must contain UTF-8 data after any byte-order mark.");
		return false;
	}
	const uint8* Data = Bytes.GetData() + Offset;
	const int32 Length = Bytes.Num() - Offset;
	for (int32 Index = 0; Index < Length;)
	{
		const uint8 Lead = Data[Index];
		uint32 CodePoint = 0;
		int32 SequenceLength = 0;
		if (Lead <= 0x7F) { CodePoint = Lead; SequenceLength = 1; }
		else if (Lead >= 0xC2 && Lead <= 0xDF) { CodePoint = Lead & 0x1F; SequenceLength = 2; }
		else if (Lead >= 0xE0 && Lead <= 0xEF) { CodePoint = Lead & 0x0F; SequenceLength = 3; }
		else if (Lead >= 0xF0 && Lead <= 0xF4) { CodePoint = Lead & 0x07; SequenceLength = 4; }
		else { OutReason = TEXT("Imported text contains an invalid UTF-8 lead byte."); return false; }
		if (Index + SequenceLength > Length)
		{
			OutReason = TEXT("Imported text ends inside a UTF-8 sequence.");
			return false;
		}
		for (int32 Part = 1; Part < SequenceLength; ++Part)
		{
			const uint8 Continuation = Data[Index + Part];
			if ((Continuation & 0xC0) != 0x80)
			{
				OutReason = TEXT("Imported text contains a malformed UTF-8 sequence.");
				return false;
			}
			CodePoint = (CodePoint << 6) | (Continuation & 0x3F);
		}
		const bool bOverlong = (SequenceLength == 2 && CodePoint < 0x80) ||
		                       (SequenceLength == 3 && CodePoint < 0x800) ||
		                       (SequenceLength == 4 && CodePoint < 0x10000);
		if (bOverlong || CodePoint == 0 || (CodePoint >= 0xD800 && CodePoint <= 0xDFFF) || CodePoint > 0x10FFFF)
		{
			OutReason = TEXT("Imported text contains an embedded NUL or invalid Unicode code point.");
			return false;
		}
		Index += SequenceLength;
	}
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Data), Length);
	OutText = FString(Converted.Length(), Converted.Get());
	return true;
}

bool UNativeWebFontAsset::ValidatePayload(const FString& Filename, TConstArrayView<uint8> Bytes, FString& OutReason)
{
	if (Filename.IsEmpty() || Filename == TEXT(".") || Filename == TEXT("..") || Filename.Contains(TEXT("/")) ||
		Filename.Contains(TEXT("\\")) || Filename.Contains(TEXT(":")))
	{
		OutReason = TEXT("Font filename must be a safe leaf name.");
		return false;
	}
	for (TCHAR Character : Filename)
	{
		if (Character < 32 || Character == 127)
		{
			OutReason = TEXT("Font filename contains an unsafe control character.");
			return false;
		}
	}
	const FString Extension = FPaths::GetExtension(Filename, true).ToLower();
	if (Extension != TEXT(".ttf") && Extension != TEXT(".otf"))
	{
		OutReason = TEXT("Font filename must end with .ttf or .otf.");
		return false;
	}
	if (Bytes.Num() < 12)
	{
		OutReason = TEXT("Font data is empty or too short to contain an SFNT header.");
		return false;
	}
	const uint8* Data = Bytes.GetData();
	const uint32 Signature = (uint32(Data[0]) << 24) | (uint32(Data[1]) << 16) | (uint32(Data[2]) << 8) | uint32(Data[3]);
	const bool bKnownSignature = Signature == 0x00010000 || Signature == 0x4F54544F || Signature == 0x74727565;
	const uint32 TableCount = (uint32(Data[4]) << 8) | uint32(Data[5]);
	const uint64 DirectoryEnd = 12ull + uint64(TableCount) * 16ull;
	if (!bKnownSignature || TableCount == 0 || TableCount > 4096 || DirectoryEnd > uint64(Bytes.Num()))
	{
		OutReason = TEXT("Font data does not contain a bounded single-face SFNT table directory.");
		return false;
	}
	for (uint32 Table = 0; Table < TableCount; ++Table)
	{
		const uint64 Record = 12ull + uint64(Table) * 16ull;
		const uint32 Offset = (uint32(Data[Record + 8]) << 24) | (uint32(Data[Record + 9]) << 16) |
		                      (uint32(Data[Record + 10]) << 8) | uint32(Data[Record + 11]);
		const uint32 Length = (uint32(Data[Record + 12]) << 24) | (uint32(Data[Record + 13]) << 16) |
		                      (uint32(Data[Record + 14]) << 8) | uint32(Data[Record + 15]);
		if (Length == 0 || uint64(Offset) + uint64(Length) > uint64(Bytes.Num()))
		{
			OutReason = TEXT("Font table has an empty or out-of-bounds range.");
			return false;
		}
	}
	return true;
}
