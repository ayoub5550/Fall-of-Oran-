#include "FOCharacter.h"
#include "FallOfOran.h"
#include "FOZombie.h"
#include "FOGameMode.h"
#include "FOWorldBuilder.h"
#include "Components/FOHealthComponent.h"
#include "Components/FOWeaponComponent.h"
#include "Components/FOInteractionComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"

AFOCharacter::AFOCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false; // RE-style: body follows camera yaw
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->MaxAcceleration = 1100.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1600.f;
	GetCharacterMovement()->GroundFriction = 8.f;
	GetCharacterMovement()->BrakingFrictionFactor = 1.5f;
	GetCharacterMovement()->bUseSeparateBrakingFriction = true;
	GetCharacterMovement()->PerchRadiusThreshold = 20.f;   // don't hang on kerbs
	GetCharacterMovement()->SetWalkableFloorAngle(50.f);
	GetCharacterMovement()->MaxStepHeight = 48.f;           // kerbs, debris

	HealthComp = CreateDefaultSubobject<UFOHealthComponent>(TEXT("Health"));
	Weapon = CreateDefaultSubobject<UFOWeaponComponent>(TEXT("Weapon"));
	Interaction = CreateDefaultSubobject<UFOInteractionComponent>(TEXT("Interaction"));

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(RootComponent);
	Boom->TargetArmLength = 260.f;
	Boom->SocketOffset = FVector(0.f, 55.f, 30.f); // over the right shoulder
	Boom->TargetOffset = FVector(0.f, 0.f, 60.f);
	Boom->bUsePawnControlRotation = true;
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 14.f;
	Boom->bEnableCameraRotationLag = true;
	Boom->CameraRotationLagSpeed = 18.f;
	Boom->CameraLagMaxDistance = 60.f;
	Boom->bDoCollisionTest = true;

	Cam = CreateDefaultSubobject<UCameraComponent>(TEXT("Cam"));
	Cam->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Cam->FieldOfView = 72.f;
	Cam->PostProcessSettings.bOverride_VignetteIntensity = true;
	Cam->PostProcessSettings.VignetteIntensity = 0.75f;
	Cam->PostProcessSettings.bOverride_FilmGrainIntensity = true;
	Cam->PostProcessSettings.FilmGrainIntensity = 0.35f;
	Cam->PostProcessSettings.bOverride_ColorSaturation = true;
	Cam->PostProcessSettings.ColorSaturation = FVector4(0.85f, 0.88f, 0.92f, 1.f);
	Cam->PostProcessSettings.bOverride_ColorContrast = true;
	Cam->PostProcessSettings.ColorContrast = FVector4(1.12f, 1.12f, 1.12f, 1.f);

	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Flashlight->SetRelativeLocation(FVector(40.f, -30.f, -10.f));
	Flashlight->SetIntensity(9000.f);
	Flashlight->SetLightColor(FLinearColor(1.f, 0.93f, 0.8f));
	Flashlight->SetAttenuationRadius(3200.f);
	Flashlight->SetInnerConeAngle(14.f);
	Flashlight->SetOuterConeAngle(30.f);
	Flashlight->SetCastShadows(true);
	Flashlight->SetMobility(EComponentMobility::Movable);

	Muzzle = CreateDefaultSubobject<UPointLightComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(GetMesh());
	Muzzle->SetRelativeLocation(FVector(0.f, -40.f, 130.f));
	Muzzle->SetIntensity(0.f);
	Muzzle->SetLightColor(FLinearColor(1.f, 0.75f, 0.35f));
	Muzzle->SetAttenuationRadius(700.f);
	Muzzle->SetCastShadows(false);

	Fill = CreateDefaultSubobject<UPointLightComponent>(TEXT("Fill"));
	Fill->SetupAttachment(RootComponent);
	Fill->SetRelativeLocation(FVector(-140.f, 60.f, 160.f));
	Fill->SetIntensity(180.f);
	Fill->SetLightColor(FLinearColor(0.75f, 0.82f, 1.f));
	Fill->SetAttenuationRadius(320.f);
	Fill->SetCastShadows(false);

	// Mixamo SWAT hero (imported by Tools/import_assets.py). Mixamo forward is -Y after import → yaw -90.
	USkeletalMesh* SK = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Chars/Hero/SK_hero_swat.SK_hero_swat"));
	if (SK)
	{
		GetMesh()->SetSkeletalMesh(SK);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		GetMesh()->bCastDynamicShadow = true;
		GetMesh()->bReceivesDecals = false;
	}
	// NOTE: animation clips are loaded in BeginPlay (see LoadAnims): loading UAnimSequence in the
	// constructor happens during CDO creation at module startup, before the animation systems are
	// ready -> clips ended up without usable data and the hero stood in T-pose.

	GunSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/gunshot.gunshot"));
	ClickSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/click.click"));
	ReloadSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/reload.reload"));
	HurtSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/hurt.hurt"));
	HeartbeatSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/heartbeat.heartbeat"));

	// Runtime-built Enhanced Input (no assets needed). Gamepad_Left2D/Right2D are also what the
	// default mobile virtual joysticks emit, so touch works with the same mappings.
	IMC = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC"));
	IA_Move = CreateDefaultSubobject<UInputAction>(TEXT("IA_Move"));
	IA_Move->ValueType = EInputActionValueType::Axis2D;
	IA_Look = CreateDefaultSubobject<UInputAction>(TEXT("IA_Look"));
	IA_Look->ValueType = EInputActionValueType::Axis2D;
	IA_Fire = CreateDefaultSubobject<UInputAction>(TEXT("IA_Fire"));
	IA_Fire->ValueType = EInputActionValueType::Boolean;
	IA_Sprint = CreateDefaultSubobject<UInputAction>(TEXT("IA_Sprint"));
	IA_Sprint->ValueType = EInputActionValueType::Boolean;
	IA_Interact = CreateDefaultSubobject<UInputAction>(TEXT("IA_Interact"));
	IA_Interact->ValueType = EInputActionValueType::Boolean;

	auto Swz = [this](const TCHAR* N) { auto* M = CreateDefaultSubobject<UInputModifierSwizzleAxis>(N); M->Order = EInputAxisSwizzle::YXZ; return M; };
	auto Neg = [this](const TCHAR* N) { return CreateDefaultSubobject<UInputModifierNegate>(N); };
	{ FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Move, EKeys::W); M.Modifiers.Add(Swz(TEXT("SwzW"))); }
	{ FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Move, EKeys::S); M.Modifiers.Add(Swz(TEXT("SwzS"))); M.Modifiers.Add(Neg(TEXT("NegS"))); }
	{ FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Move, EKeys::A); M.Modifiers.Add(Neg(TEXT("NegA"))); }
	IMC->MapKey(IA_Move, EKeys::D);
	IMC->MapKey(IA_Move, EKeys::Gamepad_Left2D);
	{ FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Look, EKeys::Mouse2D); auto* Sc = CreateDefaultSubobject<UInputModifierScalar>(TEXT("MouseScale")); Sc->Scalar = FVector(1.f, -1.f, 1.f); M.Modifiers.Add(Sc); }
	{ FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Look, EKeys::Gamepad_Right2D); auto* Sc = CreateDefaultSubobject<UInputModifierScalar>(TEXT("PadScale")); Sc->Scalar = FVector(2.4f, 1.8f, 1.f); M.Modifiers.Add(Sc); }
	IMC->MapKey(IA_Fire, EKeys::LeftMouseButton);
	IMC->MapKey(IA_Fire, EKeys::Gamepad_RightTrigger);
	IMC->MapKey(IA_Fire, EKeys::SpaceBar);
	IMC->MapKey(IA_Sprint, EKeys::LeftShift);
	IMC->MapKey(IA_Sprint, EKeys::Gamepad_LeftThumbstick);
	IMC->MapKey(IA_Interact, EKeys::E);
	IMC->MapKey(IA_Interact, EKeys::Gamepad_FaceButton_Left);
}

