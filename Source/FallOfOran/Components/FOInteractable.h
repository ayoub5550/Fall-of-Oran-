#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FOInteractable.generated.h"

class AFOCharacter;

UINTERFACE(MinimalAPI)
class UFOInteractable : public UInterface { GENERATED_BODY() };

/**
 * Anything the player can "use": notes, breakers, keypads, doors.
 * Implement on an AActor. The player's UFOInteractionComponent finds the closest one in front of the
 * player and shows GetPrompt() on the HUD "تفاعل" button.
 */
class IFOInteractable
{
	GENERATED_BODY()
public:
	virtual bool CanInteract(AFOCharacter* Who) const { return true; }
	virtual FString GetPrompt() const { return TEXT("تفاعل"); }
	virtual void Interact(AFOCharacter* Who) = 0;
	/** Max distance (cm) at which this shows up. */
	virtual float GetInteractRange() const { return 230.f; }
};
