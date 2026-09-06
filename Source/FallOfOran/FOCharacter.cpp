#include "FOCharacter.h"
#include "FallOfOran.h"
#include "FOZombie.h"
#include "FOGameMode.h"
#include "FOWorldBuilder.h"
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
	GetCharacterMovement()->MaxWalkSpeed = 260.f;
	GetCharacterMovement()->MaxAcceleration = 900.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1400.f;

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(RootComponent);
	Boom->TargetArmLength = 260.f;
	Boom->SocketOffset = FVector(0.f, 55.f, 30.f); // over the right shoulder
	Boom->TargetOffset = FVector(0.f, 0.f, 60.f);
	Boom->bUsePawnControlRotation = true;
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 12.f;
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
void AFOCharacter::OnFire(const FInputActionValue&) { TryShoot(); }
void AFOCharacter::OnSprintStart(const FInputActionValue&) { bSprinting = true; }
void AFOCharacter::OnSprintEnd(const FInputActionValue&) { bSprinting = false; }

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
	if (bAnimStarted && CurrentAnim == A && OneShotTimer <= 0.f && bLoop) return;
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

	FireCooldown -= Dt;
	MuzzleTimer -= Dt;
	if (Muzzle) Muzzle->SetIntensity(MuzzleTimer > 0.f ? 4000.f : 0.f);
	if (OneShotTimer > 0.f) OneShotTimer -= Dt;
	if (bReloading)
	{
		ReloadTimer -= Dt;
		if (ReloadTimer <= 0.f)
		{
			bReloading = false;
			const int32 Need = MagSize - Ammo;
			const int32 Take = FMath::Min(Need, Reserve);
			Ammo += Take; Reserve -= Take;
		}
	}

	if (IsDead())
	{
		GetCharacterMovement()->StopMovementImmediately();
		return;
	}
	if (!bPlaying) { MoveInput = FVector2D::ZeroVector; PlayAnim(EFOAnim::Idle, true); return; }

	// Movement relative to the camera yaw; body faces camera yaw (RE4 style)
	const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
	const FVector Fwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	const float Injured = Health < 30.f ? 0.7f : 1.f;
	GetCharacterMovement()->MaxWalkSpeed = (bSprinting ? 420.f : 260.f) * Injured;
	if (!MoveInput.IsNearlyZero(0.05f))
	{
		AddMovementInput(Fwd, MoveInput.Y);
		AddMovementInput(Right, MoveInput.X);
	}
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), YawRot, Dt, 10.f));

	// Animation state
	if (OneShotTimer <= 0.f)
	{
		const float Spd = GetVelocity().Size2D();
		if (Spd > 300.f) PlayAnim(EFOAnim::Run, true);
		else if (Spd > 20.f) PlayAnim(Health < 30.f ? EFOAnim::InjuredWalk : EFOAnim::Walk, true, FMath::Clamp(Spd / 220.f, 0.7f, 1.3f));
		else PlayAnim(Health < 30.f ? EFOAnim::InjuredIdle : EFOAnim::Idle, true);
	}

	// Heartbeat when hurt
	if (Heartbeat)
	{
		const float Want = Health < 40.f ? FMath::GetMappedRangeValueClamped(FVector2D(0.f, 40.f), FVector2D(1.f, 0.f), Health) : 0.f;
		Heartbeat->SetVolumeMultiplier(FMath::FInterpTo(Heartbeat->VolumeMultiplier, Want, Dt, 2.f));
		if (Want > 0.f && !Heartbeat->IsPlaying()) Heartbeat->Play();
	}
}

void AFOCharacter::StartReload()
{
	if (bReloading || Reserve <= 0 || Ammo >= MagSize) return;
	bReloading = true;
	ReloadTimer = 1.6f;
	PlayAnim(EFOAnim::Reload, false, 1.4f);
	if (ReloadSound) UGameplayStatics::PlaySound2D(this, ReloadSound);
}

void AFOCharacter::TryShoot()
{
	AFOGameMode* GM = GetWorld()->GetAuthGameMode<AFOGameMode>();
	if (!GM || GM->State != EFOState::Playing || IsDead() || bReloading || FireCooldown > 0.f) return;
	if (Ammo <= 0)
	{
		if (ClickSound) UGameplayStatics::PlaySound2D(this, ClickSound);
		StartReload();
		return;
	}
	Ammo--;
	FireCooldown = 0.32f;
	MuzzleTimer = 0.06f;
	PlayAnim(EFOAnim::Shoot, false, 1.6f);
	if (GunSound) UGameplayStatics::PlaySoundAtLocation(this, GunSound, GetActorLocation());

	// Camera-centre ray
	const FVector Start = Cam->GetComponentLocation();
	const FVector Dir = Cam->GetForwardVector();
	FHitResult Hit;
	FCollisionQueryParams QP(SCENE_QUERY_STAT(Shoot), true, this);
	AFOZombie* Target = nullptr;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * GunRange, ECC_Visibility, QP))
	{
		Target = Cast<AFOZombie>(Hit.GetActor());
		if (!Target) SpawnBloodAt(Hit.ImpactPoint, Hit.ImpactNormal); // wall dust/impact: reuse decal spawner
	}
	// Aim assist (from the Godot build): nearest living zombie inside a distance-scaled cone
	if (!Target)
	{
		float Best = 1e9f;
		for (TActorIterator<AFOZombie> It(GetWorld()); It; ++It)
		{
			AFOZombie* Z = *It;
			if (Z->bDying) continue;
			const FVector To = Z->GetActorLocation() + FVector(0, 0, 60.f) - Start;
			const float D = To.Size();
			if (D > GunRange) continue;
			const float Cone = FMath::Clamp(190.f / FMath::Max(D / 100.f, 1.f), 0.12f, 0.35f);
			const float Ang = FMath::Acos(FVector::DotProduct(To.GetSafeNormal(), Dir));
			if (Ang < Cone && D < Best) { Best = D; Target = Z; }
		}
	}
	if (Target)
	{
		Target->TakeHit(GunDamage, this);
		SpawnBloodAt(Target->GetActorLocation() + FVector(0, 0, 80.f), -Dir);
	}
	if (Ammo == 0) StartReload();
}

void AFOCharacter::SpawnBloodAt(const FVector& Loc, const FVector& Normal)
{
	if (AFOWorldBuilder* WB = AFOWorldBuilder::Get(GetWorld())) WB->SpawnBloodSplat(Loc, Normal);
}

void AFOCharacter::TakeHit(float Amount)
{
	if (IsDead()) return;
	AFOGameMode* GM = GetWorld()->GetAuthGameMode<AFOGameMode>();
	if (!GM || GM->State != EFOState::Playing) return;
	Health = FMath::Max(0.f, Health - Amount);
	GM->OnPlayerDamaged();
	if (HurtSound) UGameplayStatics::PlaySound2D(this, HurtSound);
	if (Health <= 0.f)
	{
		PlayAnim(EFOAnim::Death, false);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		GM->OnPlayerDied();
	}
	else PlayAnim(EFOAnim::Hit, false, 1.5f);
}

void AFOCharacter::AddAmmo(int32 N) { Reserve += N; }
void AFOCharacter::AddHealth(float N) { Health = FMath::Min(100.f, Health + N); }