void AFOCharacter::LoadAnims()
{
	if (Anims.Num() > 0) return;
	ON_SCOPE_EXIT { UE_LOG(LogTemp, Display, TEXT("FOCharacter: %d hero anims loaded"), Anims.Num()); };
	auto LoadAnim = [this](EFOAnim A, const TCHAR* Path) {
		if (UAnimSequence* S = LoadObject<UAnimSequence>(nullptr, Path)) Anims.Add(A, S);
	};
	LoadAnim(EFOAnim::Idle, TEXT("/Game/Chars/Hero/Anims/h_idle.h_idle"));
	LoadAnim(EFOAnim::Walk, TEXT("/Game/Chars/Hero/Anims/h_walk.h_walk"));
	LoadAnim(EFOAnim::Run, TEXT("/Game/Chars/Hero/Anims/h_run.h_run"));
	LoadAnim(EFOAnim::Shoot, TEXT("/Game/Chars/Hero/Anims/h_shoot.h_shoot"));
	LoadAnim(EFOAnim::Reload, TEXT("/Game/Chars/Hero/Anims/h_reload.h_reload"));
	LoadAnim(EFOAnim::Hit, TEXT("/Game/Chars/Hero/Anims/h_hit.h_hit"));
	LoadAnim(EFOAnim::Death, TEXT("/Game/Chars/Hero/Anims/h_death.h_death"));
	LoadAnim(EFOAnim::InjuredIdle, TEXT("/Game/Chars/Hero/Anims/h_injured_idle.h_injured_idle"));
	LoadAnim(EFOAnim::InjuredWalk, TEXT("/Game/Chars/Hero/Anims/h_injured_walk.h_injured_walk"));
}

