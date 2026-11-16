#include "Assets/NativeWebContentAssetDetails.h"

#include "Assets/NativeWebContentAssetActions.h"
#include "Assets/NativeWebContentAssets.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"

namespace
{
	void ShowError(const FString& Message)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
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
			if (!NativeWebContentAssetActions::Clear(*Asset, Error)) ShowError(Error);
			return;
		}
		IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
		if (!Desktop)
		{
			ShowError(TEXT("The platform file dialog service is unavailable."));
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
		if (!bSelected || Filenames.IsEmpty()) return;
		const FString& Path = Filenames[0];
		if (bExport)
		{
			if (!NativeWebContentAssetActions::ExportFile(*Asset, Path, Error)) ShowError(Error);
		}
		else if (!NativeWebContentAssetActions::ImportFile(*Asset, Path, Error))
		{
			ShowError(Error);
		}
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
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("Imported Content"));
	if (!Asset->IsA<UNativeWebFontAsset>())
	{
		const FText TextSizeLabel = FText::FromString(TEXT("Imported text size (KB)"));
		Category.AddCustomRow(TextSizeLabel)
		.NameContent()[SNew(STextBlock).Text(TextSizeLabel)]
		.ValueContent()[SNew(STextBlock).Text_Lambda([Asset]()
		{
			const UNativeWebContentAsset* Current = Asset.Get();
			const double Kilobytes = Current ? double(Current->GetImportedSize()) / 1024.0 : 0.0;
			return FText::AsNumber(Kilobytes);
		})];
	}
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
			.IsEnabled_Lambda([Asset]() { const UNativeWebContentAsset* Current = Asset.Get(); return Current && Current->GetImportedSize() > 0; })
			.OnClicked_Lambda([Asset]()
			{
				RunDialogAction(Asset, true, false);
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton).Text(FText::FromString(TEXT("Clear")))
			.IsEnabled_Lambda([Asset]() { const UNativeWebContentAsset* Current = Asset.Get(); return Current && (Current->GetImportedSize() != 0 || !Current->GetImportedFilename().IsEmpty()); })
			.OnClicked_Lambda([Asset]()
			{
				RunDialogAction(Asset, false, true);
				return FReply::Handled();
			})
		]
	];
}
