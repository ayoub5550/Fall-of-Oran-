#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AFOGameMode;
class STextBlock;
class SBox;
class SBorder;

/**
 * Slate HUD built in code (no UMG assets). Reads AFOGameMode every frame (no delegates needed for a
 * single-player HUD). Layers: flash | in-game HUD | hint banner | note panel | keypad | menu overlay.
 */
class SFOHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFOHud) {}
		SLATE_ARGUMENT(AFOGameMode*, GameMode)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& G, const double T, const float Dt) override;
	virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override;
	virtual FReply OnTouchStarted(const FGeometry& G, const FPointerEvent& E) override;
	virtual bool SupportsKeyboardFocus() const override { return false; }
private:
	FReply HandleTap();
	TSharedRef<SWidget> BrightnessBar(int32 FontSize);
	TSharedRef<SWidget> KeypadPanel();
	TSharedRef<SWidget> HudButton(const FText& Label, int32 FontSize, float W, float H, const FLinearColor& Col, TFunction<void()> OnPress);
	FSlateFontInfo Font(int32 Size) const;
	FSlateColor FlashColor() const;
	FSlateColor SprintColor() const;
	EVisibility OverlayVis() const;
	EVisibility HudVis() const;
	EVisibility HintVis() const;
	EVisibility NoteVis() const;
	EVisibility KeypadVis() const;
	EVisibility InteractVis() const;
	EVisibility MenuOnlyVis() const;
	EVisibility CampaignMenuVis() const;
	EVisibility EndScreenVis() const;
	EVisibility ChallengeVis() const;

	TWeakObjectPtr<AFOGameMode> GM;
	TSharedPtr<STextBlock> AmmoText, KillsText, LevelText, ChallengeStatusText, ModeText, ObjectiveText, TitleText, SubtitleText, HintText, NoteTextBlock, InteractText, KeypadText, BrightText, BrightText2;
	TSharedPtr<SBox> HealthFill;
	float HealthFrac = 1.f;
	FLinearColor FlashCol = FLinearColor::Transparent;
};
