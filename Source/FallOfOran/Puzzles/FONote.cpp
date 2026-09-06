#include "Puzzles/FONote.h"
#include "FOGameMode.h"
#include "FOCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"

AFONote::AFONote()
{
	PrimaryActorTick.bCanEverTick = true;
	Paper = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Paper"));
	RootComponent = Paper;
	Paper->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	Paper->SetRelativeScale3D(FVector(0.21f, 0.30f, 1.f));
	Paper->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Paper);
	Glow->SetRelativeLocation(FVector(0, 0, 30.f));
	Glow->SetLightColor(FLinearColor(1.f, 0.9f, 0.6f));
	Glow->SetIntensity(6.f);
	Glow->SetAttenuationRadius(200.f);
	Glow->SetCastShadows(false);
	Glow->bUseInverseSquaredFalloff = false;
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Flat.M_Flat")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, this);
		MID->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.9f, 0.85f, 0.7f));
		MID->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.6f, 0.55f, 0.4f));
		MID->SetScalarParameterValue(TEXT("Roughness"), 0.9f);
		Paper->SetMaterial(0, MID);
	}
}

void AFONote::Tick(float Dt)
{
	Super::Tick(Dt);
	T += Dt;
	Glow->SetIntensity(bRead ? 2.f : 5.f + 3.f * FMath::Sin(T * 3.f));
}

bool AFONote::CanInteract(AFOCharacter*) const
{
	const AFOGameMode* G = GetWorld()->GetAuthGameMode<AFOGameMode>();
	return G && G->State == EFOState::Playing;
}

void AFONote::Interact(AFOCharacter*)
{
	AFOGameMode* G = GetWorld()->GetAuthGameMode<AFOGameMode>();
	if (!G) return;
	if (USoundBase* S = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/pickup.pickup"))) UGameplayStatics::PlaySound2D(this, S, 0.6f);
	G->ShowNote(Text, 7.f);
	if (!bRead) { bRead = true; G->ReportEvent(FFOGameEvent(EFOGameEvent::NoteRead, Tag)); G->ReportEvent(FFOGameEvent(EFOGameEvent::ItemCollected, Tag)); }
}
