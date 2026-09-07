#pragma once
#include "CoreMinimal.h"
#include "FOTypes.h"

/**
 * The selectable challenge modes (data-driven, same spirit as FFOLevelRegistry).
 * Index 0 = Survival "خمس موجات", index 1 = Supply Run "خط الإمداد".
 * Adding a mode variant = one more FFOChallengeDef here; no other C++ has to change.
 *
 * Design rationale (see docs/CHALLENGE_MODES.md):
 *   - bounded waves with an explicit calm resupply window between them (intensity / relief pacing)
 *   - the timed run only pays out when the player physically reaches extraction
 */
class FFOChallengeRegistry
{
public:
	static const TArray<FFOChallengeDef>& All();
	static const FFOChallengeDef* Get(int32 Index);
	static const FFOChallengeDef* Find(FName Id);
	static const FFOChallengeDef* FindByMode(EFOFlowMode Mode);
	static int32 Num() { return All().Num(); }
	/** Compact playable level description used by AFOWorldBuilder for a challenge. */
	static FFOLevelDef MakeLevelDef(const FFOChallengeDef& Def);
};
