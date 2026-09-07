#pragma once
#include "CoreMinimal.h"
#include "FOTypes.generated.h"

/**
 * Shared data types for Fall of Oran.
 * Everything gameplay-related is DATA-DRIVEN: a level is an FFOLevelDef, an objective is an
 * FFOObjectiveDef, a puzzle is an FFOPuzzleDef. Code reads these; it never hard-codes level content.
 * Add a level  -> Core/FOLevelRegistry.cpp   (no C++ elsewhere needs to change)
 * Add a puzzle -> Puzzles/ (subclass AFOPuzzleBase) + one EFOPuzzleType entry
 * Add an objective type -> Mission/FOObjective.cpp (subclass UFOObjective) + one EFOObjectiveType entry
 */

// ------------------------------------------------------------------ gameplay events
/** Everything that happens in the world is reported as one of these to AFOGameMode::ReportEvent. */
UENUM()
enum class EFOGameEvent : uint8
{
	ZombieKilled,   // Tag = zombie variant name
	ItemCollected,  // Tag = item id (e.g. "fuel", "note"), Count = amount
	PuzzleSolved,   // Tag = puzzle id
	ExitReached,    // player entered the exit zone
	NoteRead,       // Tag = note id
	PlayerDamaged,
	PlayerDied,
};

USTRUCT()
struct FFOGameEvent
{
	GENERATED_BODY()
	EFOGameEvent Type = EFOGameEvent::ZombieKilled;
	FName Tag;
	int32 Count = 1;
	FVector Location = FVector::ZeroVector;
	FFOGameEvent() {}
	FFOGameEvent(EFOGameEvent InType, FName InTag = NAME_None, int32 InCount = 1) : Type(InType), Tag(InTag), Count(InCount) {}
};

// ------------------------------------------------------------------ objectives
UENUM()
enum class EFOObjectiveType : uint8
{
	Collect,      // collect Count items whose id == Tag        (fuel cans, notes)
	Kill,         // kill Count zombies
	SolvePuzzle,  // puzzle with id == Tag reports PuzzleSolved
	Reach,        // reach the exit zone (ExitReached)
};

USTRUCT()
struct FFOObjectiveDef
{
	GENERATED_BODY()
	UPROPERTY() EFOObjectiveType Type = EFOObjectiveType::Collect;
	UPROPERTY() FName Tag;
	UPROPERTY() int32 Count = 1;
	/** HUD text; "{n}" and "{max}" are replaced with progress. Arabic. */
	UPROPERTY() FString Text;
	/** Shown once when this objective becomes active (optional, 4 s). */
	UPROPERTY() FString StartHint;
	/** Shown when the objective completes (optional). */
	UPROPERTY() FString DoneHint;
};

/** A stage = objectives that run in parallel. Stages run sequentially. */
USTRUCT()
struct FFOMissionStage
{
	GENERATED_BODY()
	UPROPERTY() TArray<FFOObjectiveDef> Objectives;
};

// ------------------------------------------------------------------ puzzles
UENUM()
enum class EFOPuzzleType : uint8
{
	BreakerSequence,  // press N breakers in the right order (order written on a note)
	Keypad,           // enter a numeric code; digits are found on notes scattered in the level
	Generator,        // fueled by the previous mission stage; defend nearby during startup
};

USTRUCT()
struct FFOPuzzleDef
{
	GENERATED_BODY()
	UPROPERTY() EFOPuzzleType Type = EFOPuzzleType::BreakerSequence;
	UPROPERTY() FName Id;
	UPROPERTY() FVector Location = FVector::ZeroVector;   // where the puzzle actor stands (street coords)
	UPROPERTY() float Yaw = 0.f;
	/** Breaker: number of switches. Keypad: number of digits. */
	UPROPERTY() int32 Size = 4;
	/** Keypad: the code (digits). Breaker: unused (order is seeded). */
	UPROPERTY() FString Code;
	/** Wrong attempt penalty: zombies spawned behind the player. */
	UPROPERTY() int32 PenaltyZombies = 2;
	/** Notes spawned for this puzzle (each reveals one hint / digit). */
	UPROPERTY() TArray<FVector> NoteLocations;
};

// ------------------------------------------------------------------ spawns
UENUM()
enum class EFOItem : uint8 { Fuel, Health, Ammo, Note, Supply };

USTRUCT()
struct FFOItemSpawn
{
	GENERATED_BODY()
	UPROPERTY() EFOItem Item = EFOItem::Ammo;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() FName Tag;      // objective tag ("fuel"); notes use "note"
	UPROPERTY() FString Text;   // notes: content shown when read
};

USTRUCT()
struct FFOLightingPreset
{
	GENERATED_BODY()
	/** Phones without eye adaptation render ~3x brighter than the sandbox: keep ≈2 / ≈1.2 (AGENT.md §6). */
	UPROPERTY() float MoonIntensity = 2.f;
	UPROPERTY() FLinearColor MoonColor = FLinearColor(0.6f, 0.7f, 0.92f);
	UPROPERTY() float SkyLightIntensity = 1.2f;
	UPROPERTY() float FogDensity = 0.014f;
	UPROPERTY() FLinearColor FogColor = FLinearColor(0.05f, 0.07f, 0.12f);
	UPROPERTY() bool bLightning = true;
};

