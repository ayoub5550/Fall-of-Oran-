#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "FOTypes.h"
#include "FOGameInstance.generated.h"

/** Disk format for campaign progress (slot "fo_progress"). */
UCLASS()
class UFOSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() FFOProgress Progress;
};

/**
 * Lives for the whole app session (survives level loads).
 * Owns campaign progress (unlocked levels, best scores) and the "which level do we play next" state.
 * Configured in DefaultEngine.ini: GameInstanceClass=/Script/FallOfOran.FOGameInstance
 */
UCLASS()
class UFOGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	virtual void Init() override;

	static UFOGameInstance* Get(const UObject* WorldContext);

	const FFOLevelDef* CurrentLevel() const;
	int32 CurrentLevelIndex() const { return Progress.SelectedLevel; }
	bool IsUnlocked(int32 Index) const { return Index >= 0 && Index < Progress.UnlockedLevels; }
	void SelectLevel(int32 Index);
	/** Called by the GameMode when a level is won. Unlocks the next one and records scores. */
	void OnLevelCompleted(int32 Index, int32 Kills, float Seconds);
	/** True when the last campaign level was just completed. */
	bool IsCampaignFinished() const;
	/** Advance SelectedLevel to the next level (clamped). Returns false if there is none. */
	bool AdvanceToNextLevel();
	/** One-shot handoff; not persisted. Reload remains, duplicate start menu does not. */
	bool bContinueIntoLevel = false;

	void SetBrightness(float B) { Progress.Brightness = FMath::Clamp(B, 0.25f, 4.f); Save(); }
	float Brightness() const { return Progress.Brightness; }

	void Save();
	void Load();
	void ResetProgress();

	FFOProgress Progress;
	static const TCHAR* SlotName;
};
