#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/FOTypes.h"
#include "Components/FOInteractable.h"
#include "FOPuzzleBase.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class AFOGameMode;

UENUM()
enum class EFOPuzzleState : uint8 { Idle, Solved };

/**
 * Base for every puzzle. A puzzle is spawned by AFOWorldBuilder from an FFOPuzzleDef, builds its own
 * geometry in Setup(), and calls Solve() / Fail() from its own logic.
 *  - Solve()  -> GameMode->ReportEvent(PuzzleSolved, Id)  (mission advances)
 *  - Fail()   -> penalty zombies + hint
 *  - GetHintText(i) -> text written on the i-th note spawned for this puzzle
 * Interaction is only allowed while the mission says this puzzle is in the active stage.
 */
UCLASS(Abstract)
class AFOPuzzleBase : public AActor, public IFOInteractable
{
	GENERATED_BODY()
public:
	AFOPuzzleBase();
	virtual void Setup(const FFOPuzzleDef& InDef, const FRandomStream& Rng);
	virtual FString GetHintText(int32 NoteIndex) const { return FString(); }

	// IFOInteractable
	virtual bool CanInteract(AFOCharacter* Who) const override;
	virtual void Interact(AFOCharacter* Who) override {}

	/** Self-test hook (-FOSelfTest): perform the correct solution through the normal code path. */
	virtual void DebugSolve() {}
	FName GetId() const { return Def.Id; }
	bool IsSolved() const { return State == EFOPuzzleState::Solved; }

protected:
	void Solve();
	void Fail(const FString& Message);
	AFOGameMode* GM() const;
	/** Helpers for quick geometry: unlit-ish coloured boxes with optional emissive. */
	UStaticMeshComponent* MakeBox(const FVector& RelLoc, const FVector& Size, const FLinearColor& Tint, const FLinearColor& Emissive = FLinearColor::Black, bool bCollide = true);
	UMaterialInstanceDynamic* MakeFlat(const FLinearColor& Tint, const FLinearColor& Emissive = FLinearColor::Black, float Rough = 0.7f);
	UPointLightComponent* MakeLight(const FVector& RelLoc, const FLinearColor& Color, float Intensity, float Radius);

	UPROPERTY() FFOPuzzleDef Def;
	EFOPuzzleState State = EFOPuzzleState::Idle;
	UPROPERTY() USceneComponent* Root = nullptr;
	UPROPERTY() UMaterialInterface* FlatMaster = nullptr;
	UPROPERTY() UStaticMesh* CubeMesh = nullptr;
	UPROPERTY() USoundBase* SolvedSound = nullptr;
	UPROPERTY() USoundBase* FailSound = nullptr;
	UPROPERTY() USoundBase* ClickSound = nullptr;
};

/** Small interactable pinned to a parent puzzle (a single breaker switch, a keypad face...). */
UCLASS()
class AFOPuzzlePart : public AActor, public IFOInteractable
{
	GENERATED_BODY()
public:
	AFOPuzzlePart();
	TWeakObjectPtr<AFOPuzzleBase> Parent;
	int32 Index = 0;
	FString Prompt;
	TFunction<void(AFOCharacter*)> OnUse;
	virtual bool CanInteract(AFOCharacter* Who) const override;
	virtual FString GetPrompt() const override { return Prompt; }
	virtual void Interact(AFOCharacter* Who) override { if (OnUse) OnUse(Who); }
	UPROPERTY() UStaticMeshComponent* Mesh = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* Mat = nullptr;
	UPROPERTY() UPointLightComponent* Lamp = nullptr;
};
