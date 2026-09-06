#include "FOZombie.h"
#include "FallOfOran.h"
#include "FOCharacter.h"
#include "FOGameMode.h"
#include "FOWorldBuilder.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInstanceDynamic.h"

static const TCHAR* ZombieMeshPaths[4] = {
	TEXT("/Game/Chars/Zombies/SK_z_war.SK_z_war"),
	TEXT("/Game/Chars/Zombies/SK_z_girl.SK_z_girl"),
	TEXT("/Game/Chars/Zombies/SK_z_cop.SK_z_cop"),
	TEXT("/Game/Chars/Zombies/SK_z_parasite.SK_z_parasite") };

TMap<EZAnim, UAnimSequence*>* AFOZombie::SharedAnims()
{
	static TMap<EZAnim, UAnimSequence*> M;
	if (M.Num() == 0)
	{
		auto L = [](EZAnim A, const TCHAR* P) { if (UAnimSequence* S = LoadObject<UAnimSequence>(nullptr, P)) { S->AddToRoot(); M.Add(A, S); } };
		L(EZAnim::Idle, TEXT("/Game/Chars/Zombies/Anims/z_idle.z_idle"));
		L(EZAnim::Walk, TEXT("/Game/Chars/Zombies/Anims/z_walk.z_walk"));
		L(EZAnim::Run, TEXT("/Game/Chars/Zombies/Anims/z_run.z_run"));
		L(EZAnim::Attack, TEXT("/Game/Chars/Zombies/Anims/z_attack.z_attack"));
		L(EZAnim::Bite, TEXT("/Game/Chars/Zombies/Anims/z_bite.z_bite"));
		L(EZAnim::Scream, TEXT("/Game/Chars/Zombies/Anims/z_scream.z_scream"));
		L(EZAnim::Hit, TEXT("/Game/Chars/Zombies/Anims/z_hit.z_hit"));
		L(EZAnim::Death, TEXT("/Game/Chars/Zombies/Anims/z_death.z_death"));
		L(EZAnim::Crawl, TEXT("/Game/Chars/Zombies/Anims/z_crawl.z_crawl"));
		L(EZAnim::StandUp, TEXT("/Game/Chars/Zombies/Anims/z_standup.z_standup"));
	}
	return &M;
}

AFOZombie::AFOZombie()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Zombie"));
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 100.f;
	GetCharacterMovement()->MaxAcceleration = 600.f;
	bUseControllerRotationYaw = false;
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	GetMesh()->SetCollisionProfileName(TEXT("NoCollision"));

	Eye = CreateDefaultSubobject<UPointLightComponent>(TEXT("Eye"));
	Eye->SetupAttachment(GetMesh());
	Eye->SetRelativeLocation(FVector(0.f, -15.f, 165.f));
	Eye->SetIntensity(6.f);
	Eye->SetLightColor(FLinearColor(0.9f, 0.1f, 0.08f));
	Eye->SetAttenuationRadius(90.f);
	Eye->SetCastShadows(false);

	GroanSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/groan.groan"));
	ScreamSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/scream.scream"));
	DieSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/zombie_die.zombie_die"));
	BiteSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/bite.bite"));
}

void AFOZombie::BeginPlay()
{
	Super::BeginPlay();
	Hp *= HpMul;
	Anims = *SharedAnims();
	if (USkeletalMesh* SK = LoadObject<USkeletalMesh>(nullptr, ZombieMeshPaths[FMath::Clamp(Variant, 0, 3)]))
	{
		GetMesh()->SetSkeletalMesh(SK);
		// Decay tint variation
		static const FLinearColor Tints[5] = { {0.95f,1.f,0.92f}, {0.82f,0.86f,0.84f}, {1.f,0.93f,0.88f}, {0.78f,0.84f,0.88f}, {0.9f,0.9f,0.9f} };
		const FLinearColor T = Tints[FMath::RandRange(0, 4)];
		for (int32 i = 0; i < GetMesh()->GetNumMaterials(); i++)
			if (UMaterialInstanceDynamic* MID = GetMesh()->CreateDynamicMaterialInstance(i))
				MID->SetVectorParameterValue(TEXT("Tint"), T);
	}
	const float S = FMath::FRandRange(0.94f, 1.06f) * (Variant == 1 ? 0.95f : 1.f);
	GetMesh()->SetRelativeScale3D(FVector(S));
	GetCharacterMovement()->MaxWalkSpeed = 100.f;
	PlayAnim(EZAnim::Walk, true, FMath::FRandRange(0.85f, 1.15f));
	WanderTimer = FMath::FRandRange(0.5f, 3.f);
	GroanTimer = FMath::FRandRange(2.f, 9.f);
}

void AFOZombie::PlayAnim(EZAnim A, bool bLoop, float Speed)
{
	if (CurrentAnim == A && bLoop && OneShotTimer <= 0.f)
	{
		GetMesh()->SetPlayRate(Speed);
		return;
	}
	UAnimSequence** S = Anims.Find(A);
	if (!S || !*S) return;
	CurrentAnim = A;
	GetMesh()->PlayAnimation(*S, bLoop);
	GetMesh()->SetPlayRate(Speed);
	OneShotTimer = bLoop ? 0.f : (*S)->GetPlayLength() / Speed;
}

