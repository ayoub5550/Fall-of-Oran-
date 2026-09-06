#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/FOTypes.h"
#include "FOMissionComponent.generated.h"

class UFOObjective;

DECLARE_MULTICAST_DELEGATE_OneParam(FFOMissionHint, const FString& /*Text*/);
DECLARE_MULTICAST_DELEGATE(FFOMissionSimple);

/**
 * Runs a level's stages: stage N's objectives are active in parallel; when all are complete the next
 * stage starts; when the last stage completes OnMissionComplete fires. Lives on the GameMode.
 *
 * Flow:  AFOGameMode::ReportEvent(E)  ->  Mission->HandleEvent(E)  ->  objectives advance
 *        HUD reads GetObjectiveText() every frame; hints arrive through OnHint (timed banner).
 */
UCLASS()
class UFOMissionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	void Start(const FFOLevelDef& Level);
	void HandleEvent(const FFOGameEvent& E);

	bool IsComplete() const { return bComplete; }
	int32 StageIndex() const { return Stage; }
	/** Multi-line HUD text: one line per active objective. */
	FString GetObjectiveText() const;
	/** True if the exit may be used now (a Reach objective is active). */
	bool IsExitOpen() const;
	/** True if the puzzle with this id is part of the *current* stage. */
	bool IsPuzzleActive(FName PuzzleId) const;

	FFOMissionHint OnHint;             // "البوابة فُتحت!" etc.
	FFOMissionSimple OnStageChanged;
	FFOMissionSimple OnMissionComplete;

private:
	void EnterStage(int32 Index);
	UPROPERTY() TArray<UFOObjective*> Active;
	TArray<FFOMissionStage> Stages;
	int32 Stage = -1;
	bool bComplete = false;
};
