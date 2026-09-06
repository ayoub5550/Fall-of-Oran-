#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AFOGameMode;
class STextBlock;
class SBox;

/** Slate HUD built in code (no UMG assets): health bar, ammo, kills, fuel, objective, fire button, menus. */
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
	FReply OnFirePressed();
	TSharedRef<SWidget> BrightnessBar(int32 FontSize);
	TSharedPtr<STextBlock> BrightText, BrightText2;
	TWeakObjectPtr<AFOGameMode> GM;
	TSharedPtr<STextBlock> AmmoText, KillsText, FuelText, ObjectiveText, TitleText, SubtitleText;
	TSharedPtr<SBox> HealthFill;
	TSharedPtr<SWidget> Overlay, Flash, GameHud;
	FSlateFontInfo Font(int32 Size) const;
	FSlateColor FlashColor() const;
	EVisibility OverlayVis() const;
	EVisibility HudVis() const;
	float HealthFrac = 1.f;
	FLinearColor FlashCol = FLinearColor::Transparent;
};