void AFOZombie::TakeHit(float Damage, AFOCharacter* From)
{
	if (bDying) return;
	Hp -= Damage;
	bChasing = true;
	if (Hp <= 0.f) { Die(From); return; }
	if (FMath::FRand() < 0.5f) PlayAnim(EZAnim::Hit, false, 1.6f);
}

void AFOZombie::Die(AFOCharacter* Killer)
{
	bDying = true;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	if (Eye) Eye->SetIntensity(0.f);
	PlayAnim(EZAnim::Death, false, 1.f);
	DeathTimer = 4.5f;
	if (DieSound) UGameplayStatics::PlaySoundAtLocation(this, DieSound, GetActorLocation());
	if (AFOWorldBuilder* WB = AFOWorldBuilder::Get(GetWorld())) WB->SpawnBloodPool(GetActorLocation());
	if (AFOGameMode* GM = GetWorld()->GetAuthGameMode<AFOGameMode>()) GM->ReportEvent(FFOGameEvent(EFOGameEvent::ZombieKilled, FName(*FString::FromInt(Variant))));
}

void AFOZombie::Tick(float Dt)
{
	Super::Tick(Dt);
	if (OneShotTimer > 0.f) OneShotTimer -= Dt;
	if (bDying)
	{
		DeathTimer -= Dt;
		if (DeathTimer < 1.5f) AddActorWorldOffset(FVector(0, 0, -60.f * Dt)); // sink into the ground
		if (DeathTimer <= 0.f) Destroy();
		return;
	}
	AFOGameMode* GM = GetWorld()->GetAuthGameMode<AFOGameMode>();
	if (!GM || GM->State != EFOState::Playing) return;
	if (!Target) Target = Cast<AFOCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Target || Target->IsDead()) return;

	FVector To = Target->GetActorLocation() - GetActorLocation();
	To.Z = 0.f;
	const float Dist = To.Size();

	// Perf LOD: far zombies sleep (no AI, no anim ticks)
	if (Dist > 4500.f)
	{
		GetMesh()->bPauseAnims = true;
		GetMesh()->SetComponentTickEnabled(false);
		return;
	}
	GetMesh()->bPauseAnims = false;
	GetMesh()->SetComponentTickEnabled(true);

	if (Dist < DetectRange) bChasing = true;
	else if (Dist > DetectRange * 2.f) bChasing = false;

	FVector Move = FVector::ZeroVector;
	if (bChasing)
	{
		if (!bScreamed)
		{
			bScreamed = true;
			PlayAnim(EZAnim::Scream, false, 1.f);
			if (ScreamSound) UGameplayStatics::PlaySoundAtLocation(this, ScreamSound, GetActorLocation());
		}
		const float Speed = bRunner ? 330.f : 190.f;
		GetCharacterMovement()->MaxWalkSpeed = Speed;
		if (OneShotTimer <= 0.f) Move = To.GetSafeNormal();
		if (Dist > 10.f) SetActorRotation(FMath::RInterpTo(GetActorRotation(), To.Rotation(), Dt, 6.f));
		AttackTimer -= Dt;
		if (Dist <= AttackRange && AttackTimer <= 0.f)
		{
			AttackTimer = AttackCooldown;
			PlayAnim(FMath::FRand() < 0.5f ? EZAnim::Attack : EZAnim::Bite, false, 1.4f);
			if (BiteSound) UGameplayStatics::PlaySoundAtLocation(this, BiteSound, GetActorLocation());
			Target->TakeHit(AttackDamage);
		}
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = 60.f;
		WanderTimer -= Dt;
		if (WanderTimer <= 0.f)
		{
			WanderTimer = FMath::FRandRange(2.f, 5.f);
			const float A = FMath::FRandRange(0.f, 2.f * PI);
			WanderDir = FVector(FMath::Cos(A), FMath::Sin(A), 0.f);
		}
		Move = WanderDir;
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), WanderDir.Rotation(), Dt, 2.f));
	}
	if (!Move.IsNearlyZero()) AddMovementInput(Move, 1.f);

	if (OneShotTimer <= 0.f)
	{
		if (bChasing && Dist <= AttackRange + 40.f) PlayAnim(EZAnim::Idle, true);
		else if (bChasing) PlayAnim(bRunner ? EZAnim::Run : EZAnim::Walk, true, bRunner ? 1.f : 1.35f);
		else PlayAnim(EZAnim::Walk, true, 0.8f);
	}

	GroanTimer -= Dt;
	if (GroanTimer <= 0.f)
	{
		GroanTimer = bChasing ? FMath::FRandRange(2.5f, 7.f) : FMath::FRandRange(6.f, 14.f);
		if (GroanSound) UGameplayStatics::PlaySoundAtLocation(this, GroanSound, GetActorLocation(), 1.f, FMath::FRandRange(0.75f, 1.1f));
	}
}
