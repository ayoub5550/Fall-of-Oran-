#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FOWeaponComponent.generated.h"

class AFOZombie;
class UCameraComponent;
class USoundBase;

/** Tunable numbers for one firearm. Add a second weapon by making another FFOWeaponStats. */
USTRUCT()
struct FFOWeaponStats
{
	GENERATED_BODY()
	UPROPERTY() FString Name = TEXT("مسدس");
	UPROPERTY() int32 MagSize = 12;
	UPROPERTY() float Damage = 40.f;
	UPROPERTY() float Range = 4500.f;         // cm
	UPROPERTY() float FireInterval = 0.32f;   // s
	UPROPERTY() float ReloadTime = 1.6f;      // s
	UPROPERTY() float AimAssistMaxCone = 0.35f; // rad, at point blank
	UPROPERTY() float AimAssistMinCone = 0.12f; // rad, at max range
};

DECLARE_MULTICAST_DELEGATE(FFOWeaponEvent);
DECLARE_MULTICAST_DELEGATE_TwoParams(FFOWeaponHit, AActor* /*Hit*/, const FHitResult& /*Result*/);

/**
 * Hitscan weapon with magazine/reserve, cooldown, reload and distance-scaled aim assist.
 * Owner supplies the aim ray (camera). Fires delegates so the owner can play animations/VFX.
 */
UCLASS()
class UFOWeaponComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UFOWeaponComponent();
	virtual void TickComponent(float Dt, ELevelTick, FActorComponentTickFunction*) override;

	/** Attempts a shot along Start/Dir. Returns true if a round was fired. */
	bool Fire(const FVector& Start, const FVector& Dir);
	void StartReload();
	void AddReserve(int32 N) { Reserve += N; OnAmmoChanged.Broadcast(); }
	bool CanFire() const { return !bReloading && Cooldown <= 0.f; }

	UPROPERTY() FFOWeaponStats Stats;
	int32 Ammo = 12;
	int32 Reserve = 36;
	bool bReloading = false;

	FFOWeaponEvent OnFired;        // a round left the barrel
	FFOWeaponEvent OnDryFire;      // trigger pulled on empty mag
	FFOWeaponEvent OnReloadStart;
	FFOWeaponEvent OnReloadEnd;
	FFOWeaponEvent OnAmmoChanged;
	FFOWeaponHit   OnHit;          // Hit actor may be null (wall hit still reported with HitResult)

private:
	float Cooldown = 0.f;
	float ReloadTimer = 0.f;
	AFOZombie* FindAimAssistTarget(const FVector& Start, const FVector& Dir) const;
};
