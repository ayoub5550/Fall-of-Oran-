#include "FOHud.h"
#include "FOGameMode.h"
#include "FOCharacter.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Styling/CoreStyle.h"
#include "Engine/Font.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "FOHud"

FSlateFontInfo SFOHud::Font(int32 Size) const
{
	// Noto Kufi Arabic shipped as a raw TTF under Content/Fonts (staged as UFS, see DefaultGame.ini);
	// Slate shapes Arabic via HarfBuzz. Falls back to the engine font if the file is missing.
	static const FString Ttf = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Fonts/FOKufi.ttf"));
	static const bool bHave = FPaths::FileExists(Ttf);
	if (bHave) { return FSlateFontInfo(Ttf, Size); }
	return FCoreStyle::GetDefaultFontStyle("Bold", Size);
}

void SFOHud::Construct(const FArguments& InArgs)
{
	GM = InArgs._GameMode;
	const FLinearColor Red(0.75f, 0.08f, 0.06f);
	const FLinearColor Sand(0.95f, 0.85f, 0.65f);

	ChildSlot
	[
		SNew(SOverlay)
		// damage / heal flash
		+ SOverlay::Slot()
		[
			SAssignNew(Flash, SImage).Visibility(EVisibility::HitTestInvisible).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(this, &SFOHud::FlashColor)
		]
		// in-game HUD
		+ SOverlay::Slot()
		[
			SAssignNew(GameHud, SOverlay).Visibility(this, &SFOHud::HudVis)
			// health bar top-left
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(28.f, 22.f)
			[
				SNew(SBox).WidthOverride(300.f).HeightOverride(16.f)
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f)).Padding(2.f).HAlign(HAlign_Left)
					[
						SAssignNew(HealthFill, SBox).WidthOverride(296.f)
						[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(Red) ]
					]
				]
			]
			// objective under the health bar
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(28.f, 46.f)
			[ SAssignNew(ObjectiveText, STextBlock).Visibility(EVisibility::HitTestInvisible).Font(Font(18)).ColorAndOpacity(Sand).ShadowOffset(FVector2D(1, 1)) ]
			// kills + fuel top-right
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(28.f, 18.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[ SAssignNew(KillsText, STextBlock).Font(Font(22)).ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.85f)).ShadowOffset(FVector2D(1, 1)) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[ SAssignNew(FuelText, STextBlock).Font(Font(22)).ColorAndOpacity(FLinearColor(0.95f, 0.6f, 0.2f)).ShadowOffset(FVector2D(1, 1)) ]
			]
			// brightness (small, top-centre)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 14.f)[ BrightnessBar(18) ]
			// crosshair
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[ SNew(SBox).WidthOverride(6.f).HeightOverride(6.f)[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(FLinearColor(1, 1, 1, 0.8f)) ] ]
			// ammo bottom-right + fire button
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(40.f, 0.f, 40.f, 40.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 0, 0, 12.f)[ SAssignNew(AmmoText, STextBlock).Font(Font(30)).ColorAndOpacity(Sand).ShadowOffset(FVector2D(1, 1)) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(SBox).WidthOverride(150.f).HeightOverride(150.f)
					[
						SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.55f, 0.05f, 0.04f, 0.55f)).OnPressed_Lambda([this]() { OnFirePressed(); })
						.HAlign(HAlign_Center).VAlign(VAlign_Center)
						[ SNew(STextBlock).Text(LOCTEXT("Fire", "نار")).Font(Font(34)).ColorAndOpacity(Sand) ]
					]
				]
			]
		]
		// menu / death / win overlay
		+ SOverlay::Slot()
		[
			SAssignNew(Overlay, SBorder).Visibility(this, &SFOHud::OverlayVis).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.72f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 14.f)[ SAssignNew(TitleText, STextBlock).Font(Font(64)).ColorAndOpacity(Red).ShadowOffset(FVector2D(2, 2)) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ SAssignNew(SubtitleText, STextBlock).Font(Font(22)).ColorAndOpacity(Sand).AutoWrapText(true).Justification(ETextJustify::Center) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 28.f, 0, 0)[ BrightnessBar(24) ]
			]
		]
	];
	SetVisibility(EVisibility::Visible);
}

