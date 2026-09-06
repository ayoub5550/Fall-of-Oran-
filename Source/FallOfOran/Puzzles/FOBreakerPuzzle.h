#pragma once
#include "CoreMinimal.h"
#include "Puzzles/FOPuzzleBase.h"
#include "FOBreakerPuzzle.generated.h"

/**
 * Electrical panel with N coloured breakers. Press them in the order written on the level's note.
 * Wrong press: everything resets, alarm, penalty zombies. All correct: panel lights green, Solve().
 */
UCLASS()
class AFOBreakerPuzzle : public AFOPuzzleBase
{
	GENERATED_BODY()
public:
	virtual void Setup(const FFOPuzzleDef& InDef, const FRandomStream& Rng) override;
	virtual FString GetHintText(int32 NoteIndex) const override;
	virtual FString GetPrompt() const override { return TEXT("لوحة القواطع"); }
	virtual void DebugSolve() override { Press(Order.Num() > 1 ? Order[1] : 0); /* one wrong press first */ for (int32 k : Order) Press(k); }

private:
	void Press(int32 Index);
	void ResetSwitches();
	TArray<int32> Order;        // Order[k] = index of the switch to press k-th
	int32 Progress = 0;
	UPROPERTY() TArray<AFOPuzzlePart*> Switches;
	UPROPERTY() UStaticMeshComponent* StatusPanel = nullptr;
	UPROPERTY() UPointLightComponent* StatusLight = nullptr;
	static const TCHAR* ColorName(int32 i);
	static FLinearColor Color(int32 i);
};
