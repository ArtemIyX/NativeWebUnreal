#include "Assets/NativeWebContentAssetDetails.h"

#include "Assets/NativeWebContentAssetActions.h"
#include "Assets/NativeWebContentAssets.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IDesktopPlatform.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Misc/Paths.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SWindow.h"

namespace
{
	void Notify(const FString& Message, SNotificationItem::ECompletionState State)
	{
		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = State == SNotificationItem::CS_Fail ? 7.0f : 3.0f;
		Info.bUseSuccessFailIcons = true;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
			Item->SetCompletionState(State);
	}

	void SelectAsset(IDetailLayoutBuilder& DetailBuilder, TWeakObjectPtr<UNativeWebContentAsset>& OutAsset)
	{
		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailBuilder.GetObjectsBeingCustomized(Objects);
		if (Objects.Num() == 1) OutAsset = Cast<UNativeWebContentAsset>(Objects[0].Get());
	}

	void RunDialogAction(TWeakObjectPtr<UNativeWebContentAsset> WeakAsset, bool bExport, bool bClear)
	{
		UNativeWebContentAsset* Asset = WeakAsset.Get();
		if (!IsValid(Asset)) return;
		FString Error;
		if (bClear)
		{
			if (!NativeWebContentAssetActions::Clear(*Asset, Error))
				Notify(FString::Printf(TEXT("Clear failed: %s"), *Error), SNotificationItem::CS_Fail);
			else Notify(TEXT("Content cleared."), SNotificationItem::CS_Success);
			return;
		}
		IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
		if (!Desktop)
		{
			Notify(FString::Printf(TEXT("%s failed: the file dialog service is unavailable."), bExport ? TEXT("Export") : TEXT("Import")), SNotificationItem::CS_Fail);
			return;
		}
		void* ParentWindow = nullptr;
		if (FSlateApplication::IsInitialized())
		{
			const TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();
			if (Window.IsValid() && Window->GetNativeWindow().IsValid()) ParentWindow = Window->GetNativeWindow()->GetOSWindowHandle();
		}
		const bool bFont = Asset->IsA<UNativeWebFontAsset>();
		const FString Filter = bFont ? TEXT("Font files (*.ttf;*.otf)|*.ttf;*.otf") :
			Asset->IsA<UNativeWebHtmlAsset>() ? TEXT("HTML files (*.html;*.htm)|*.html;*.htm") :
			Asset->IsA<UNativeWebCssAsset>() ? TEXT("CSS files (*.css)|*.css") : TEXT("JavaScript files (*.js)|*.js");
		const FString Suggested = Asset->GetImportedFilename().IsEmpty()
			? FString() : Asset->GetImportedFilename();
		const FString InitialDirectory = FPaths::ProjectContentDir();
		TArray<FString> Filenames;
		bool bSelected = false;
		if (bExport)
		{
			bSelected = Desktop->SaveFileDialog(ParentWindow, TEXT("Export NativeWeb content"), InitialDirectory,
			                                    Suggested, Filter, EFileDialogFlags::None, Filenames);
		}
		else
		{
			bSelected = Desktop->OpenFileDialog(ParentWindow, TEXT("Import NativeWeb content"), InitialDirectory,
			                                    Suggested, Filter, EFileDialogFlags::None, Filenames);
		}
		if (!bSelected || Filenames.IsEmpty())
		{
			Notify(bExport ? TEXT("Export canceled.") : TEXT("Import canceled."), SNotificationItem::CS_None);
			return;
		}
		const FString& Path = Filenames[0];
		if (bExport)
		{
			if (!NativeWebContentAssetActions::ExportFile(*Asset, Path, Error))
				Notify(FString::Printf(TEXT("Export failed: %s"), *Error), SNotificationItem::CS_Fail);
			else Notify(TEXT("Content exported."), SNotificationItem::CS_Success);
		}
		else if (!NativeWebContentAssetActions::ImportFile(*Asset, Path, Error))
		{
			Notify(FString::Printf(TEXT("Import failed: %s"), *Error), SNotificationItem::CS_Fail);
		}
		else Notify(TEXT("Content imported."), SNotificationItem::CS_Success);
	}
}

TSharedRef<IDetailCustomization> FNativeWebContentAssetDetails::MakeInstance()
{
	return MakeShared<FNativeWebContentAssetDetails>();
}

void FNativeWebContentAssetDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TWeakObjectPtr<UNativeWebContentAsset> Asset;
	SelectAsset(DetailBuilder, Asset);
	if (!Asset.IsValid()) return;
	DetailBuilder.HideProperty(TEXT("Bytes"), UNativeWebContentAsset::StaticClass());
	DetailBuilder.HideProperty(TEXT("CompressedBytes"), UNativeWebContentAsset::StaticClass());
	DetailBuilder.HideProperty(TEXT("UncompressedSize"), UNativeWebContentAsset::StaticClass());
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("Imported Content"));
	Category.AddCustomRow(FText::FromString(TEXT("Import"))).WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton).Text(FText::FromString(TEXT("Import"))).OnClicked_Lambda([Asset]()
			{
				RunDialogAction(Asset, false, false);
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton).Text(FText::FromString(TEXT("Export")))
			.IsEnabled_Lambda([Asset]() { const UNativeWebContentAsset* Current = Asset.Get(); return Current && Current->HasStoredContent(); })
			.OnClicked_Lambda([Asset]()
			{
				RunDialogAction(Asset, true, false);
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton).Text(FText::FromString(TEXT("Clear")))
			.IsEnabled_Lambda([Asset]() { const UNativeWebContentAsset* Current = Asset.Get(); return Current && Current->HasStoredContent(); })
			.OnClicked_Lambda([Asset]()
			{
				RunDialogAction(Asset, false, true);
				return FReply::Handled();
			})
		]
	];
}
