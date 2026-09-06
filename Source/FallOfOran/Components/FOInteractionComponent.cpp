#include "Components/FOInteractionComponent.h"
#include "Components/FOInteractable.h"
#include "FOCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UFOInteractionComponent::UFOInteractionComponent() { PrimaryComponentTick.bCanEverTick = true; }

void UFOInteractionComponent::TickComponent(float Dt, ELevelTick, FActorComponentTickFunction*)
{
	Refresh -= Dt;
	if (Refresh > 0.f) return;
	Refresh = 0.15f; // 6-7 Hz is plenty and cheap on phones
	AFOCharacter* Who = Cast<AFOCharacter>(GetOwner());
	if (!Who) return;
	const FVector P = Who->GetActorLocation();
	const FVector Fwd = Who->GetActorForwardVector();
	AActor* Best = nullptr; float BestD = 1e9f;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		IFOInteractable* I = Cast<IFOInteractable>(A);
		if (!I || !I->CanInteract(Who)) continue;
		FVector To = A->GetActorLocation() - P; To.Z = 0.f;
		const float D = To.Size();
		if (D > FMath::Min(SearchRadius, I->GetInteractRange())) continue;
		if (D > 80.f && FVector::DotProduct(To / D, Fwd) < FrontDot) continue;
		if (D < BestD) { BestD = D; Best = A; }
	}
	Target = Best;
}

FString UFOInteractionComponent::GetPrompt() const
{
	if (IFOInteractable* I = Cast<IFOInteractable>(Target.Get())) return I->GetPrompt();
	return FString();
}

void UFOInteractionComponent::TryInteract()
{
	AFOCharacter* Who = Cast<AFOCharacter>(GetOwner());
	if (IFOInteractable* I = Cast<IFOInteractable>(Target.Get()))
		if (Who && I->CanInteract(Who)) I->Interact(Who);
}
