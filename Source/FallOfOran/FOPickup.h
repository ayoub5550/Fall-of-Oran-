#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/FOTypes.h"
#include "FOPickup.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class USphereComponent;
class AFOCharacter;

/** Glowing pickup: fuel can (objective item, reports ItemCollected(Tag)), medkit (+HealAmount), ammo box (+AmmoAmount). */
UCLASS()
class AFOPickup : public AActor
{
	GENERATED_BODY()
public:
	AFOPickup();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	EFOItem Item = EFOItem::Ammo;
	FName Tag = TEXT("ammo");
	float HealAmount = 40.f;
	int32 AmmoAmount = 12;
	UPROPERTY() UStaticMeshComponent* Mesh = nullptr;
	UPROPERTY() UPointLightComponent* Glow = nullptr;
	UPROPERTY() USphereComponent* Trigger = nullptr;
	float T = 0.f;
	UFUNCTION() void OnOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);

private:
	void TryCollect(AFOCharacter* Player);
	bool bCollected = false;
	float CollectCheckTimer = 0.f;
};