void AFOCharacter::BeginPlay()
{
	Super::BeginPlay();
	LoadAnims();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
			if (auto* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				Sub->AddMappingContext(IMC, 0);
		PC->SetControlRotation(FRotator(-8.f, GetActorRotation().Yaw, 0.f));
	}
	PlayAnim(EFOAnim::Idle, true);
	BindWeaponEvents();
	HealthComp->OnDied.AddUObject(this, &AFOCharacter::OnDied);
	if (HeartbeatSound)
	{
		Heartbeat = UGameplayStatics::SpawnSound2D(this, HeartbeatSound, 0.f, 1.f, 0.f, nullptr, true, false);
	}
}

void AFOCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AFOCharacter::OnMove);
		EIC->BindAction(IA_Move, ETriggerEvent::Completed, this, &AFOCharacter::OnMove);
		EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AFOCharacter::OnLook);
		EIC->BindAction(IA_Fire, ETriggerEvent::Started, this, &AFOCharacter::OnFire);
		EIC->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AFOCharacter::OnSprintStart);
		EIC->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AFOCharacter::OnSprintEnd);
		EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &AFOCharacter::OnInteract);
	}
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AFOCharacter::OnTouchBegin);
	PlayerInputComponent->BindTouch(IE_Repeat, this, &AFOCharacter::OnTouchMove);
	PlayerInputComponent->BindTouch(IE_Released, this, &AFOCharacter::OnTouchEnd);
}

void AFOCharacter::OnMove(const FInputActionValue& V) { MoveInput = V.Get<FVector2D>(); }
void AFOCharacter::OnLook(const FInputActionValue& V)
{
	const FVector2D L = V.Get<FVector2D>();
	AddControllerYawInput(L.X * 0.9f);
	AddControllerPitchInput(L.Y * 0.9f);
}
void AFOCharacter::OnFire(const FInputActionValue&)
{
	// Desktop GameOnly input captures the mouse before the Slate menu sees it.
	// Let the existing fire action (Space/click/controller) start or retry, but
	// never fire a shot on the same input used to dismiss an overlay.
	if (AFOGameMode* GM = GetWorld()->GetAuthGameMode<AFOGameMode>())
	{
		if (GM->State != EFOState::Playing) { GM->StartGame(); return; }
	}
	TryShoot();
}
void AFOCharacter::OnSprintStart(const FInputActionValue&) { bSprinting = true; }
void AFOCharacter::OnSprintEnd(const FInputActionValue&) { bSprinting = false; }
void AFOCharacter::OnInteract(const FInputActionValue&) { TryInteract(); }

