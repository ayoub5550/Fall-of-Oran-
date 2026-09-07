#include "Components/FOWeaponComponent.h"
#include "FOCharacter.h"
#include "FOZombie.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"

UFOWeaponComponent::UFOWeaponComponent() { PrimaryComponentTick.bCanEverTick = true; }

void UFOWeaponComponent::TickComponent(float Dt, ELevelTick, FActorComponentTickFunction*)
{
	Cooldown -= Dt;
	if (bReloading)
	{
		ReloadTimer -= Dt;
		if (ReloadTimer <= 0.f)
		{
			bReloading = false;
			const int32 Take = FMath::Min(Stats.MagSize - Ammo, Reserve);
			Ammo += Take; Reserve -= Take;
			OnAmmoChanged.Broadcast();
			OnReloadEnd.Broadcast();
		}
	}
}

void UFOWeaponComponent::StartReload()
{
	if (bReloading || Reserve <= 0 || Ammo >= Stats.MagSize) return;
	bReloading = true;
	ReloadTimer = Stats.ReloadTime;
	OnReloadStart.Broadcast();
}

AFOZombie* UFOWeaponComponent::FindAimAssistTarget(const FVector& Start, const FVector& Dir) const
{
	AFOZombie* Best = nullptr; float BestD = 1e9f;
	for (TActorIterator<AFOZombie> It(GetWorld()); It; ++It)
	{
		AFOZombie* Z = *It;
		if (Z->bDying) continue;
		const FVector To = Z->GetActorLocation() + FVector(0, 0, 60.f) - Start;
		const float D = To.Size();
		if (D > Stats.Range) continue;
		// Cone shrinks with distance: generous up close, precise far away (feels fair on touch screens).
		const float Cone = FMath::Lerp(Stats.AimAssistMaxCone, Stats.AimAssistMinCone, FMath::Clamp(D / FMath::Max(Stats.Range, 1.f), 0.f, 1.f));
		const float Ang = FMath::Acos(FMath::Clamp(FVector::DotProduct(To.GetSafeNormal(), Dir.GetSafeNormal()), -1.f, 1.f));
		if (Ang >= Cone || D >= BestD) continue;
		FHitResult Sight;
		FCollisionQueryParams QP(SCENE_QUERY_STAT(FOAimAssist), true, GetOwner());
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Sight, Start, Start + To, ECC_Visibility, QP);
		if (bBlocked && Sight.GetActor() != Z) continue;
		BestD = D; Best = Z;
	}
	return Best;
}

bool UFOWeaponComponent::Fire(const FVector& Start, const FVector& Dir)
{
	if (!CanFire()) return false;
	if (Ammo <= 0) { OnDryFire.Broadcast(); StartReload(); return false; }
	Ammo--;
	Cooldown = Stats.FireInterval;
	OnAmmoChanged.Broadcast();
	OnFired.Broadcast();

	FHitResult Hit;
	FCollisionQueryParams QP(SCENE_QUERY_STAT(FOWeapon), true, GetOwner());
	AActor* Target = nullptr;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * Stats.Range, ECC_Visibility, QP);
	if (bHit) Target = Cast<AFOZombie>(Hit.GetActor());
	if (!Target)
		if (AFOZombie* Z = FindAimAssistTarget(Start, Dir))
		{
			Target = Z;
			Hit.ImpactPoint = Z->GetActorLocation() + FVector(0, 0, 80.f);
			Hit.ImpactNormal = -Dir;
		}
	if (AFOZombie* Z = Cast<AFOZombie>(Target)) Z->TakeHit(Stats.Damage, Cast<AFOCharacter>(GetOwner()));
	if (Target || bHit) OnHit.Broadcast(Target, Hit);
	if (Ammo == 0) StartReload();
	return true;
}
