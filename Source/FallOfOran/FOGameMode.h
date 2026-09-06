#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/FOTypes.h"
#include "FOGameMode.generated.h"

class AFOCharacter;
class AFOWorldBuilder;
class UFOMissionComponent;
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
	virtual void Tick(float Dt) override;

	// ---- flow
	void StartGame();          // tap on menu / death / win screen
	void Restart();            // reload current level
	void SelectRelativeLevel(int32 Delta); // menu: ◄ ►
	void Win();

	// ---- events (the only way gameplay code talks to the mission)
	void ReportEvent(const FFOGameEvent& E);
	void OnPlayerDamaged();
	void OnPlayerDied();
	void OnPuzzleFailed(const FString& Message, int32 PenaltyZombies);
	bool IsPuzzleActive(FName Id) const;

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

	UPROPERTY() UFOMissionComponent* Mission = nullptr;
	UPROPERTY() AFOWorldBuilder* World = nullptr;
	UPROPERTY() AFOKeypadPuzzle* ActiveKeypad = nullptr;
	TSharedPtr<SFOHud> Hud;

	// Automated screenshot tour (-FOShots [-FOShotMax=N]) for headless proof renders.
	bool bShotMode = false;
	// Logic self-test (-FOSelfTest): drives the mission with synthetic events and puzzle DebugSolve(); logs SELFTEST PASS/FAIL and exits.
	bool bSelfTest = false;
	float SelfTestClock = 0.f;
	void TickSelfTest(float Dt);
	float ShotClock = 0.f;
	int32 ShotIndex = 0;
	void TickShots(float Dt);

private:
	const FFOLevelDef* Level = nullptr;
	int32 LevelIndex = 0;
	bool bCampaignDone = false;
};
