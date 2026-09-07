#include "Puzzles/FOGeneratorPuzzle.h"
#include "FOCharacter.h"
#include "FOGameMode.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "FallOfOran.h"

AFOGeneratorPuzzle::AFOGeneratorPuzzle() { PrimaryActorTick.bCanEverTick = true; }

void AFOGeneratorPuzzle::Setup(const FFOPuzzleDef& InDef, const FRandomStream& Rng)
{
	Super::Setup(InDef, Rng);
	if (UStaticMesh* Asset = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Props/Checkpoint/SM_CheckpointGenerator/SM_CheckpointGenerator.SM_CheckpointGenerator")))
	{
		UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(this);
		Body->SetupAttachment(RootComponent);
		Body->SetStaticMesh(Asset);
		Body->SetCollisionProfileName(TEXT("BlockAll"));
		Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Body->RegisterComponent();
		UBoxComponent* Bounds = NewObject<UBoxComponent>(this);
		Bounds->SetupAttachment(RootComponent);
		Bounds->SetRelativeLocation(Asset->GetBoundingBox().GetCenter());
		Bounds->SetBoxExtent(Asset->GetBoundingBox().GetExtent());
		Bounds->SetCollisionProfileName(TEXT("BlockAll"));
		Bounds->RegisterComponent();
	}
	else
	{
		UE_LOG(LogFO, Warning, TEXT("Checkpoint generator mesh missing; using blockout"));
		MakeBox(FVector(0, 0, 55), FVector(120, 180, 110), FLinearColor(0.3f, 0.28f, 0.08f));
	}
	// Readable front control face: low emissive only on local machine details,
	// not a global exposure change (physical phones are brighter than Lavapipe).
	MakeBox(FVector(-68, 35, 82), FVector(5, 48, 38),
		FLinearColor(0.086f, 0.086f, 0.086f));
	MakeBox(FVector(-72, 35, 89), FVector(2, 32, 8),
		FLinearColor(0.9f, 0.75f, 0.1f), FLinearColor(0.9f, 0.75f, 0.1f) * 0.8f, false);
	StatusLight = MakeLight(FVector(-80, 35, 100), FLinearColor(1, 0.2f, 0.05f), 12.f, 350.f);
}

bool AFOGeneratorPuzzle::CanInteract(AFOCharacter* Who) const
{
	return Who && FVector::DistSquared2D(Who->GetActorLocation(), GetActorLocation())
		<= FMath::Square(GetInteractRange()) && !bRunning && Super::CanInteract(Who);
}

void AFOGeneratorPuzzle::Interact(AFOCharacter* Who)
{
	if (!CanInteract(Who)) return;
	bRunning = true;
	if (AFOGameMode* G = GM())
	{
		G->SetHint(TEXT("المولّد يجذبهم! ابق قربه ودافع حتى تستقر الطاقة"), 5.f);
		G->SpawnZombiesBehindPlayer(3);
	}
	UE_LOG(LogFO, Display, TEXT("Generator started: %s"), *Def.Id.ToString());
}

void AFOGeneratorPuzzle::Tick(float Dt)
{
	Super::Tick(Dt);
	AFOGameMode* G = GM();
	if (!bRunning || IsSolved() || !G || G->State != EFOState::Playing || !G->Player()) return;
	const bool bNearby = FVector::DistSquared2D(G->Player()->GetActorLocation(), GetActorLocation()) <= FMath::Square(DefendRadius);
	if (bNearby) Elapsed += Dt;
	if (StatusLight) StatusLight->SetIntensity(18.f + 8.f * FMath::Sin(Elapsed * 5.f));
	HintClock -= Dt;
	if (HintClock <= 0.f)
	{
		HintClock = 1.f;
		G->SetHint(bNearby
			? FString::Printf(TEXT("دافع قرب المولّد — تشغيل الطاقة %d%%"), FMath::Clamp(FMath::FloorToInt(Elapsed / StartupSeconds * 100.f), 0, 100))
			: TEXT("ارجع قرب المولّد لاستكمال التشغيل — التقدّم محفوظ"), 1.5f);
	}
	if (Elapsed >= StartupSeconds) FinishStartup();
}

void AFOGeneratorPuzzle::FinishStartup()
{
	bRunning = false;
	if (StatusLight) { StatusLight->SetLightColor(FLinearColor::Green); StatusLight->SetIntensity(30.f); }
	Solve();
}

void AFOGeneratorPuzzle::DebugSolve()
{
	// Synthetic logic tests do not wait through or claim to play the defence encounter.
	if (AFOGameMode* G = GM())
		if (G->bSelfTest && Super::CanInteract(G->Player())) FinishStartup();
}
