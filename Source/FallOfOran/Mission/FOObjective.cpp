#include "Mission/FOObjective.h"

UFOObjective* UFOObjective::Create(UObject* Outer, const FFOObjectiveDef& Def)
{
	// All four built-in types share one matcher; a new type with special rules gets its own subclass here.
	UFOObjective* O = NewObject<UFOObjective>(Outer);
	O->Init(Def);
	return O;
}

int32 UFOObjective::Matches(const FFOGameEvent& E) const
{
	switch (Def.Type)
	{
	case EFOObjectiveType::Collect:     return (E.Type == EFOGameEvent::ItemCollected && E.Tag == Def.Tag) ? E.Count : 0;
	case EFOObjectiveType::Kill:        return E.Type == EFOGameEvent::ZombieKilled ? E.Count : 0;
	case EFOObjectiveType::SolvePuzzle: return (E.Type == EFOGameEvent::PuzzleSolved && E.Tag == Def.Tag) ? 1 : 0;
	case EFOObjectiveType::Reach:       return E.Type == EFOGameEvent::ExitReached ? 1 : 0;
	}
	return 0;
}

bool UFOObjective::HandleEvent(const FFOGameEvent& E)
{
	if (IsComplete()) return false;
	const int32 Add = Matches(E);
	if (Add <= 0) return false;
	Current = FMath::Min(Def.Count, Current + Add);
	return true;
}

FString UFOObjective::GetText() const
{
	FString T = Def.Text;
	T.ReplaceInline(TEXT("{n}"), *FString::FromInt(Current));
	T.ReplaceInline(TEXT("{max}"), *FString::FromInt(Def.Count));
	return T;
}
