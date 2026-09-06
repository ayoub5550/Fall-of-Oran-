#include "FOHud.h"
#include "FOGameMode.h"
#include "FOCharacter.h"
#include "Core/FOLevelRegistry.h"
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

static const FLinearColor GRed(0.75f, 0.08f, 0.06f);
static const FLinearColor GSand(0.95f, 0.85f, 0.65f);
static const FLinearColor GPanel(0.f, 0.f, 0.f, 0.72f);

TSharedRef<SWidget> SFOHud::HudButton(const FText& Label, int32 FontSize, float W, float H, const FLinearColor& Col, TFunction<void()> OnPress)
{
	return SNew(SBox).WidthOverride(W).HeightOverride(H)
	[
		SNew(SButton).ButtonColorAndOpacity(Col).HAlign(HAlign_Center).VAlign(VAlign_Center)
		.OnPressed_Lambda([OnPress]() { if (OnPress) OnPress(); })
		[ SNew(STextBlock).Text(Label).Font(Font(FontSize)).ColorAndOpacity(GSand) ]
	];
}

TSharedRef<SWidget> SFOHud::BrightnessBar(int32 FontSize)
{
	TSharedPtr<STextBlock> Label;
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0)[ HudButton(FText::FromString(TEXT("\u2212")), FontSize, FontSize * 2.6f, FontSize * 2.2f, FLinearColor(0.1f, 0.1f, 0.12f, 0.6f), [this]() { if (GM.IsValid()) GM->AdjustBrightness(0.8f); }) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0)[ SAssignNew(Label, STextBlock).Font(Font(FontSize)).ColorAndOpacity(GSand).ShadowOffset(FVector2D(1, 1)) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0)[ HudButton(FText::FromString(TEXT("+")), FontSize, FontSize * 2.6f, FontSize * 2.2f, FLinearColor(0.1f, 0.1f, 0.12f, 0.6f), [this]() { if (GM.IsValid()) GM->AdjustBrightness(1.25f); }) ];
	if (!BrightText.IsValid()) BrightText = Label; else BrightText2 = Label;
	return Row;
}

TSharedRef<SWidget> SFOHud::KeypadPanel()
{
	const FLinearColor Key(0.12f, 0.12f, 0.15f, 0.9f);
	TSharedRef<SVerticalBox> Grid = SNew(SVerticalBox);
	Grid->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 10.f)[ SNew(STextBlock).Text(LOCTEXT("KeypadTitle", "أدخل الشيفرة")).Font(Font(24)).ColorAndOpacity(GSand) ];
	Grid->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 14.f)
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.02f, 0.08f, 0.04f, 0.95f)).Padding(FMargin(24.f, 8.f))
		[ SAssignNew(KeypadText, STextBlock).Font(Font(40)).ColorAndOpacity(FLinearColor(0.3f, 1.f, 0.4f)) ]
	];
	const TCHAR* Rows[] = { TEXT("123"), TEXT("456"), TEXT("789") };
	for (const TCHAR* R : Rows)
	{
		TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox);
		for (int32 i = 0; i < 3; i++)
		{
			const TCHAR D = R[i];
			Line->AddSlot().AutoWidth().Padding(4.f)[ HudButton(FText::FromString(FString::Chr(D)), 30, 86.f, 70.f, Key, [this, D]() { if (GM.IsValid()) GM->KeypadPress(D); }) ];
		}
		Grid->AddSlot().AutoHeight().HAlign(HAlign_Center)[ Line ];
	}
	Grid->AddSlot().AutoHeight().HAlign(HAlign_Center)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f)[ HudButton(FText::FromString(TEXT("\u232B")), 30, 86.f, 70.f, FLinearColor(0.35f, 0.15f, 0.05f, 0.9f), [this]() { if (GM.IsValid()) GM->KeypadBackspace(); }) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f)[ HudButton(FText::FromString(TEXT("0")), 30, 86.f, 70.f, Key, [this]() { if (GM.IsValid()) GM->KeypadPress(TCHAR('0')); }) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f)[ HudButton(LOCTEXT("KeypadClose", "إغلاق"), 22, 86.f, 70.f, FLinearColor(0.4f, 0.05f, 0.05f, 0.9f), [this]() { if (GM.IsValid()) GM->CloseKeypad(); }) ]
	];
	return SNew(SBorder).Visibility(this, &SFOHud::KeypadVis).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(GPanel).Padding(24.f)[ Grid ];
}

