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
class UFOHealthComponent;
class UFOWeaponComponent;
class UFOInteractionComponent;
struct FInputActionValue;

UENUM()
enum class EFOAnim : uint8 { Idle, Walk, Run, Shoot, Reload, Hit, Death, InjuredIdle, InjuredWalk };

/**
 * Third-person survivor (over-the-shoulder camera).
 * Thin: input, camera, animation and movement feel live here; numbers live in the components:
 *   HealthComp (UFOHealthComponent) — hp / death
 *   Weapon     (UFOWeaponComponent) — ammo, fire, reload, aim assist
 *   Interaction(UFOInteractionComponent) — "use" prompt for notes / puzzles
 */
UCLASS()
class AFOCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AFOCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// ---- gameplay API used by zombies, pickups, HUD
	void TakeHit(float Amount);
	void AddAmmo(int32 N);
	void AddHealth(float N);
	void TryShoot();
	void TryInteract();
	bool IsDead() const;
	float GetHealth() const;
	float GetHealthFraction() const;
	int32 GetAmmo() const;
	int32 GetReserve() const;
	bool HasInteractTarget() const;
	FString GetInteractPrompt() const;
	int32 Kills = 0;

	// ---- components
	UPROPERTY() UFOHealthComponent* HealthComp = nullptr;
	UPROPERTY() UFOWeaponComponent* Weapon = nullptr;
	UPROPERTY() UFOInteractionComponent* Interaction = nullptr;
	UPROPERTY() USpringArmComponent* Boom = nullptr;
	UPROPERTY() UCameraComponent* Cam = nullptr;
	UPROPERTY() USpotLightComponent* Flashlight = nullptr;
	UPROPERTY() UPointLightComponent* Muzzle = nullptr;
	UPROPERTY() UPointLightComponent* Fill = nullptr;

	// ---- movement feel (tweak here, not in Tick)
	float WalkSpeed = 270.f;
	float SprintSpeed = 430.f;
	float InjuredSpeedMul = 0.7f;
	float InputSmoothing = 9.f;      // how fast the stick value is followed (higher = snappier)
	float BodyTurnSpeed = 11.f;      // yaw interp toward camera
	float BaseFov = 72.f, SprintFov = 80.f;
	float SprintCamLength = 300.f;   // camera pulls back slightly while sprinting

	// ---- animation (played directly on the skeletal mesh; no AnimBP needed headless)
	UPROPERTY() TMap<EFOAnim, UAnimSequence*> Anims;
	void LoadAnims();
	void PlayAnim(EFOAnim A, bool bLoop, float Speed = 1.f);
	EFOAnim CurrentAnim = EFOAnim::Idle;
	bool bAnimStarted = false;   // PlayAnim must not early-out before the first clip is playing
	float OneShotTimer = 0.f;

	// ---- input
	UPROPERTY() UInputMappingContext* IMC = nullptr;
	UPROPERTY() UInputAction* IA_Move = nullptr;
	UPROPERTY() UInputAction* IA_Look = nullptr;
	UPROPERTY() UInputAction* IA_Fire = nullptr;
	UPROPERTY() UInputAction* IA_Sprint = nullptr;
	UPROPERTY() UInputAction* IA_Interact = nullptr;
	void OnMove(const FInputActionValue& V);
	void OnLook(const FInputActionValue& V);
	void OnFire(const FInputActionValue& V);
	void OnSprintStart(const FInputActionValue& V);
	void OnSprintEnd(const FInputActionValue& V);
	void OnInteract(const FInputActionValue& V);
	void OnTouchBegin(ETouchIndex::Type Idx, FVector Loc);
	void OnTouchMove(ETouchIndex::Type Idx, FVector Loc);
	void OnTouchEnd(ETouchIndex::Type Idx, FVector Loc);
	int32 LookTouch = -1; FVector2D LastTouch;

	bool bSprinting = false;
	float MuzzleTimer = 0.f;
	FVector2D MoveInput = FVector2D::ZeroVector;    // raw
	FVector2D MoveSmoothed = FVector2D::ZeroVector; // followed

	UPROPERTY() USoundBase* GunSound = nullptr;
	UPROPERTY() USoundBase* ClickSound = nullptr;
	UPROPERTY() USoundBase* ReloadSound = nullptr;
	UPROPERTY() USoundBase* HurtSound = nullptr;
	UPROPERTY() USoundBase* HeartbeatSound = nullptr;
	UPROPERTY() UAudioComponent* Heartbeat = nullptr;

private:
	void BindWeaponEvents();
	void SpawnBloodAt(const FVector& Loc, const FVector& Normal);
	void OnDied();
};
