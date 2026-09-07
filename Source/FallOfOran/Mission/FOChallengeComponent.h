#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/FOTypes.h"
#include "FOChallengeComponent.generated.h"

class AFOZombie;
class AFOPickup;

DECLARE_MULTICAST_DELEGATE_OneParam(FFOChallengeHint, const FString& /*Text*/);
DECLARE_MULTICAST_DELEGATE(FFOChallengeCleared);
DECLARE_MULTICAST_DELEGATE_OneParam(FFOChallengeFailed, const FString& /*Reason*/);

UENUM()
enum class EFOChallengePhase : uint8
{
	Idle,        // built, not started
	Countdown,   // survival: pre-first-wave breather
	WaveActive,  // survival: enemies spawning / alive
	WaveRest,    // survival: resupply window between waves
	RunActive,   // supply run: clock ticking
	Cleared,     // won
	Failed,      // lost (timer, death, or invalid definition)
};

/**
 * Rules engine for the challenge modes (Survival, Supply Run). Lives on AFOGameMode next to
 * UFOMissionComponent, which stays exclusively the campaign's stage machine.
 *
 * Guarantees that the tests in FOChallengeValidation.cpp assert:
 *   - wave progression counts ACTUAL dead spawned enemies (a spoofed ZombieKilled event cannot advance a wave)
 *   - live enemies never exceed the wave's MaxLive
 *   - enemies never spawn inside FFOChallengeDef::SafeSpawnRadius of the player
 *   - an invalid / empty definition fails instead of instantly completing
 *   - Supply Run only completes when the extraction zone is physically reached after all supplies are held
 *
 * Time is advanced explicitly through Advance(Dt) from AFOGameMode::Tick, so tests can simulate
 * exact deltas without relying on real frame timing.
 */
UCLASS()
class UFOChallengeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	/** Begin the challenge. An invalid definition fails immediately (never silently completes). */
	void Start(const FFOChallengeDef& InDef);
	/** Simulate Dt seconds of challenge logic. Safe to call with 0. */
	void Advance(float Dt);
	/** Gameplay events routed from AFOGameMode::ReportEvent. */
	void HandleEvent(const FFOGameEvent& E);
	/** Player death: turns into a challenge failure. */
	void NotifyPlayerDied();
	/** Removes every enemy this component spawned (used on failure / teardown / tests). */
	void ClearSpawnedEnemies();

	// ---- state queries
	EFOChallengePhase Phase() const { return CurrentPhase; }
	bool IsActive() const { return CurrentPhase == EFOChallengePhase::Countdown || CurrentPhase == EFOChallengePhase::WaveActive || CurrentPhase == EFOChallengePhase::WaveRest || CurrentPhase == EFOChallengePhase::RunActive; }
	bool IsCleared() const { return CurrentPhase == EFOChallengePhase::Cleared; }
	bool IsFailed() const { return CurrentPhase == EFOChallengePhase::Failed; }
	bool IsExtractionOpen() const;
	int32 WaveNumber() const { return WaveIndex + 1; }
	int32 WaveCount() const { return Def.Waves.Num(); }
	int32 WavesCleared() const { return ClearedWaves; }
	int32 LiveEnemies() const;
	int32 DeadThisWave() const;
	int32 WaveTarget() const { return Def.Waves.IsValidIndex(WaveIndex) ? Def.Waves[WaveIndex].Enemies : 0; }
	int32 SuppliesHeld() const { return SupplyCollected; }
	int32 SupplyTarget() const { return Def.SupplyTarget; }
	float TimeRemaining() const { return Clock; }
	float PhaseTimer() const { return PhaseClock; }
	float ElapsedSeconds() const { return Elapsed; }
	const FFOChallengeDef& Definition() const { return Def; }
	/** Multi-line HUD objective text (Arabic). */
	FString GetObjectiveText() const;
	/** Short status line: wave / timer. */
	FString GetStatusLine() const;
	FString FailureReason() const { return Reason; }

	FFOChallengeHint OnHint;
	FFOChallengeCleared OnCleared;
	FFOChallengeFailed OnFailed;

	/** Test seam: skip the countdown / rest window. */
	void ForceEndPhaseTimer() { PhaseClock = 0.f; }
	/** Test seam: the exact spawn locations chosen so far. */
	const TArray<FVector>& SpawnLog() const { return SpawnLocations; }
	int32 ResupplyDrops() const { return ResupplyCount; }
	/** Enemies that disappeared without dying (never counted as kills). */
	int32 LostEnemyCount() const { return LostEnemies; }

private:
	void EnterCountdown();
	void BeginWave(int32 Index);
	void EnterRest();
	void FinishCleared();
	void Fail(const FString& InReason);
	void TickSurvival(float Dt);
	void TickSupplyRun(float Dt);
	void SpawnEnemy(float RunnerChance, float HeavyChance, float HpMul, bool bWaveEnemy);
	void DropResupply();
	bool ChooseSpawnLocation(FVector& Out) const;
	void PruneEnemies();

	FFOChallengeDef Def;
	EFOChallengePhase CurrentPhase = EFOChallengePhase::Idle;
	FString Reason;

	// survival
	int32 WaveIndex = 0;
	int32 ClearedWaves = 0;
	int32 SpawnedThisWave = 0;
	int32 DeadCounted = 0;              // actual confirmed deaths of THIS wave's spawned enemies
	float SpawnClock = 0.f;
	float PhaseClock = 0.f;             // countdown / rest remaining

	// supply run
	int32 SupplyCollected = 0;
	float Clock = 0.f;                  // remaining seconds
	float AmbientClock = 0.f;

	float Elapsed = 0.f;
	int32 ResupplyCount = 0;
	int32 LostEnemies = 0;
	TArray<TWeakObjectPtr<AFOZombie>> WaveEnemies;   // this wave (or ambient roamers)
	TArray<TWeakObjectPtr<AFOZombie>> AllSpawned;    // for teardown
	TArray<TWeakObjectPtr<AFOPickup>> DroppedPickups;
	TArray<FVector> SpawnLocations;
	mutable FRandomStream Rng;
};
