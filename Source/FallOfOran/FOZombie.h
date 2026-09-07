#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FOZombie.generated.h"

class UAnimSequence;
class AFOCharacter;
class UPointLightComponent;

UENUM()
enum class EZAnim : uint8 { Idle, Walk, Run, Attack, Bite, Scream, Hit, Death, Crawl, StandUp };

/** Shambling / running zombie. Simple steering AI (no navmesh needed for the street). */
UCLASS()
class AFOZombie : public ACharacter
{
	GENERATED_BODY()
public:
	AFOZombie();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void TakeHit(float Damage, AFOCharacter* From);
	void Die(AFOCharacter* Killer);
	void PlayAnim(EZAnim A, bool bLoop, float Speed = 1.f);
	bool HasAttackLine() const;

	int32 Variant = 0;          // 0 war, 1 girl, 2 cop, 3 parasite
	bool bRunner = false;       // fast variant
	bool bChasing = false;
	bool bDying = false;
	bool bScreamed = false;
	float Hp = 100.f;
	float HpMul = 1.f;          // per-level difficulty (applied in BeginPlay)
	float AttackTimer = 0.f;
	float AttackWindup = 0.f;
	bool bAttackPending = false;
	bool bHeavy = false;        // non-running police silhouette: durable, slow, avoidable strike
	float ChaseSpeed = 190.f;
	float MeleeDamage = 20.f;
	float MeleeCooldown = 1.3f;
	float WindupSeconds = 0.45f;
	float WanderTimer = 0.f;
	FVector WanderDir = FVector::ZeroVector;
	float GroanTimer = 3.f;
	float OneShotTimer = 0.f;
	float DeathTimer = 0.f;
	EZAnim CurrentAnim = EZAnim::Idle;

	static constexpr float DetectRange = 1400.f;
	static constexpr float AttackRange = 170.f;
	static constexpr float AttackDamage = 20.f;
	static constexpr float AttackCooldown = 1.3f;

	UPROPERTY() TMap<EZAnim, UAnimSequence*> Anims;
	UPROPERTY() UPointLightComponent* Eye = nullptr;
	UPROPERTY() USoundBase* GroanSound = nullptr;
	UPROPERTY() USoundBase* ScreamSound = nullptr;
	UPROPERTY() USoundBase* DieSound = nullptr;
	UPROPERTY() USoundBase* BiteSound = nullptr;
	UPROPERTY() AFOCharacter* Target = nullptr;
	static TMap<EZAnim, UAnimSequence*>* SharedAnims();
};