void SFOHud::Construct(const FArguments& InArgs)
{
	GM = InArgs._GameMode;
	ChildSlot
	[
		SNew(SOverlay)
		// damage / heal flash
		+ SOverlay::Slot()
		[ SNew(SImage).Visibility(EVisibility::HitTestInvisible).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(this, &SFOHud::FlashColor) ]
		// ---------------------------------------------------------------- in-game HUD
		+ SOverlay::Slot()
		[
			SNew(SOverlay).Visibility(this, &SFOHud::HudVis)
			// health bar + objectives (top-left)
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(28.f, 22.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).WidthOverride(300.f).HeightOverride(16.f)
					[
						SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f)).Padding(2.f).HAlign(HAlign_Left)
						[ SAssignNew(HealthFill, SBox).WidthOverride(296.f)[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(GRed) ] ]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8.f, 0, 0)
				[ SAssignNew(ObjectiveText, STextBlock).Visibility(EVisibility::HitTestInvisible).Font(Font(18)).ColorAndOpacity(GSand).ShadowOffset(FVector2D(1, 1)) ]
			]
			// kills + level (top-right)
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(28.f, 18.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[ SAssignNew(KillsText, STextBlock).Font(Font(22)).ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.85f)).ShadowOffset(FVector2D(1, 1)) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[ SAssignNew(LevelText, STextBlock).Font(Font(16)).ColorAndOpacity(FLinearColor(0.7f, 0.65f, 0.55f)).ShadowOffset(FVector2D(1, 1)) ]
			]
			// brightness (small, top-centre)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 14.f)[ BrightnessBar(16) ]
			// crosshair
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[ SNew(SBox).WidthOverride(6.f).HeightOverride(6.f)[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(FLinearColor(1, 1, 1, 0.8f)) ] ]
			// ammo + interact + fire (bottom-right)
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(40.f, 0.f, 40.f, 40.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 0, 0, 12.f)[ SAssignNew(AmmoText, STextBlock).Font(Font(30)).ColorAndOpacity(GSand).ShadowOffset(FVector2D(1, 1)) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 0, 0, 12.f)
				[
					SNew(SBox).Visibility(this, &SFOHud::InteractVis).WidthOverride(150.f).HeightOverride(64.f)
					[
						SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.1f, 0.35f, 0.5f, 0.75f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.OnPressed_Lambda([this]() { if (GM.IsValid()) if (AFOCharacter* P = GM->Player()) P->TryInteract(); })
						[ SAssignNew(InteractText, STextBlock).Font(Font(20)).ColorAndOpacity(GSand) ]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[ HudButton(LOCTEXT("Fire", "نار"), 34, 150.f, 150.f, FLinearColor(0.55f, 0.05f, 0.04f, 0.55f), [this]() { if (GM.IsValid()) if (AFOCharacter* P = GM->Player()) P->TryShoot(); }) ]
			]
			// hint banner (upper centre)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0, 70.f)
			[
				SNew(SBorder).Visibility(this, &SFOHud::HintVis).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.6f)).Padding(FMargin(22.f, 10.f))
				[ SAssignNew(HintText, STextBlock).Font(Font(22)).ColorAndOpacity(FLinearColor(1.f, 0.9f, 0.6f)).Justification(ETextJustify::Center).AutoWrapText(true) ]
			]
			// note panel (centre)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0, 0, 0, 120.f)
			[
				SNew(SBox).Visibility(this, &SFOHud::NoteVis).MaxDesiredWidth(560.f)
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.85f, 0.78f, 0.6f, 0.95f)).Padding(FMargin(26.f, 18.f))
					[ SAssignNew(NoteTextBlock, STextBlock).Font(Font(22)).ColorAndOpacity(FLinearColor(0.15f, 0.1f, 0.05f)).Justification(ETextJustify::Center).AutoWrapText(true) ]
				]
			]
			// keypad (centre)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[ KeypadPanel() ]
		]
		// ---------------------------------------------------------------- menu / death / win overlay
		+ SOverlay::Slot()
		[
			SNew(SBorder).Visibility(this, &SFOHud::OverlayVis).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(GPanel).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 14.f)[ SAssignNew(TitleText, STextBlock).Font(Font(64)).ColorAndOpacity(GRed).ShadowOffset(FVector2D(2, 2)) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ SAssignNew(SubtitleText, STextBlock).Font(Font(22)).ColorAndOpacity(GSand).AutoWrapText(true).Justification(ETextJustify::Center) ]
				// level chooser (menu only)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 22.f, 0, 0)
				[
					SNew(SHorizontalBox).Visibility(this, &SFOHud::MenuOnlyVis)
					+ SHorizontalBox::Slot().AutoWidth().Padding(6.f, 0)[ HudButton(FText::FromString(TEXT("\u25C0 السابق")), 20, 170.f, 56.f, FLinearColor(0.1f, 0.1f, 0.12f, 0.7f), [this]() { if (GM.IsValid()) GM->SelectRelativeLevel(-1); }) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(6.f, 0)[ HudButton(FText::FromString(TEXT("التالي \u25B6")), 20, 170.f, 56.f, FLinearColor(0.1f, 0.1f, 0.12f, 0.7f), [this]() { if (GM.IsValid()) GM->SelectRelativeLevel(+1); }) ]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 18.f, 0, 0)[ BrightnessBar(22) ]
			]
		]
	];
	SetVisibility(EVisibility::Visible);
}