TSharedRef<SWidget> SFOHud::BrightnessBar(int32 FontSize)
{
	const FLinearColor Sand(0.95f, 0.85f, 0.65f);
	TSharedPtr<STextBlock> Label;
	auto Btn = [&](const FText& T, float Step) {
		return SNew(SBox).WidthOverride(FontSize * 2.6f).HeightOverride(FontSize * 2.2f)
		[
			SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.12f, 0.6f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
			.OnPressed_Lambda([this, Step]() { if (GM.IsValid()) GM->AdjustBrightness(Step); })
			[ SNew(STextBlock).Text(T).Font(Font(FontSize)).ColorAndOpacity(Sand) ]
		];
	};
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0)[ Btn(FText::FromString(TEXT("\u2212")), 0.8f) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0)[ SAssignNew(Label, STextBlock).Font(Font(FontSize)).ColorAndOpacity(Sand).ShadowOffset(FVector2D(1, 1)) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0)[ Btn(FText::FromString(TEXT("+")), 1.25f) ];
	if (!BrightText.IsValid()) BrightText = Label; else BrightText2 = Label;
	return Row;
}

FSlateColor SFOHud::FlashColor() const { return FlashCol; }
EVisibility SFOHud::OverlayVis() const { return (GM.IsValid() && GM->State == EFOState::Playing) ? EVisibility::Collapsed : EVisibility::Visible; }
EVisibility SFOHud::HudVis() const { return (GM.IsValid() && GM->State == EFOState::Playing) ? EVisibility::Visible : EVisibility::Collapsed; }

void SFOHud::Tick(const FGeometry& G, const double T, const float Dt)
{
	SCompoundWidget::Tick(G, T, Dt);
	if (!GM.IsValid()) return;
	AFOCharacter* P = GM->Player();
	if (P)
	{
		HealthFrac = FMath::FInterpTo(HealthFrac, P->Health / 100.f, Dt, 8.f);
		HealthFill->SetWidthOverride(FMath::Max(0.f, 296.f * HealthFrac));
		AmmoText->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), P->Ammo, P->Reserve)));
	}
	KillsText->SetText(FText::FromString(FString::Printf(TEXT("قتلى %d"), GM->Kills)));
	FuelText->SetText(FText::FromString(FString::Printf(TEXT("وقود %d/%d"), GM->Fuel, AFOGameMode::FuelNeeded)));
	ObjectiveText->SetText(FText::FromString(GM->Objective));
	{
		const FText BT = FText::FromString(FString::Printf(TEXT("سطوع %d%%"), FMath::RoundToInt(GM->CurrentBrightness() * 100.f)));
		if (BrightText.IsValid()) BrightText->SetText(BT);
		if (BrightText2.IsValid()) BrightText2->SetText(BT);
	}
	const float A = FMath::Clamp(GM->DamageFlash * 1.6f, 0.f, 0.45f);
	FlashCol = P && P->Health < 100.f && GM->DamageFlash > 0.f ? FLinearColor(0.6f, 0.f, 0.f, A) : FLinearColor(0.f, 0.f, 0.f, 0.f);
	switch (GM->State)
	{
	case EFOState::Menu:
		TitleText->SetText(LOCTEXT("Title", "وهران: السقوط"));
		SubtitleText->SetText(LOCTEXT("Intro", "الميناء هو المخرج الوحيد. اجمع 3 عبوات وقود لفتح البوابة.\nالمس الشاشة للبدء"));
		break;
	case EFOState::Dead:
		TitleText->SetText(LOCTEXT("Dead", "لقد مُتّ"));
		SubtitleText->SetText(FText::FromString(GM->Subtitle));
		break;
	case EFOState::Won:
		TitleText->SetText(LOCTEXT("Won", "نجوت"));
		SubtitleText->SetText(FText::FromString(GM->Subtitle));
		break;
	default: break;
	}
}

FReply SFOHud::HandleTap()
{
	if (GM.IsValid() && GM->State != EFOState::Playing) { GM->StartGame(); return FReply::Handled(); }
	return FReply::Unhandled();
}
FReply SFOHud::OnMouseButtonDown(const FGeometry&, const FPointerEvent&) { return HandleTap(); }
FReply SFOHud::OnTouchStarted(const FGeometry&, const FPointerEvent&) { return HandleTap(); }
FReply SFOHud::OnFirePressed()
{
	if (GM.IsValid()) if (AFOCharacter* P = GM->Player()) P->TryShoot();
	return FReply::Handled();
}
#undef LOCTEXT_NAMESPACE
