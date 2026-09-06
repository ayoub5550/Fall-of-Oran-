#include "Components/FOHealthComponent.h"

UFOHealthComponent::UFOHealthComponent() { PrimaryComponentTick.bCanEverTick = false; }

float UFOHealthComponent::ApplyDamage(float Amount, AActor*)
{
	if (IsDead() || bInvulnerable || Amount <= 0.f) return 0.f;
	const float Before = Health;
	Health = FMath::Max(0.f, Health - Amount);
	OnChanged.Broadcast(Health, Health - Before);
	if (Health <= 0.f) OnDied.Broadcast();
	return Before - Health;
}

float UFOHealthComponent::Heal(float Amount)
{
	if (IsDead() || Amount <= 0.f) return 0.f;
	const float Before = Health;
	Health = FMath::Min(MaxHealth, Health + Amount);
	OnChanged.Broadcast(Health, Health - Before);
	return Health - Before;
}

void UFOHealthComponent::Reset(float NewMax)
{
	if (NewMax > 0.f) MaxHealth = NewMax;
	Health = MaxHealth;
	OnChanged.Broadcast(Health, 0.f);
}
