#include "Subsystems/NativeWebLocalPlayerSubsystem.h"

#include "Containers/Ticker.h"

void UNativeWebLocalPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UNativeWebLocalPlayerSubsystem::Tick));
}

void UNativeWebLocalPlayerSubsystem::Deinitialize()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	for (TPair<FName, TObjectPtr<UNativeWebView>>& Pair : Views)
		if (Pair.Value) Pair.Value->Close();
	Views.Reset();
	Super::Deinitialize();
}

UNativeWebView* UNativeWebLocalPlayerSubsystem::CreateView(FName Name, const FNativeWebViewOptions& Options, FNativeWebError& OutError)
{
	check(IsInGameThread());
	OutError = {};
	if (Name.IsNone())
	{
		OutError.Code = TEXT("InvalidName"); OutError.Message = TEXT("A NativeWeb view requires a non-empty local name."); return nullptr;
	}
	if (Views.Contains(Name))
	{
		OutError.Code = TEXT("NameInUse"); OutError.Message = FString::Printf(TEXT("NativeWeb view '%s' already exists."), *Name.ToString()); OutError.ViewName = Name; return nullptr;
	}
	UNativeWebView* View = NewObject<UNativeWebView>(this);
	if (!View->Initialize(Name, Options, GetLocalPlayer(), OutError)) return nullptr;
	Views.Add(Name, View);
	if (Options.bCreateViewportOverlay)
	{
		if (!View->AttachToPlayerViewport(Options.ViewportZOrder))
		{
			View->Close(); Views.Remove(Name);
			OutError.Code = TEXT("ViewportUnavailable"); OutError.Message = TEXT("Could not attach the NativeWeb view to the local player viewport."); OutError.ViewName = Name;
			return nullptr;
		}
		View->Show();
	}
	return View;
}

UNativeWebView* UNativeWebLocalPlayerSubsystem::FindView(FName Name) const
{
	if (const TObjectPtr<UNativeWebView>* Found = Views.Find(Name)) return Found->Get();
	return nullptr;
}

bool UNativeWebLocalPlayerSubsystem::DestroyView(FName Name)
{
	check(IsInGameThread());
	TObjectPtr<UNativeWebView>* Found = Views.Find(Name);
	if (!Found) return false;
	if (*Found) (*Found)->Close();
	Views.Remove(Name);
	return true;
}

bool UNativeWebLocalPlayerSubsystem::Tick(float)
{
	TArray<TObjectPtr<UNativeWebView>> Snapshot;
	Views.GenerateValueArray(Snapshot);
	for (const TObjectPtr<UNativeWebView>& View : Snapshot)
		if (View && !View->IsClosed()) View->Tick();
	return true;
}
