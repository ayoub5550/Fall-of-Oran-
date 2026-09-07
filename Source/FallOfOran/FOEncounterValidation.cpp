// Synthetic actor integration test, NOT an ordinary-input playthrough.
#include "FOGameMode.h"
#include "FOCharacter.h"
#include "FOZombie.h"
#include "FallOfOran.h"
#include "Puzzles/FOGeneratorPuzzle.h"
#include "Components/FOHealthComponent.h"
#include "Components/FOInteractionComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"

void AFOGameMode::RunEncounterValidation()
{
	bool bPass = true;
	auto Check = [&](bool bOK, const TCHAR* Name)
	{
		if (!bOK) bPass = false;
		UE_LOG(LogFO, Display, TEXT("ENCOUNTER CHECK %s: %s"), Name, bOK ? TEXT("PASS") : TEXT("FAIL"));
	};
	AFOCharacter* P = Player();
	AFOGeneratorPuzzle* G = nullptr;
	for (TActorIterator<AFOGeneratorPuzzle> It(GetWorld()); It; ++It) G = *It;
	if (!P || !G || LevelIndex != 0)
	{
		UE_LOG(LogFO, Error, TEXT("ENCOUNTER FAIL: requires level 0 player and generator"));
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
	StartGame();
	P->HealthComp->bInvulnerable = true;
	G->SetActorTickEnabled(false); // advance exact simulated deltas below
	auto ClearEnemies = [&]()
	{
		for (TActorIterator<AFOZombie> It(GetWorld()); It; ++It) It->Destroy();
	};
	ClearEnemies();
	const FVector Front = G->GetActorForwardVector() * -1.f;
	const FVector Near = G->GetActorLocation() + Front * 190.f + FVector(0, 0, 100);
	P->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
	P->SetActorRotation((-Front).Rotation());
	Check(!G->CanInteract(P), TEXT("fuel prerequisite locks generator"));
	for (int i = 0; i < 3; ++i) ReportEvent(FFOGameEvent(EFOGameEvent::ItemCollected, TEXT("fuel")));
	ClearEnemies();
	// Sweep the actual player capsule down the central approach, not a visual inference.
	P->SetActorLocation(Near + Front * 160.f, false, nullptr, ETeleportType::TeleportPhysics);
	FHitResult Approach;
	P->SetActorLocation(Near, true, &Approach);
	Check(!Approach.bBlockingHit, TEXT("front capsule approach clear"));
	P->Interaction->TickComponent(1.f, LEVELTICK_All, nullptr);
	Check(P->HasInteractTarget(), TEXT("interaction component finds generator"));
	P->TryInteract();
	Check(G->IsRunning(), TEXT("normal interaction API starts generator"));
	int32 Count = 0;
	for (TActorIterator<AFOZombie> It(GetWorld()); It; ++It) ++Count;
	Check(Count == 3, TEXT("startup summons exactly three"));
	G->Interact(P);
	Count = 0;
	for (TActorIterator<AFOZombie> It(GetWorld()); It; ++It) ++Count;
	Check(Count == 3, TEXT("repeat interaction cannot duplicate wave"));
	ClearEnemies();
	G->Tick(5.f);
	Check(FMath::IsNearlyEqual(G->StartupProgress(), 5.f), TEXT("nearby time advances"));
	P->SetActorLocation(Near + Front * 1200.f, false, nullptr, ETeleportType::TeleportPhysics);
	G->Tick(10.f);
	Check(FMath::IsNearlyEqual(G->StartupProgress(), 5.f), TEXT("outside radius pauses without reset"));
	P->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
	State = EFOState::Menu;
	G->Tick(10.f);
	Check(FMath::IsNearlyEqual(G->StartupProgress(), 5.f), TEXT("non-playing state pauses"));
	State = EFOState::Playing;
	G->Tick(12.9f);
	Check(!G->IsSolved(), TEXT("not solved before eighteen seconds"));
	G->Tick(0.2f);
	Check(G->IsSolved() && !G->IsRunning(), TEXT("solves after eighteen nearby seconds"));

	// Same existing rig, distinct runtime stats; manually tick to isolate attack rules.
	P->SetActorLocation(FVector(600, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
	P->HealthComp->bInvulnerable = false;
	const FTransform T(FRotator::ZeroRotator, FVector(750, 0, 100));
	AFOZombie* Z = GetWorld()->SpawnActorDeferred<AFOZombie>(AFOZombie::StaticClass(), T);
	if (!Z)
	{
		Check(false, TEXT("heavy spawn"));
	}
	else
	{
		Z->Variant = 2; Z->bRunner = false;
		UGameplayStatics::FinishSpawningActor(Z, T);
		Z->SetActorTickEnabled(false);
		Z->Target = P; Z->bScreamed = true; Z->bChasing = true;
		Check(Z->bHeavy && Z->Hp == 160.f && Z->ChaseSpeed == 140.f && Z->MeleeDamage == 30.f,
			TEXT("heavy role stats"));
		const float Health = P->GetHealth();
		Z->Tick(0.01f);
		Check(Z->bAttackPending && P->GetHealth() == Health, TEXT("no instant melee damage"));
		P->SetActorLocation(FVector(400, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
		Z->Tick(0.7f);
		Check(P->GetHealth() == Health, TEXT("retreat avoids committed swing"));
		P->SetActorLocation(FVector(600, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
		Z->AttackTimer = 0; Z->OneShotTimer = 0;
		Z->Tick(0.01f); Z->Tick(0.7f);
		Check(FMath::IsNearlyEqual(P->GetHealth(), Health - 30.f), TEXT("in-range heavy swing deals thirty"));
		AActor* Wall = GetWorld()->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
		Wall->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(10, 100, 100));
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->SetWorldLocation(FVector(675, 0, 100));
		Box->RegisterComponent();
		Check(!Z->HasAttackLine(), TEXT("cover blocks melee sight"));
		Z->AttackTimer = 0; Z->OneShotTimer = 0; Z->bAttackPending = false;
		Z->Tick(0.01f);
		Check(!Z->bAttackPending, TEXT("cannot start swing through cover"));
		Wall->Destroy();
		Z->Destroy();
	}
	UE_LOG(LogFO, Display, TEXT("ENCOUNTER %s: synthetic actor integration, not device/input validation"),
		bPass ? TEXT("PASS") : TEXT("FAIL"));
	FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
}
