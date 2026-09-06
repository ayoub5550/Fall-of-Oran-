#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/FOInteractable.h"
#include "FONote.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/** A readable note (glowing paper). First read reports ItemCollected("note"); text stays re-readable. */
UCLASS()
class AFONote : public AActor, public IFOInteractable
{
	GENERATED_BODY()
public:
	AFONote();
	virtual void Tick(float Dt) override;
	virtual FString GetPrompt() const override { return TEXT("اقرأ الملاحظة"); }
	virtual void Interact(AFOCharacter* Who) override;
	virtual bool CanInteract(AFOCharacter* Who) const override;

	FString Text;
	FName Tag = TEXT("note");
	bool bRead = false;
	UPROPERTY() UStaticMeshComponent* Paper = nullptr;
	UPROPERTY() UPointLightComponent* Glow = nullptr;
	float T = 0.f;
};
