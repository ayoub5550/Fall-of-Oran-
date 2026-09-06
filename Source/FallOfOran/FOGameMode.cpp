#include "FOGameMode.h"
#include "FallOfOran.h"
#include "FOCharacter.h"
#include "FOZombie.h"
#include "FOWorldBuilder.h"
#include "FOHud.h"
#include "Core/FOGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "ShaderCompiler.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "Engine/PostProcessVolume.h"

AFOGameMode::AFOGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AFOCharacter::StaticClass();
	DefaultObjective = TEXT("الهدف: اجمع 3 عبوات وقود لفتح بوابة الميناء");
	Objective = DefaultObjective;
}

AFOCharacter* AFOGameMode::Player() const { return Cast<AFOCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)); }

void AFOGameMode::BeginPlay()
{
	Super::BeginPlay();
	// Build the street procedurally, then the HUD.
	World = GetWorld()->SpawnActor<AFOWorldBuilder>(AFOWorldBuilder::StaticClass(), FTransform::Identity);
	if (World)
	{
		World->BuildWorld();
		if (UFOGameInstance* GI = UFOGameInstance::Get(this)) World->ApplyBrightness(GI->Brightness());
	}
	if (GEngine && GEngine->GameViewport)
	{
		Hud = SNew(SFOHud).GameMode(this);
		GEngine->GameViewport->AddViewportWidgetContent(Hud.ToSharedRef(), 10);
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
	State = EFOState::Menu;
	bShotMode = FParse::Param(FCommandLine::Get(), TEXT("FOShots"));
	if (bShotMode) GAreScreenMessagesEnabled = false;
}

void AFOGameMode::Tick(float Dt)
{
	Super::Tick(Dt);
	if (DamageFlash > 0.f) DamageFlash -= Dt;
	if (HintTimer > 0.f) { HintTimer -= Dt; if (HintTimer <= 0.f) Objective = Fuel >= FuelNeeded ? TEXT("البوابة فُتحت! اهرب إلى الميناء") : DefaultObjective; }
	if (RestartTimer > 0.f) { RestartTimer -= Dt; }
	if (bShotMode) TickShots(Dt);
}

void AFOGameMode::StartGame()
{
	if (State == EFOState::Menu) { State = EFOState::Playing; return; }
	if ((State == EFOState::Dead || State == EFOState::Won) && RestartTimer <= 0.f) Restart();
}

void AFOGameMode::Restart()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}

void AFOGameMode::OnZombieKilled(AFOCharacter* Killer)
{
	Kills++;
	if (Killer) Killer->Kills++;
}

void AFOGameMode::OnPlayerDamaged() { DamageFlash = 0.35f; }

void AFOGameMode::OnPlayerDied()
{
	if (State != EFOState::Playing) return;
	State = EFOState::Dead;
	Subtitle = FString::Printf(TEXT("قتلت %d زومبي — المس الشاشة للمحاولة من جديد"), Kills);
	RestartTimer = 2.5f;
}

void AFOGameMode::OnFuelCollected()
{
	Fuel++;
	if (Fuel >= FuelNeeded) SetObjective(TEXT("البوابة فُتحت! اهرب إلى الميناء 🟢"));
	else SetObjective(FString::Printf(TEXT("وقود %d/%d — ابحث عن البقية"), Fuel, FuelNeeded));
	SpawnZombiesBehindPlayer(2); // the horde hears you
}

void AFOGameMode::OnExitReached()
{
	if (State != EFOState::Playing) return;
	if (Fuel >= FuelNeeded) Win();
	else SetObjective(FString::Printf(TEXT("البوابة مقفلة! تحتاج %d عبوات وقود أخرى ⛽"), FuelNeeded - Fuel), 4.f);
}

void AFOGameMode::Win()
{
	State = EFOState::Won;
	Subtitle = FString::Printf(TEXT("نجوت من وهران. القتلى: %d — المس الشاشة للعب مجددًا"), Kills);
	RestartTimer = 2.5f;
}

void AFOGameMode::SetObjective(const FString& Text, float Seconds)
{
	Objective = Text;
	HintTimer = Seconds;
}

void AFOGameMode::AdjustBrightness(float Step)
{
	UFOGameInstance* GI = UFOGameInstance::Get(this);
	const float B = FMath::Clamp((GI ? GI->Brightness() : 1.f) * Step, 0.25f, 4.f);
	if (GI) GI->SetBrightness(B);
	if (World) World->ApplyBrightness(B);
}
float AFOGameMode::CurrentBrightness() const { const UFOGameInstance* GI = UFOGameInstance::Get(this); return GI ? GI->Brightness() : 1.f; }

