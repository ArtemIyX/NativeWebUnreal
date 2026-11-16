#include "Assets/NativeWebAssetDefinitions.h"

void UNativeWebAssetDefinitionBase::BuildFilters(TArray<FAssetFilterData>& OutFilters) const
{
	// UAssetDefinition_DataAsset already provides the shared generic Data Asset filter.
	// Per-class definitions inherit that filter rather than adding duplicate entries with the same label.
}
