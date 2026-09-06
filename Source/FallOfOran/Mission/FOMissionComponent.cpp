#include "Mission/FOMissionComponent.h"
#include "Mission/FOObjective.h"
#include "FallOfOran.h"

void UFOMissionComponent::Start(const FFOLevelDef& Level)
{
	Stages = Level.Stages;
	bComplete = false;
	Active.Empty();
	Stage = -1;
	if (Stages.Num() == 0) { bComplete = true; OnMissionComplete.Broadcast(); return; }
	EnterStage(0);
}

void UFOMissionComponent::EnterStage(int32 Index)
{
	Stage = Index;
	Active.Empty();
	for (const FFOObjectiveDef& D : Stages[Index].Objectives)
	{
		Active.Add(UFOObjective::Create(this, D));
		if (!D.StartHint.IsEmpty()) OnHint.Broadcast(D.StartHint);
	}
	UE_LOG(LogFO, Display, TEXT("Mission: stage %d/%d (%d objectives)"), Index + 1, Stages.Num(), Active.Num());
	OnStageChanged.Broadcast();
}

void UFOMissionComponent::HandleEvent(const FFOGameEvent& E)
{
	if (bComplete) return;
	bool bAllDone = true;
	for (UFOObjective* O : Active)
	{
		const bool bWasDone = O->IsComplete();
		if (O->HandleEvent(E) && O->IsComplete() && !bWasDone && !O->GetDef().DoneHint.IsEmpty()) OnHint.Broadcast(O->GetDef().DoneHint);
		bAllDone &= O->IsComplete();
	}
	if (!bAllDone) return;
	if (Stage + 1 < Stages.Num()) EnterStage(Stage + 1);
	else { bComplete = true; UE_LOG(LogFO, Display, TEXT("Mission complete")); OnMissionComplete.Broadcast(); }
}

FString UFOMissionComponent::GetObjectiveText() const
{
	FString Out;
	for (UFOObjective* O : Active)
	{
		if (!Out.IsEmpty()) Out += TEXT("\n");
		Out += (O->IsComplete() ? TEXT("✓ ") : TEXT("• ")) + O->GetText();
	}
	return Out;
}

bool UFOMissionComponent::IsExitOpen() const
{
	for (UFOObjective* O : Active) if (O->GetDef().Type == EFOObjectiveType::Reach && !O->IsComplete()) return true;
	return false;
}

bool UFOMissionComponent::IsPuzzleActive(FName PuzzleId) const
{
	for (UFOObjective* O : Active) if (O->GetDef().Type == EFOObjectiveType::SolvePuzzle && O->GetDef().Tag == PuzzleId && !O->IsComplete()) return true;
	return false;
}
