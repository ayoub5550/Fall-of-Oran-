#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FOHealthComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FFOHealthChanged, float /*New*/, float /*Delta*/);
DECLARE_MULTICAST_DELEGATE(FFODied);

/**
 * Health for any actor (player, zombies, destructibles). Owns the numbers; the owner reacts to the
 * delegates (animation, sound, game-mode notification). Never modify Health from outside — use ApplyDamage/Heal.
 */
UCLASS()
class UFOHealthComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UFOHealthComponent();

	/** Returns damage actually applied (0 if dead/invulnerable). */
	float ApplyDamage(float Amount, AActor* Instigator = nullptr);
	float Heal(float Amount);
	void Reset(float NewMax = -1.f);

	bool IsDead() const { return Health <= 0.f; }
	float GetHealth() const { return Health; }
	float GetMax() const { return MaxHealth; }
	float GetFraction() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }
	bool IsLow() const { return Health < LowThreshold; }

	UPROPERTY() float MaxHealth = 100.f;
	UPROPERTY() float LowThreshold = 30.f;   // "injured" gameplay state
	bool bInvulnerable = false;

	FFOHealthChanged OnChanged;
	FFODied OnDied;

private:
	UPROPERTY() float Health = 100.f;
};
