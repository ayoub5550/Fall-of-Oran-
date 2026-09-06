#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FOCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USpotLightComponent;
class UPointLightComponent;
class UInputMappingContext;
class UInputAction;
class UAnimSequence;
struct FInputActionValue;

UENUM()
enum class EFOAnim : uint8 { Idle, Walk, Run, Shoot, Reload, Hit, Death, InjuredIdle, InjuredWalk };

/** Third-person survivor (RE4-remake style over-the-shoulder camera). */
UCLASS()
class AFOCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AFOCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// Gameplay API (mirrors the Godot v1.4 API so docs stay valid)
	void TakeHit(float Amount);
	void AddAmmo(int32 N);
	void AddHealth(float N);
	void TryShoot();
	bool IsDead() const { return Health <= 0.f; }

	float Health = 100.f;
	int32 Ammo = 12;
	int32 Reserve = 36;
	static constexpr int32 MagSize = 12;
	static constexpr float GunDamage = 40.f;
	static constexpr float GunRange = 4500.f; // cm
	int32 Kills = 0;

	UPROPERTY() USpringArmComponent* Boom = nullptr;
	UPROPERTY() UCameraComponent* Cam = nullptr;
	UPROPERTY() USpotLightComponent* Flashlight = nullptr;
	UPROPERTY() UPointLightComponent* Muzzle = nullptr;
	UPROPERTY() UPointLightComponent* Fill = nullptr;

	// Animation (played directly on the skeletal mesh, no AnimBP needed headless)
	UPROPERTY() TMap<EFOAnim, UAnimSequence*> Anims;
	EFOAnim CurrentAnim = EFOAnim::Idle;
	float OneShotTimer = 0.f;
	void PlayAnim(EFOAnim A, bool bLoop, float Speed = 1.f);

	// Input
	UPROPERTY() UInputMappingContext* IMC = nullptr;
	UPROPERTY() UInputAction* IA_Move = nullptr;
	UPROPERTY() UInputAction* IA_Look = nullptr;
	UPROPERTY() UInputAction* IA_Fire = nullptr;
	UPROPERTY() UInputAction* IA_Sprint = nullptr;
	void OnMove(const FInputActionValue& V);
	void OnLook(const FInputActionValue& V);
	void OnFire(const FInputActionValue& V);
	void OnSprintStart(const FInputActionValue& V);
	void OnSprintEnd(const FInputActionValue& V);

	// Touch look: right half of the screen drags the camera
	void OnTouchBegin(ETouchIndex::Type Idx, FVector Loc);
	void OnTouchMove(ETouchIndex::Type Idx, FVector Loc);
	void OnTouchEnd(ETouchIndex::Type Idx, FVector Loc);
	int32 LookTouch = -1; FVector2D LastTouch;

	bool bSprinting = false;
	float FireCooldown = 0.f;
	bool bReloading = false;
	float ReloadTimer = 0.f;
	float MuzzleTimer = 0.f;
	FVector2D MoveInput = FVector2D::ZeroVector;

	UPROPERTY() USoundBase* GunSound = nullptr;
	UPROPERTY() USoundBase* ClickSound = nullptr;
	UPROPERTY() USoundBase* ReloadSound = nullptr;
	UPROPERTY() USoundBase* HurtSound = nullptr;
	UPROPERTY() USoundBase* HeartbeatSound = nullptr;
	UPROPERTY() UAudioComponent* Heartbeat = nullptr;
	float HeartbeatCooldown = 0.f;

	void StartReload();
	void SpawnBloodAt(const FVector& Loc, const FVector& Normal);
};
