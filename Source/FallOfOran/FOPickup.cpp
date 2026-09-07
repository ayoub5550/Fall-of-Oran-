#include "FOPickup.h"
#include "FOCharacter.h"
#include "FOGameMode.h"
#include "Components/FOHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AFOPickup::AFOPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(100.f);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = Trigger;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetAttenuationRadius(220.f);
	Glow->SetIntensity(8.f);
	Glow->SetCastShadows(false);
	Glow->bUseInverseSquaredFalloff = false;
}

void AFOPickup::BeginPlay()
{
	Super::BeginPlay();
	FLinearColor C; FVector Size;
	switch (Item)
	{
	case EFOItem::Fuel: C = FLinearColor(0.9f, 0.35f, 0.05f); Size = FVector(35.f, 25.f, 45.f); break;
	case EFOItem::Health: C = FLinearColor(0.85f, 0.1f, 0.1f); Size = FVector(40.f, 30.f, 25.f); break;
	// Challenge supply crate: deliberately distinct from the campaign fuel can (different tag + colour).
	case EFOItem::Supply: C = FLinearColor(0.15f, 0.75f, 0.85f); Size = FVector(55.f, 45.f, 40.f); break;
	default: C = FLinearColor(0.25f, 0.45f, 0.15f); Size = FVector(35.f, 25.f, 22.f); break;
	}
	Mesh->SetRelativeScale3D(Size / 100.f);
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Flat.M_Flat")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, this);
		MID->SetVectorParameterValue(TEXT("Tint"), C);
		MID->SetVectorParameterValue(TEXT("Emissive"), C * 1.5f);
		MID->SetScalarParameterValue(TEXT("Roughness"), 0.5f);
		Mesh->SetMaterial(0, MID);
	}
	Glow->SetLightColor(C);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AFOPickup::OnOverlap);
}

void AFOPickup::Tick(float Dt)
{
	Super::Tick(Dt);
	T += Dt;
	Mesh->SetRelativeLocation(FVector(0, 0, 6.f * FMath::Sin(T * 2.f)));
	Mesh->AddRelativeRotation(FRotator(0, 40.f * Dt, 0));
	Glow->SetIntensity(7.f + 3.f * FMath::Sin(T * 3.f));
	// BeginOverlap can happen while in the menu or at full health. Re-check only
	// nearby overlaps so the pickup remains usable without leaving/re-entering.
	CollectCheckTimer -= Dt;
	if (!bCollected && CollectCheckTimer <= 0.f)
	{
		CollectCheckTimer = 0.15f;
		if (AFOGameMode* GM = GetWorld()->GetAuthGameMode<AFOGameMode>())
			if (GM->State == EFOState::Playing)
				if (AFOCharacter* P = GM->Player())
					if (Trigger->IsOverlappingActor(P)) TryCollect(P);
	}
}

void AFOPickup::OnOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	TryCollect(Cast<AFOCharacter>(Other));
}

void AFOPickup::TryCollect(AFOCharacter* P)
{
	AFOGameMode* GM = GetWorld()->GetAuthGameMode<AFOGameMode>();
	if (bCollected || !P || P->IsDead() || !GM || GM->State != EFOState::Playing) return;
	if (Item == EFOItem::Health && (!P->HealthComp || P->HealthComp->GetHealth() >= P->HealthComp->GetMax() || HealAmount <= 0.f)) return;
	bCollected = true; // one reward even if multiple overlap notifications arrive
	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	switch (Item)
	{
	case EFOItem::Health: P->AddHealth(HealAmount); GM->HealFlash = 0.3f; break;
	case EFOItem::Ammo: P->AddAmmo(AmmoAmount); break;
	default: break;
	}
	if (USoundBase* S = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/pickup.pickup"))) UGameplayStatics::PlaySound2D(this, S);
	GM->ReportEvent(FFOGameEvent(EFOGameEvent::ItemCollected, Tag));
	Destroy();
}
