#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/FOTypes.h"
#include "FOObjective.generated.h"

/**
 * One runtime objective, created from an FFOObjectiveDef by UFOMissionComponent.
 * Subclass to add a new objective type: override Matches() (and optionally Progress text).
 */
UCLASS()
class UFOObjective : public UObject
{
	GENERATED_BODY()
public:
	static UFOObjective* Create(UObject* Outer, const FFOObjectiveDef& Def);

	virtual void Init(const FFOObjectiveDef& InDef) { Def = InDef; Current = 0; }
	/** Feed an event; returns true if it advanced this objective. */
	bool HandleEvent(const FFOGameEvent& E);
	bool IsComplete() const { return Current >= Def.Count; }
	/** HUD line with {n}/{max} substituted. */
	FString GetText() const;
	const FFOObjectiveDef& GetDef() const { return Def; }
	int32 GetCurrent() const { return Current; }

protected:
	/** Does this event count towards the objective? Return how much to add (0 = ignore). */
	virtual int32 Matches(const FFOGameEvent& E) const;
	UPROPERTY() FFOObjectiveDef Def;
	int32 Current = 0;
};
