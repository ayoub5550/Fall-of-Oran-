#pragma once
#include "CoreMinimal.h"
#include "Puzzles/FOPuzzleBase.h"
#include "FOKeypadPuzzle.generated.h"

/**
 * Numeric keypad on a gate. Interact -> HUD opens the keypad panel (AFOGameMode::OpenKeypad).
 * Digits of the code are on notes: note i says "الرقم i+1 من الشيفرة: d". Wrong code -> Fail().
 */
UCLASS()
class AFOKeypadPuzzle : public AFOPuzzleBase
{
	GENERATED_BODY()
public:
	virtual void Setup(const FFOPuzzleDef& InDef, const FRandomStream& Rng) override;
	virtual FString GetHintText(int32 NoteIndex) const override;
	virtual FString GetPrompt() const override { return TEXT("لوحة الشيفرة"); }
	virtual void Interact(AFOCharacter* Who) override;

	/** Called by the HUD keypad. Returns true when the code was right. */
	bool Submit(const FString& Entered);
	int32 CodeLength() const { return Def.Code.Len(); }
	int32 Attempts = 0;
	virtual void DebugSolve() override { Submit(TEXT("0000")); Submit(Def.Code); }
private:
	UPROPERTY() UStaticMeshComponent* Screen = nullptr;
	UPROPERTY() UPointLightComponent* ScreenLight = nullptr;
};
