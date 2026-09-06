#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FOInteractionComponent.generated.h"

class AFOCharacter;

/** Tracks the best interactable near the owning character; HUD reads Prompt / HasTarget. */
UCLASS()
class UFOInteractionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UFOInteractionComponent();
	virtual void TickComponent(float Dt, ELevelTick, FActorComponentTickFunction*) override;

	bool HasTarget() const { return Target.IsValid(); }
	FString GetPrompt() const;
	/** Called by input (E key / HUD button). */
	void TryInteract();

	float SearchRadius = 260.f;
	float FrontDot = 0.2f;       // must be roughly in front of the player (dot with forward)
private:
	TWeakObjectPtr<AActor> Target;
	float Refresh = 0.f;
};