void AFOGameMode::SpawnZombiesBehindPlayer(int32 Count)
{
	AFOCharacter* P = Player();
	if (!P) return;
	for (int32 i = 0; i < Count; i++)
	{
		FVector Loc = P->GetActorLocation() + FVector(FMath::FRandRange(1400.f, 2200.f), FMath::FRandRange(-400.f, 400.f), 0.f);
		Loc.X = FMath::Clamp(Loc.X, -200.f, AFOWorldBuilder::StreetLength - 200.f);
		Loc.Y = FMath::Clamp(Loc.Y, -500.f, 500.f);
		Loc.Z = 100.f;
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (AFOZombie* Z = GetWorld()->SpawnActorDeferred<AFOZombie>(AFOZombie::StaticClass(), FTransform(FRotator(0, 180.f, 0), Loc)))
		{
			Z->Variant = FMath::RandRange(0, 3);
			Z->bRunner = FMath::FRand() < 0.35f;
			Z->bChasing = true;
			UGameplayStatics::FinishSpawningActor(Z, FTransform(FRotator(0, 180.f, 0), Loc));
		}
	}
}

// ---------------------------------------------------------------------------
// Screenshot tour: waits for shaders, then captures a few framed views and exits.
// ---------------------------------------------------------------------------
void AFOGameMode::TickShots(float Dt)
{
	if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) { ShotClock = 0.f; return; }
	ShotClock += Dt;
	struct FShot { FVector Loc; FRotator Rot; bool bMenu; };
	static const FShot Shots[] = {
		{ FVector(   0.f,    0.f, 100.f), FRotator(0,   0, 0), true  },  // menu overlay
		{ FVector( 300.f,    0.f, 100.f), FRotator(0,   0, 0), false },  // street ahead
		{ FVector(1500.f, -300.f, 100.f), FRotator(0,  20, 0), false },  // first fuel can area
		{ FVector(4300.f,  200.f, 100.f), FRotator(0, -25, 0), false },  // mid-street cars
		{ FVector(7500.f,    0.f, 100.f), FRotator(0,   0, 0), false },  // port gate
		{ FVector( 300.f,    0.f, 100.f), FRotator(0,   0, 0), false },  // diag 5: street, PP colour overrides off
		{ FVector( 300.f,    0.f, 100.f), FRotator(0,   0, 0), false },  // diag 6: street, PP exposure overrides off too
	};
	int32 Num = UE_ARRAY_COUNT(Shots);
	{ int32 Max = 0; if (FParse::Value(FCommandLine::Get(), TEXT("FOShotMax="), Max) && Max > 0) Num = FMath::Min(Num, Max); }
	// Give each view ~6 s to stream/settle before capturing.
	if (ShotClock < 6.f) return;
	if (ShotIndex >= Num) { FPlatformMisc::RequestExit(false); return; }
	const FShot& S = Shots[ShotIndex];
	if (!S.bMenu && State == EFOState::Menu) StartGame();
	if (AFOCharacter* P = Player())
	{
		if (ShotIndex > 0)
		{
			P->SetActorLocation(S.Loc, false, nullptr, ETeleportType::TeleportPhysics);
			if (APlayerController* PC = Cast<APlayerController>(P->GetController())) PC->SetControlRotation(S.Rot);
		}
	}
	// Optional per-shot diagnostics (-FOShotDiag): 1 baseline, 2 fog off, 3 +post-process off, 4 +bright moon.
	static bool bArmed = false;
	if (!bArmed && FParse::Param(FCommandLine::Get(), TEXT("FOShotDiag")) && GEngine)
	{
		UWorld* W = GetWorld();
		if (ShotIndex == 2) GEngine->Exec(W, TEXT("r.Fog 0"));
		if (ShotIndex == 3) GEngine->Exec(W, TEXT("showflag.postprocessing 0"));
		if (ShotIndex == 4)
		{
			GEngine->Exec(W, TEXT("showflag.postprocessing 1"));
			for (TActorIterator<AFOWorldBuilder> It(W); It; ++It)
				if (It->Moon) { It->Moon->GetLightComponent()->SetIntensity(28.f); It->LightningT = 1e9f; }
		}
		if (ShotIndex == 5 || ShotIndex == 6)
		{
			for (TActorIterator<AFOWorldBuilder> It(W); It; ++It)
				if (It->Moon) It->Moon->GetLightComponent()->SetIntensity(6.0f);
			for (TActorIterator<APostProcessVolume> It(W); It; ++It)
			{
				FPostProcessSettings& PS = It->Settings;
				PS.bOverride_SceneColorTint = false; PS.bOverride_ColorGamma = false; PS.bOverride_FilmToe = false;
				if (ShotIndex == 6) { PS.bOverride_AutoExposureMethod = false; PS.bOverride_AutoExposureBias = false; }
			}
		}
		UE_LOG(LogFO, Display, TEXT("Shot diag variant %d applied"), ShotIndex);
	}
	// Capture on the following frame so the teleport is visible; simple approach: alternate frames.
	if (!bArmed) { bArmed = true; return; }
	bArmed = false;
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Shots");
	IFileManager::Get().MakeDirectory(*Dir, true);
	FScreenshotRequest::RequestScreenshot(Dir / FString::Printf(TEXT("shot%02d.png"), ShotIndex), true, false);
	UE_LOG(LogFO, Display, TEXT("Shot %d requested"), ShotIndex);
	ShotIndex++;
	ShotClock = 0.f;
}
