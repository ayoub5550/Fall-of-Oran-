#pragma once
#include "CoreMinimal.h"
#include "FOTypes.h"

/**
 * The campaign: an ordered list of FFOLevelDef.
 * Levels are defined in C++ (FOLevelRegistry.cpp) so the whole game is reproducible headless without
 * editor-authored assets. If/when a designer wants to edit levels in the editor, wrap the same struct
 * in a UDataAsset and load it here — nothing else in the codebase has to change.
 */
class FFOLevelRegistry
{
public:
	static const TArray<FFOLevelDef>& All();
	static const FFOLevelDef* Get(int32 Index);
	static const FFOLevelDef* Find(FName Id);
	static int32 Num() { return All().Num(); }
};
