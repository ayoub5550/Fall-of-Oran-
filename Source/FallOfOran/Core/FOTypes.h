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
enum class EFOItem : uint8 { Fuel, Health, Ammo, Note };

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
