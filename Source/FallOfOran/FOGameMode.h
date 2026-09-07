#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/FOTypes.h"
#include "FOGameMode.generated.h"

class AFOCharacter;
class AFOWorldBuilder;
class UFOMissionComponent;
class UFOChallengeComponent;
class AFOKeypadPuzzle;
class SFOHud;

UENUM()
enum class EFOState : uint8 { Menu, Playing, Dead, Won };

/**
 * Per-level flow controller (one instance per loaded map):
 *   Menu -> Playing -> Dead | Won.
 * Owns the mission (objectives), the HUD, and is the single entry point for gameplay events
 * (ReportEvent). Level content comes from UFOGameInstance::CurrentLevel(); nothing here is level-specific.
 */
UCLASS()
class AFOGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AFOGameMode();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float Dt) override;

	// ---- flow
	void StartGame();          // tap on menu / death / win screen
	void Restart();            // reload current level
	void SelectRelativeLevel(int32 Delta); // menu: ◄ ►
	void SelectRelativeMode(int32 Delta);  // menu: campaign / survival / supply run
	void ReturnToMenu();                   // death / win screen: back to the mode+level menu
	void Win();
	/** Challenge failure (timer expired, invalid definition, ...). Campaign uses OnPlayerDied only. */
	void FailChallenge(const FString& Reason);

	// ---- events (the only way gameplay code talks to the mission)
	void ReportEvent(const FFOGameEvent& E);
	void OnPlayerDamaged();
	void OnPlayerDied();
	void OnPuzzleFailed(const FString& Message, int32 PenaltyZombies);
	bool IsPuzzleActive(FName Id) const;
	/** Mode-aware exit/extraction gate: campaign Reach objective, or the challenge extraction rule. */
	bool IsExitUsable() const;

	// ---- HUD helpers
	void SetHint(const FString& Text, float Seconds = 4.f);
	void ShowNote(const FString& Text, float Seconds = 6.f);
	FString GetObjectiveText() const;
	void AdjustBrightness(float Step);
	float CurrentBrightness() const;
	const FFOLevelDef* GetLevel() const { return Level; }
	int32 GetLevelIndex() const { return LevelIndex; }

	// ---- keypad UI (driven by AFOKeypadPuzzle + SFOHud)
	void OpenKeypad(AFOKeypadPuzzle* Pad);
	void KeypadPress(TCHAR Digit);
	void KeypadBackspace();
	void KeypadSubmit();
	void CloseKeypad();
	bool IsKeypadOpen() const { return ActiveKeypad != nullptr; }
	FString KeypadInput;
	int32 KeypadLength() const;

	void SpawnZombiesBehindPlayer(int32 Count);
	AFOCharacter* Player() const;

	EFOState State = EFOState::Menu;
	int32 Kills = 0;
	float LevelTime = 0.f;
	float DamageFlash = 0.f;
	float HealFlash = 0.f;   // green screen flash on medkit pickup (was reusing the red damage flash)
	FString Hint; float HintTimer = 0.f;
	FString NoteText; float NoteTimer = 0.f;
	FString Subtitle;
	float RestartTimer = 0.f;

	// ---- challenge modes
	bool IsChallenge() const { return Flow != EFOFlowMode::Campaign; }
	EFOFlowMode FlowMode() const { return Flow; }
	UFOChallengeComponent* GetChallenge() const { return Challenge; }
	/** Wave / timer line for the HUD (empty in campaign). */
	FString GetChallengeStatusLine() const;
	FString GetModeName() const;

	UPROPERTY() UFOMissionComponent* Mission = nullptr;
	UPROPERTY() UFOChallengeComponent* Challenge = nullptr;
	UPROPERTY() AFOWorldBuilder* World = nullptr;
	UPROPERTY() AFOKeypadPuzzle* ActiveKeypad = nullptr;
	TSharedPtr<SFOHud> Hud;

	// Automated screenshot tour (-FOShots [-FOShotMax=N]) for headless proof renders.
	bool bShotMode = false;
	// Logic self-test (-FOSelfTest): drives the mission with synthetic events and puzzle DebugSolve(); logs SELFTEST PASS/FAIL and exits.
	bool bSelfTest = false;
	bool bSelfTestExitPrepared = false;
	float SelfTestClock = 0.f;
	void TickSelfTest(float Dt);
	float ShotClock = 0.f;
	int32 ShotIndex = 0;
	void TickShots(float Dt);
	void RunEncounterValidation(); // explicit synthetic -FOEncounterTest only
	void RunChallengeValidation(); // explicit synthetic -FOChallengeTest only (challenge rules, retry/save isolation, wave progression)

private:
	const FFOLevelDef* Level = nullptr;
	int32 LevelIndex = 0;
	bool bCampaignDone = false;
	EFOFlowMode Flow = EFOFlowMode::Campaign;
	FName ChallengeId;
	/** Backing storage for the synthesized challenge level (Level points at it in challenge modes). */
	FFOLevelDef ChallengeLevel;
	bool bChallengeTestRan = false;
};