void AFOCharacter::OnTouchBegin(ETouchIndex::Type Idx, FVector Loc)
{
	// Right half of the screen (outside the virtual sticks) = look drag; the fire button is a Slate widget.
	int32 SX = 1280, SY = 720;
	if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->GetViewportSize(SX, SY);
	if (LookTouch < 0 && Loc.X > SX * 0.5f && Loc.Y < SY * 0.55f) { LookTouch = (int32)Idx; LastTouch = FVector2D(Loc.X, Loc.Y); }
}
void AFOCharacter::OnTouchMove(ETouchIndex::Type Idx, FVector Loc)
{
	if ((int32)Idx != LookTouch) return;
	const FVector2D D = FVector2D(Loc.X, Loc.Y) - LastTouch;
	LastTouch = FVector2D(Loc.X, Loc.Y);
	AddControllerYawInput(D.X * 0.16f);
	AddControllerPitchInput(D.Y * 0.16f);
}
void AFOCharacter::OnTouchEnd(ETouchIndex::Type Idx, FVector) { if ((int32)Idx == LookTouch) LookTouch = -1; }

void AFOCharacter::PlayAnim(EFOAnim A, bool bLoop, float Speed)
{
	if (bAnimStarted && CurrentAnim == A && OneShotTimer <= 0.f && bLoop) { GetMesh()->SetPlayRate(Speed); return; } // same clip: only retune speed
	UAnimSequence** S = Anims.Find(A);
	if (!S || !*S) return;
	bAnimStarted = true;
	CurrentAnim = A;
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	GetMesh()->PlayAnimation(*S, bLoop);
	GetMesh()->SetPlayRate(Speed);
	OneShotTimer = bLoop ? 0.f : (*S)->GetPlayLength() / Speed;
}

void AFOCharacter::Tick(float Dt)
{
	Super::Tick(Dt);
	AFOGameMode* GM = GetWorld()->GetAuthGameMode<AFOGameMode>();
	const bool bPlaying = GM && GM->State == EFOState::Playing;

	MuzzleTimer -= Dt;
	if (Muzzle) Muzzle->SetIntensity(MuzzleTimer > 0.f ? 4000.f : 0.f);
	if (OneShotTimer > 0.f) OneShotTimer -= Dt;

	if (IsDead()) { GetCharacterMovement()->StopMovementImmediately(); return; }
	if (!bPlaying || (GM && GM->IsKeypadOpen())) { MoveInput = FVector2D::ZeroVector; MoveSmoothed = FVector2D::ZeroVector; PlayAnim(EFOAnim::Idle, true); return; }

	// --- movement: camera-relative, smoothed stick, body turns toward camera yaw (RE-style)
	MoveSmoothed = FMath::Vector2DInterpTo(MoveSmoothed, MoveInput.GetClampedToMaxSize(1.f), Dt, InputSmoothing);
	const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
	const FVector Fwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	const bool bInjured = HealthComp->IsLow();
	const bool bCanSprint = bSprinting && !bInjured && !Weapon->bReloading && MoveSmoothed.Y > 0.3f;
	const float TargetSpeed = (bCanSprint ? SprintSpeed : WalkSpeed) * (bInjured ? InjuredSpeedMul : 1.f);
	GetCharacterMovement()->MaxWalkSpeed = FMath::FInterpTo(GetCharacterMovement()->MaxWalkSpeed, TargetSpeed, Dt, 6.f);
	if (!MoveSmoothed.IsNearlyZero(0.03f))
	{
		AddMovementInput(Fwd, MoveSmoothed.Y);
		AddMovementInput(Right, MoveSmoothed.X);
	}
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), YawRot, Dt, BodyTurnSpeed));

	// --- camera feel: FOV and boom ease with sprint
	const float Spd = GetVelocity().Size2D();
	const bool bSprintingNow = Spd > WalkSpeed + 40.f;
	Cam->SetFieldOfView(FMath::FInterpTo(Cam->FieldOfView, bSprintingNow ? SprintFov : BaseFov, Dt, 5.f));
	Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, bSprintingNow ? SprintCamLength : 260.f, Dt, 4.f);

	// --- animation state
	if (OneShotTimer <= 0.f)
	{
		if (Spd > WalkSpeed + 30.f) PlayAnim(EFOAnim::Run, true, FMath::Clamp(Spd / SprintSpeed, 0.8f, 1.15f));
		else if (Spd > 20.f) PlayAnim(bInjured ? EFOAnim::InjuredWalk : EFOAnim::Walk, true, FMath::Clamp(Spd / 220.f, 0.7f, 1.3f));
		else PlayAnim(bInjured ? EFOAnim::InjuredIdle : EFOAnim::Idle, true);
	}

	// --- heartbeat when hurt
	if (Heartbeat)
	{
		const float H = HealthComp->GetHealth();
		const float Want = H < 40.f ? FMath::GetMappedRangeValueClamped(FVector2D(0.f, 40.f), FVector2D(1.f, 0.f), H) : 0.f;
		Heartbeat->SetVolumeMultiplier(FMath::FInterpTo(Heartbeat->VolumeMultiplier, Want, Dt, 2.f));
		if (Want > 0.f && !Heartbeat->IsPlaying()) Heartbeat->Play();
	}
}

