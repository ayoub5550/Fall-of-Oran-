#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FOPickup.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class USphereComponent;

UENUM()
enum class EFOPickup : uint8 { Fuel, Health, Ammo };

/** Glowing pickup: fuel can (objective), medkit (+40 hp), ammo box (+12). */
UCLASS()
class AFOPickup : public AActor
{
	GENERATED_BODY()
public:
	AFOPickup();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	EFOPickup Kind = EFOPickup::Ammo;
	UPROPERTY() UStaticMeshComponent* Mesh = nullptr;
	UPROPERTY() UPointLightComponent* Glow = nullptr;
	UPROPERTY() USphereComponent* Trigger = nullptr;
	float T = 0.f;
	UFUNCTION() void OnOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
};