FSlateColor SFOHud::FlashColor() const { return FlashCol; }
EVisibility SFOHud::OverlayVis() const { return (GM.IsValid() && GM->State == EFOState::Playing) ? EVisibility::Collapsed : EVisibility::Visible; }
EVisibility SFOHud::HudVis() const { return (GM.IsValid() && GM->State == EFOState::Playing) ? EVisibility::Visible : EVisibility::Collapsed; }
EVisibility SFOHud::HintVis() const { return (GM.IsValid() && !GM->Hint.IsEmpty()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }
EVisibility SFOHud::NoteVis() const { return (GM.IsValid() && !GM->NoteText.IsEmpty() && !GM->IsKeypadOpen()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }
EVisibility SFOHud::KeypadVis() const { return (GM.IsValid() && GM->IsKeypadOpen()) ? EVisibility::Visible : EVisibility::Collapsed; }
EVisibility SFOHud::MenuOnlyVis() const { return (GM.IsValid() && GM->State == EFOState::Menu) ? EVisibility::Visible : EVisibility::Collapsed; }
EVisibility SFOHud::InteractVis() const
{
	if (!GM.IsValid() || GM->IsKeypadOpen()) return EVisibility::Collapsed;
	const AFOCharacter* P = GM->Player();
	return (P && P->HasInteractTarget()) ? EVisibility::Visible : EVisibility::Collapsed;
}

void SFOHud::Tick(const FGeometry& G, const double T, const float Dt)
{
	SCompoundWidget::Tick(G, T, Dt);
	if (!GM.IsValid()) return;
	AFOCharacter* P = GM->Player();
	if (P)
	{
		HealthFrac = FMath::FInterpTo(HealthFrac, P->GetHealthFraction(), Dt, 8.f);
		HealthFill->SetWidthOverride(FMath::Max(0.f, 296.f * HealthFrac));
		AmmoText->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), P->GetAmmo(), P->GetReserve())));
		InteractText->SetText(FText::FromString(P->GetInteractPrompt()));
	}
	const FFOLevelDef* L = GM->GetLevel();
	KillsText->SetText(FText::FromString(FString::Printf(TEXT("قتلى %d"), GM->Kills)));
	LevelText->SetText(FText::FromString(L ? FString::Printf(TEXT("المستوى %d — %s"), GM->GetLevelIndex() + 1, *L->Title) : FString()));
	ObjectiveText->SetText(FText::FromString(GM->GetObjectiveText()));
	HintText->SetText(FText::FromString(GM->Hint));
	NoteTextBlock->SetText(FText::FromString(GM->NoteText));
	{
		FString Dots; for (int32 i = 0; i < GM->KeypadLength(); i++) Dots += i < GM->KeypadInput.Len() ? GM->KeypadInput.Mid(i, 1) : TEXT("_"); 
		KeypadText->SetText(FText::FromString(Dots));
		const FText BT = FText::FromString(FString::Printf(TEXT("سطوع %d%%"), FMath::RoundToInt(GM->CurrentBrightness() * 100.f)));
		if (BrightText.IsValid()) BrightText->SetText(BT);
		if (BrightText2.IsValid()) BrightText2->SetText(BT);
	}
	const float A = FMath::Clamp(GM->DamageFlash * 1.6f, 0.f, 0.45f);
	FlashCol = P && GM->DamageFlash > 0.f ? FLinearColor(0.6f, 0.f, 0.f, A) : FLinearColor(0.f, 0.f, 0.f, 0.f);
	switch (GM->State)
	{
	case EFOState::Menu:
		TitleText->SetText(LOCTEXT("Title", "وهران: السقوط"));
		SubtitleText->SetText(FText::FromString(L ? FString::Printf(TEXT("المستوى %d/%d: %s\n%s\nالمس الشاشة للبدء"), GM->GetLevelIndex() + 1, FFOLevelRegistry::Num(), *L->Title, *L->Intro) : FString(TEXT("المس الشاشة للبدء"))));
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
#undef LOCTEXT_NAMESPACE