// ------------------------------------------------------------------ weapon glue
void AFOCharacter::BindWeaponEvents()
{
	Weapon->OnFired.AddLambda([this]() {
		MuzzleTimer = 0.06f;
		PlayAnim(EFOAnim::Shoot, false, 1.6f);
		if (GunSound) UGameplayStatics::PlaySoundAtLocation(this, GunSound, GetActorLocation());
	});
	Weapon->OnDryFire.AddLambda([this]() { if (ClickSound) UGameplayStatics::PlaySound2D(this, ClickSound); });
	Weapon->OnReloadStart.AddLambda([this]() {
		PlayAnim(EFOAnim::Reload, false, 1.4f);
		if (ReloadSound) UGameplayStatics::PlaySound2D(this, ReloadSound);
	});
	Weapon->OnHit.AddLambda([this](AActor* Hit, const FHitResult& R) { SpawnBloodAt(R.ImpactPoint, R.ImpactNormal); });
}

void AFOCharacter::TryShoot()
{
	AFOGameMode* GM = GetWorld()->GetAuthGameMode<AFOGameMode>();
	if (!GM || GM->State != EFOState::Playing || GM->IsKeypadOpen() || IsDead()) return;
	Weapon->Fire(Cam->GetComponentLocation(), Cam->GetForwardVector());
}

void AFOCharacter::TryInteract()
{
	AFOGameMode* GM = GetWorld()->GetAuthGameMode<AFOGameMode>();
	if (!GM || GM->State != EFOState::Playing || IsDead()) return;
	Interaction->TryInteract();
}

void AFOCharacter::SpawnBloodAt(const FVector& Loc, const FVector& Normal)
{
	if (AFOWorldBuilder* WB = AFOWorldBuilder::Get(GetWorld())) WB->SpawnBloodSplat(Loc, Normal);
}

// ------------------------------------------------------------------ health glue
void AFOCharacter::TakeHit(float Amount)
{
	if (IsDead()) return;
	AFOGameMode* GM = GetWorld()->GetAuthGameMode<AFOGameMode>();
	if (!GM || GM->State != EFOState::Playing) return;
	if (HealthComp->ApplyDamage(Amount) <= 0.f) return;
	GM->OnPlayerDamaged();
	if (HurtSound) UGameplayStatics::PlaySound2D(this, HurtSound);
	if (!IsDead()) PlayAnim(EFOAnim::Hit, false, 1.5f);
}

void AFOCharacter::OnDied()
{
	PlayAnim(EFOAnim::Death, false);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	if (AFOGameMode* GM = GetWorld()->GetAuthGameMode<AFOGameMode>()) GM->OnPlayerDied();
}

void AFOCharacter::AddAmmo(int32 N) { Weapon->AddReserve(N); }
void AFOCharacter::AddHealth(float N) { HealthComp->Heal(N); }
bool AFOCharacter::IsDead() const { return HealthComp && HealthComp->IsDead(); }
float AFOCharacter::GetHealth() const { return HealthComp ? HealthComp->GetHealth() : 0.f; }
float AFOCharacter::GetHealthFraction() const { return HealthComp ? HealthComp->GetFraction() : 0.f; }
int32 AFOCharacter::GetAmmo() const { return Weapon ? Weapon->Ammo : 0; }
int32 AFOCharacter::GetReserve() const { return Weapon ? Weapon->Reserve : 0; }
bool AFOCharacter::HasInteractTarget() const { return Interaction && Interaction->HasTarget(); }
FString AFOCharacter::GetInteractPrompt() const { return Interaction ? Interaction->GetPrompt() : FString(); }
