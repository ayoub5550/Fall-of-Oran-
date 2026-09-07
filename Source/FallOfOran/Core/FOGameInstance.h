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

/** Disk format for challenge-mode personal bests (slot "fo_challenge"). Deliberately a SEPARATE
 *  slot so a challenge can never corrupt or unlock campaign progress. */
UCLASS()
class UFOChallengeSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() FFOChallengeProgress Progress;
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

	// ---------------------------------------------------------------- mode selection
	/** Which flow the next map load plays: campaign or one of the challenge modes. Session state
	 *  plus a one-line record in the challenge slot; never written into the campaign save. */
	EFOFlowMode FlowMode() const { return Mode; }
	void SetFlowMode(EFOFlowMode NewMode);
	/** Cycle Campaign -> Survival -> SupplyRun -> Campaign (menu ◄ ►). */
	void CycleFlowMode(int32 Delta);
	/** Definition for the selected challenge mode, or nullptr in campaign mode. */
	const FFOChallengeDef* CurrentChallenge() const;
	/** Records an attempt / result for a challenge. Campaign progress is untouched. */
	void OnChallengeFinished(FName Id, bool bCleared, int32 WavesCleared, int32 Kills, float Seconds, float TimeLeft);
	void NoteChallengeAttempt(FName Id);
	const FFOChallengeRecord* ChallengeRecord(FName Id) const;
	void SaveChallenges();
	void LoadChallenges();
	void ResetChallengeRecords();

	FFOChallengeProgress Challenges;
	static const TCHAR* ChallengeSlotName;

	FFOProgress Progress;
	static const TCHAR* SlotName;

private:
	EFOFlowMode Mode = EFOFlowMode::Campaign;
	FFOChallengeRecord& MutableRecord(FName Id);
};
