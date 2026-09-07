#include "Puzzles/FOGeneratorPuzzle.h"
#include "FOCharacter.h"
#include "FOGameMode.h"
#include "Components/PointLightComponent.h"
#include "FallOfOran.h"

AFOGeneratorPuzzle::AFOGeneratorPuzzle() { PrimaryActorTick.bCanEverTick = true; }

void AFOGeneratorPuzzle::Setup(const FFOPuzzleDef& InDef, const FRandomStream& Rng)
{
	Super::Setup(InDef, Rng);
	MakeBox(FVector(0, 0, 55), FVector(120, 180, 110), FLinearColor(0.3f, 0.28f, 0.08f));
	MakeBox(FVector(0, 0, 120), FVector(100, 160, 20), FLinearColor(0.08f, 0.08f, 0.08f));
	MakeBox(FVector(40, 50, 165), FVector(18, 18, 90), FLinearColor(0.1f, 0.1f, 0.1f));
	StatusLight = MakeLight(FVector(-70, 0, 110), FLinearColor(1, 0.2f, 0.05f), 12.f, 350.f);
}

bool AFOGeneratorPuzzle::CanInteract(AFOCharacter* Who) const
{
	return !bRunning && Super::CanInteract(Who);
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