// ------------------------------------------------------------------ level
USTRUCT()
struct FFOLevelDef
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() FString Title;        // Arabic, shown in menu + intro
	UPROPERTY() FString Intro;        // one-line briefing
	UPROPERTY() int32 Seed = 1;       // world generator seed (buildings, debris, palettes)
	UPROPERTY() float StreetLength = 9000.f;
	UPROPERTY() int32 ZombieCount = 16;
	UPROPERTY() float RunnerChance = 0.2f;
	UPROPERTY() float ZombieHpMul = 1.f;
	UPROPERTY() FFOLightingPreset Lighting;
	UPROPERTY() TArray<FFOItemSpawn> Items;
	UPROPERTY() TArray<FFOPuzzleDef> Puzzles;
	UPROPERTY() TArray<FFOMissionStage> Stages;
	/** Text shown on the win screen of this level. */
	UPROPERTY() FString OutroText;
	/** Challenge modes: build the compact authored arena dressing instead of a long empty street. */
	UPROPERTY() bool bChallengeArena = false;
};


// ------------------------------------------------------------------ challenge modes
/**
 * Selectable game flows. Campaign = the unchanged 3-stage story levels (FFOLevelRegistry).
 * Survival / SupplyRun = bounded challenge modes defined by FFOChallengeDef (FFOChallengeRegistry),
 * driven by UFOChallengeComponent. Challenges never unlock campaign levels and never write the
 * campaign save slot.
 */
UENUM()
enum class EFOFlowMode : uint8
{
	Campaign,
	Survival,
	SupplyRun,
};

/** One Survival wave. Waves are bounded and escalate; MaxLive caps simultaneous live enemies. */
USTRUCT()
struct FFOSurvivalWave
{
	GENERATED_BODY()
	/** Total enemies that must actually die for this wave to be cleared. */
	UPROPERTY() int32 Enemies = 4;
	UPROPERTY() int32 MaxLive = 4;            // never more than this alive at once
	UPROPERTY() float RunnerChance = 0.f;
	UPROPERTY() float HeavyChance = 0.f;      // police silhouette (durable, slow)
	UPROPERTY() float HpMul = 1.f;
	UPROPERTY() float SpawnInterval = 1.2f;   // seconds between individual spawns
	/** Resupply / regroup window after the wave is cleared (0 for the last wave). */
	UPROPERTY() float RestSeconds = 12.f;
	UPROPERTY() FString Banner;               // HUD hint when the wave starts
};

USTRUCT()
struct FFOChallengeDef
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() EFOFlowMode Mode = EFOFlowMode::Survival;
	UPROPERTY() FString Title;
	UPROPERTY() FString Intro;
	UPROPERTY() FString Rules;        // menu text: how the mode is won/lost
	UPROPERTY() FString OutroText;    // win screen
	UPROPERTY() int32 Seed = 4242;
	/** Compact authored arena; challenges use a short street so there is no long empty walk. */
	UPROPERTY() float StreetLength = 3200.f;
	UPROPERTY() FFOLightingPreset Lighting;
	/** Enemies never spawn closer than this to the player (fair spawn radius, cm). */
	UPROPERTY() float SafeSpawnRadius = 1100.f;
	/** Pickups (health / ammo) respawned at every resupply point at each wave break / run start. */
	UPROPERTY() TArray<FVector> ResupplyPoints;

	// -------- Survival
	UPROPERTY() TArray<FFOSurvivalWave> Waves;
	UPROPERTY() float FirstWaveDelay = 4.f;

	// -------- Supply Run
	/** Tagged supply crates ("supply", NOT "fuel": fuel triggers the campaign ambush rule). */
	UPROPERTY() TArray<FVector> SupplyPoints;
	UPROPERTY() int32 SupplyTarget = 4;
	UPROPERTY() float TimeLimitSeconds = 240.f;
	/** Ambient roamers kept alive at the same time (they respawn while the clock runs). */
	UPROPERTY() int32 AmbientLive = 5;
	UPROPERTY() float AmbientRunnerChance = 0.35f;
	UPROPERTY() float AmbientHpMul = 1.f;
	UPROPERTY() float AmbientRespawnSeconds = 6.f;

	/** Cheap structural validation; an invalid definition must never be playable (no instant win). */
	bool IsValid() const
	{
		if (Id.IsNone() || StreetLength < 800.f || SafeSpawnRadius < 600.f) return false;
		if (Mode == EFOFlowMode::Survival)
		{
			if (Waves.Num() == 0) return false;
			for (const FFOSurvivalWave& W : Waves) if (W.Enemies <= 0 || W.MaxLive <= 0) return false;
			return true;
		}
		if (Mode == EFOFlowMode::SupplyRun)
			return SupplyTarget > 0 && SupplyPoints.Num() >= SupplyTarget && TimeLimitSeconds > 10.f;
		return false;
	}
};

/** Isolated per-challenge personal best. Stored in its own save slot; campaign data is untouched. */
USTRUCT()
struct FFOChallengeRecord
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() bool bCleared = false;
	UPROPERTY() int32 BestWave = 0;          // survival: highest wave cleared
	UPROPERTY() int32 BestKills = 0;
	UPROPERTY() float BestClearSeconds = 0.f; // 0 = never cleared
	UPROPERTY() float BestTimeLeft = 0.f;     // supply run: most time left on the clock
	UPROPERTY() int32 Attempts = 0;
};

USTRUCT()
struct FFOChallengeProgress
{
	GENERATED_BODY()
	UPROPERTY() TArray<FFOChallengeRecord> Records;
};

// ------------------------------------------------------------------ persistent progress
USTRUCT()
struct FFOProgress
{
	GENERATED_BODY()
	UPROPERTY() int32 UnlockedLevels = 1;           // levels [0, UnlockedLevels) are playable
	UPROPERTY() int32 SelectedLevel = 0;
	UPROPERTY() TArray<int32> BestKills;            // per level
	UPROPERTY() TArray<float> BestTimeSeconds;      // per level, 0 = never finished
	UPROPERTY() float Brightness = 1.f;              // user light multiplier (0.25 .. 4), see AFOWorldBuilder::ApplyBrightness
};
