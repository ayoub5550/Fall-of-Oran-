#include "FOGameMode.h"
#include "FallOfOran.h"
#include "FOCharacter.h"
#include "FOZombie.h"
#include "FOWorldBuilder.h"
#include "FOHud.h"
#include "Core/FOGameInstance.h"
#include "Core/FOLevelRegistry.h"
#include "Core/FOChallengeRegistry.h"
#include "Mission/FOMissionComponent.h"
#include "Mission/FOChallengeComponent.h"
#include "Puzzles/FOKeypadPuzzle.h"
#include "Mission/FOObjective.h"
#include "Components/FOHealthComponent.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "ShaderCompiler.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	// Process-local regression state; weak ownership must not keep the old HUD
	// alive across OpenLevel. Used only with -FOSelfTestRestart.
	TWeakPtr<SFOHud> GSelfTestPreviousHud;
	bool GSelfTestDidRestart = false;
}

AFOGameMode::AFOGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AFOCharacter::StaticClass();
	Mission = CreateDefaultSubobject<UFOMissionComponent>(TEXT("Mission"));
	Challenge = CreateDefaultSubobject<UFOChallengeComponent>(TEXT("Challenge"));
}

AFOCharacter* AFOGameMode::Player() const { return Cast<AFOCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)); }

void AFOGameMode::BeginPlay()
{
	Super::BeginPlay();
	UFOGameInstance* GI = UFOGameInstance::Get(this);
	Flow = GI ? GI->FlowMode() : EFOFlowMode::Campaign;
	const FFOChallengeDef* ChallengeDef = GI ? GI->CurrentChallenge() : nullptr;
	if (Flow != EFOFlowMode::Campaign && (!ChallengeDef || !ChallengeDef->IsValid()))
	{
		UE_LOG(LogFO, Warning, TEXT("GameMode: challenge mode %d unavailable, falling back to campaign"), (int32)Flow);
		Flow = EFOFlowMode::Campaign;
		ChallengeDef = nullptr;
		if (GI) GI->SetFlowMode(EFOFlowMode::Campaign);
	}
	if (ChallengeDef)
	{
		// Compact authored arena built from the challenge definition; the campaign registry is untouched.
		ChallengeLevel = FFOChallengeRegistry::MakeLevelDef(*ChallengeDef);
		Level = &ChallengeLevel;
		LevelIndex = GI ? GI->CurrentLevelIndex() : 0;   // kept for HUD/campaign selection, never written on a challenge win
		ChallengeId = ChallengeDef->Id;
	}
	else
	{
		Level = GI ? GI->CurrentLevel() : FFOLevelRegistry::Get(0);
		LevelIndex = GI ? GI->CurrentLevelIndex() : 0;
	}
	check(Level);
	UE_LOG(LogFO, Display, TEXT("GameMode: flow %d, level '%s'"), (int32)Flow, *Level->Title);

	// World first (puzzles/notes need the mission to exist but not to be started), then mission, then HUD.
	World = GetWorld()->SpawnActor<AFOWorldBuilder>(AFOWorldBuilder::StaticClass(), FTransform::Identity);
	if (World)
	{
		World->BuildWorld(*Level);
		World->ApplyBrightness(GI ? GI->Brightness() : 1.f);
	}
	if (IsChallenge() && ChallengeDef)
	{
		// The campaign mission machine stays idle in challenge modes (no stages, no unlocking).
		Challenge->OnHint.AddLambda([this](const FString& T) { SetHint(T, 4.f); });
		Challenge->OnCleared.AddLambda([this]() { Win(); });
		Challenge->OnFailed.AddLambda([this](const FString& R) { FailChallenge(R); });
	}
	else
	{
		Mission->OnHint.AddLambda([this](const FString& T) { SetHint(T, 5.f); });
		Mission->OnMissionComplete.AddLambda([this]() { Win(); });
		Mission->Start(*Level);
	}

	if (GEngine && GEngine->GameViewport)
	{
		Hud = SNew(SFOHud).GameMode(this);
		GEngine->GameViewport->AddViewportWidgetContent(Hud.ToSharedRef(), 10);
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		// Touch controls are Slate widgets, not gameplay key bindings. Give the UI
		// first refusal and leave unhandled touches available for camera look.
		PC->bShowMouseCursor = FSlateApplication::IsInitialized() && FSlateApplication::Get().IsFakingTouchEvents();
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		if (GEngine && GEngine->GameViewport)
			GEngine->GameViewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
	}
	State = EFOState::Menu;
	bShotMode = FParse::Param(FCommandLine::Get(), TEXT("FOShots"));
	bSelfTest = FParse::Param(FCommandLine::Get(), TEXT("FOSelfTest"));
	{ int32 Skip = 0; if (FParse::Value(FCommandLine::Get(), TEXT("FOShotSkip="), Skip)) ShotIndex = Skip; } // -FOShotSkip=3 renders only the puzzle view
	if (bShotMode) GAreScreenMessagesEnabled = false;
	if (GI && GI->bContinueIntoLevel)
	{
		GI->bContinueIntoLevel = false;
		StartGame();
	}
}

void AFOGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	// The viewport survives OpenLevel. Leaving this shared widget attached stacks
	// dead menu overlays after each retry/level change and darkens the next level.
	if (Hud.IsValid() && GEngine && GEngine->GameViewport)
		GEngine->GameViewport->RemoveViewportWidgetContent(Hud.ToSharedRef());
	Hud.Reset();
	CloseKeypad();
	Super::EndPlay(Reason);
}

void AFOGameMode::Tick(float Dt)
{
	Super::Tick(Dt);
	if (State == EFOState::Playing) LevelTime += Dt;
	if (DamageFlash > 0.f) DamageFlash -= Dt;
	if (HealFlash > 0.f) HealFlash -= Dt;
	if (HintTimer > 0.f) { HintTimer -= Dt; if (HintTimer <= 0.f) Hint.Empty(); }
	if (NoteTimer > 0.f) { NoteTimer -= Dt; if (NoteTimer <= 0.f) NoteText.Empty(); }
	if (RestartTimer > 0.f) RestartTimer -= Dt;
	if (FParse::Param(FCommandLine::Get(), TEXT("FOEncounterTest")))
	{
		RunEncounterValidation();
		return;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("FOChallengeTest")))
	{
		if (!bChallengeTestRan) { bChallengeTestRan = true; RunChallengeValidation(); }
		return;
	}
	if (IsChallenge() && State == EFOState::Playing && Challenge) Challenge->Advance(Dt);
	if (bShotMode) TickShots(Dt);
	if (bSelfTest) TickSelfTest(Dt);
}

// ------------------------------------------------------------------ flow
void AFOGameMode::StartGame()
{
	if (State == EFOState::Menu)
	{
		State = EFOState::Playing;
		if (!Level->Intro.IsEmpty()) SetHint(Level->Intro, 5.f);
		if (IsChallenge())
		{
			UFOGameInstance* GI = UFOGameInstance::Get(this);
			if (GI) GI->NoteChallengeAttempt(ChallengeId);
			const FFOChallengeDef* Def = GI ? GI->CurrentChallenge() : nullptr;
			if (!Def) { FailChallenge(TEXT("تعذّر تحميل التحدي")); return; }
			Challenge->Start(*Def);   // an invalid definition fails here; it can never instantly win
			return;
		}
		// Empty missions can complete during BeginPlay, before the player starts.
		if (Mission && Mission->IsComplete()) Win();
		return;
	}
	if (RestartTimer > 0.f) return;
	if (State == EFOState::Dead) { Restart(); return; }   // retry the same challenge / level
	if (State == EFOState::Won)
	{
		UFOGameInstance* GI = UFOGameInstance::Get(this);
		if (IsChallenge()) { Restart(); return; }          // challenges never advance the campaign
		if (GI && !bCampaignDone) GI->bContinueIntoLevel = GI->AdvanceToNextLevel();
		else if (GI) GI->SelectLevel(0);
		Restart();
	}
}

void AFOGameMode::Restart() { UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this))); }

void AFOGameMode::SelectRelativeLevel(int32 Delta)
{
	if (State != EFOState::Menu) return;
	UFOGameInstance* GI = UFOGameInstance::Get(this);
	if (!GI) return;
	const int32 Want = LevelIndex + Delta;
	if (!GI->IsUnlocked(Want)) { SetHint(TEXT("هذا المستوى مقفل — أنهِ المستوى السابق أولاً 🔒"), 3.f); return; }
	GI->SelectLevel(Want);
	Restart();
}

void AFOGameMode::SelectRelativeMode(int32 Delta)
{
	if (State != EFOState::Menu) return;
	UFOGameInstance* GI = UFOGameInstance::Get(this);
	if (!GI) return;
	const EFOFlowMode Before = GI->FlowMode();
	GI->CycleFlowMode(Delta);
	if (GI->FlowMode() == Before) { SetHint(TEXT("لا يوجد طور آخر متاح"), 3.f); return; }
	GI->bContinueIntoLevel = false;
	Restart();
}

void AFOGameMode::ReturnToMenu()
{
	// Reload the same map and stop at the menu (no campaign advance, no challenge auto-start).
	if (UFOGameInstance* GI = UFOGameInstance::Get(this)) GI->bContinueIntoLevel = false;
	Restart();
}

void AFOGameMode::FailChallenge(const FString& Reason)
{
	if (State != EFOState::Playing) return;
	State = EFOState::Dead;
	CloseKeypad();
	UFOGameInstance* GI = UFOGameInstance::Get(this);
	if (GI && IsChallenge())
		GI->OnChallengeFinished(ChallengeId, false, Challenge ? Challenge->WavesCleared() : 0, Kills, 0.f, 0.f);
	Subtitle = FString::Printf(TEXT("%s\nقتلت %d — المس الشاشة للمحاولة من جديد أو اختر «القائمة»"), *Reason, Kills);
	RestartTimer = 2.f;
}

FString AFOGameMode::GetChallengeStatusLine() const
{
	return (IsChallenge() && Challenge && (Challenge->IsActive() || Challenge->IsCleared())) ? Challenge->GetStatusLine() : FString();
}

FString AFOGameMode::GetModeName() const
{
	switch (Flow)
	{
	case EFOFlowMode::Survival:  return TEXT("طور الصمود");
	case EFOFlowMode::SupplyRun: return TEXT("طور خط الإمداد");
	default:                     return TEXT("الحملة");
	}
}

void AFOGameMode::Win()
{
	if (State != EFOState::Playing) return;
	State = EFOState::Won;
	UFOGameInstance* GI = UFOGameInstance::Get(this);
	const int32 M = (int32)LevelTime / 60, S = (int32)LevelTime % 60;
	if (IsChallenge())
	{
		// Challenge results live in their own save slot: no unlocking, no campaign writes.
		const float TimeLeft = Challenge ? Challenge->TimeRemaining() : 0.f;
		if (GI) GI->OnChallengeFinished(ChallengeId, true, Challenge ? Challenge->WavesCleared() : 0, Kills, LevelTime, TimeLeft);
		const FFOChallengeRecord* Rec = GI ? GI->ChallengeRecord(ChallengeId) : nullptr;
		FString Best;
		if (Rec) Best = FString::Printf(TEXT("\nأفضل نتيجة: %d قتيل — %.0f ث"), Rec->BestKills, Rec->BestClearSeconds);
		Subtitle = FString::Printf(TEXT("%s\nالقتلى: %d — الوقت %d:%02d%s\nالمس الشاشة لإعادة التحدي أو اختر «القائمة»"),
			*Level->OutroText, Kills, M, S, *Best);
		RestartTimer = 2.5f;
		return;
	}
	if (GI) GI->OnLevelCompleted(LevelIndex, Kills, LevelTime);
	bCampaignDone = LevelIndex + 1 >= FFOLevelRegistry::Num();
	Subtitle = FString::Printf(TEXT("%s\nالقتلى: %d — الوقت %d:%02d\n%s"), *Level->OutroText, Kills, M, S,
		bCampaignDone ? TEXT("أنهيت الحملة كاملة! المس الشاشة للعودة إلى البداية") : TEXT("المس الشاشة للمستوى التالي"));
	RestartTimer = 2.5f;
}

// ------------------------------------------------------------------ events
void AFOGameMode::ReportEvent(const FFOGameEvent& E)
{
	if (State != EFOState::Playing) return;
	switch (E.Type)
	{
	case EFOGameEvent::ZombieKilled:
		Kills += E.Count;
		if (AFOCharacter* P = Player()) P->Kills += E.Count;
		break;
	case EFOGameEvent::ItemCollected:
		// The clatter ambush belongs to the campaign fuel cans only. Challenge supply crates use the
		// "supply" tag on purpose so this rule can never be reused as a hidden challenge spawner.
		if (!IsChallenge() && E.Tag == TEXT("fuel")) SpawnZombiesBehindPlayer(2);
		break;
	case EFOGameEvent::ExitReached:
		if (!IsChallenge() && !Mission->IsExitOpen()) { SetHint(TEXT("البوابة مقفلة! أكمل الهدف الحالي أولاً 🔒"), 3.f); return; }
		// Challenge extraction rules (including the "locked" hint) live in the challenge component.
		break;
	default: break;
	}
	if (IsChallenge()) { if (Challenge) Challenge->HandleEvent(E); return; }
	Mission->HandleEvent(E);
}

void AFOGameMode::OnPlayerDamaged() { DamageFlash = 0.35f; }

void AFOGameMode::OnPlayerDied()
{
	if (State != EFOState::Playing) return;
	if (IsChallenge() && Challenge && Challenge->IsActive())
	{
		Challenge->NotifyPlayerDied();   // routes back through FailChallenge with the mode's reason text
		return;
	}
	State = EFOState::Dead;
	CloseKeypad();
	Subtitle = FString::Printf(TEXT("قتلت %d زومبي — المس الشاشة للمحاولة من جديد"), Kills);
	RestartTimer = 2.5f;
}

void AFOGameMode::OnPuzzleFailed(const FString& Message, int32 PenaltyZombies)
{
	SetHint(Message, 4.f);
	DamageFlash = 0.25f;
	SpawnZombiesBehindPlayer(PenaltyZombies);
}

bool AFOGameMode::IsExitUsable() const
{
	if (IsChallenge()) return Challenge && Challenge->IsExtractionOpen();
	return Mission && Mission->IsExitOpen();
}

bool AFOGameMode::IsPuzzleActive(FName Id) const { return Mission && Mission->IsPuzzleActive(Id); }

// ------------------------------------------------------------------ HUD helpers
void AFOGameMode::SetHint(const FString& Text, float Seconds) { Hint = Text; HintTimer = Seconds; }
void AFOGameMode::ShowNote(const FString& Text, float Seconds) { NoteText = Text; NoteTimer = Seconds; }
FString AFOGameMode::GetObjectiveText() const
{
	if (IsChallenge()) return Challenge ? Challenge->GetObjectiveText() : FString();
	return Mission ? Mission->GetObjectiveText() : FString();
}

void AFOGameMode::AdjustBrightness(float Step)
{
	UFOGameInstance* GI = UFOGameInstance::Get(this);
	const float B = FMath::Clamp((GI ? GI->Brightness() : 1.f) * Step, 0.25f, 4.f);
	if (GI) GI->SetBrightness(B);
	if (World) World->ApplyBrightness(B);
}
float AFOGameMode::CurrentBrightness() const { const UFOGameInstance* GI = UFOGameInstance::Get(this); return GI ? GI->Brightness() : 1.f; }

// ------------------------------------------------------------------ keypad
void AFOGameMode::OpenKeypad(AFOKeypadPuzzle* Pad) { ActiveKeypad = Pad; KeypadInput.Empty(); }
void AFOGameMode::CloseKeypad() { ActiveKeypad = nullptr; KeypadInput.Empty(); }
int32 AFOGameMode::KeypadLength() const { return ActiveKeypad ? ActiveKeypad->CodeLength() : 4; }
void AFOGameMode::KeypadPress(TCHAR Digit)
{
	if (!ActiveKeypad || KeypadInput.Len() >= KeypadLength()) return;
	KeypadInput.AppendChar(Digit);
	if (KeypadInput.Len() >= KeypadLength()) KeypadSubmit();
}
void AFOGameMode::KeypadBackspace() { if (KeypadInput.Len() > 0) KeypadInput.LeftChopInline(1); }
void AFOGameMode::KeypadSubmit()
{
	if (!ActiveKeypad) return;
	const bool bOk = ActiveKeypad->Submit(KeypadInput);
	if (bOk) CloseKeypad(); else KeypadInput.Empty();
}

// ------------------------------------------------------------------ spawning
void AFOGameMode::SpawnZombiesBehindPlayer(int32 Count)
{
	AFOCharacter* P = Player();
	if (!P || !World) return;
	for (int32 i = 0; i < Count; i++)
	{
		FVector Loc = P->GetActorLocation() + FVector(FMath::FRandRange(1400.f, 2200.f) * (FMath::FRand() < 0.7f ? 1.f : -1.f), FMath::FRandRange(-400.f, 400.f), 0.f);
		Loc.X = FMath::Clamp(Loc.X, -200.f, World->StreetLength - 200.f);
		Loc.Y = FMath::Clamp(Loc.Y, -500.f, 500.f);
		Loc.Z = 100.f;
		const FTransform T(FRotator(0, 180.f, 0), Loc);
		if (AFOZombie* Z = GetWorld()->SpawnActorDeferred<AFOZombie>(AFOZombie::StaticClass(), T))
		{
			Z->Variant = FMath::RandRange(0, 3);
			Z->bRunner = Z->Variant != 2 && FMath::FRand() < 0.35f;
			Z->bChasing = true;
			Z->HpMul = Level->ZombieHpMul;
			UGameplayStatics::FinishSpawningActor(Z, T);
		}
	}
}

// ------------------------------------------------------------------ self test
void AFOGameMode::TickSelfTest(float Dt)
{
	if (IsChallenge())
	{
		// -FOSelfTest drives campaign stages; challenge rules have their own check (-FOChallengeTest).
		bSelfTest = false;
		UE_LOG(LogFO, Display, TEXT("SELFTEST SKIP: challenge flow active, use -FOChallengeTest"));
		return;
	}
	SelfTestClock += Dt;
	if (SelfTestClock < 1.f) return;
	if (State == EFOState::Menu)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("FOSelfTestRestart")) && GSelfTestDidRestart)
		{
			if (GSelfTestPreviousHud.IsValid())
			{
				bSelfTest = false;
				UE_LOG(LogFO, Error, TEXT("SELFTEST FAIL: previous HUD retained after restart"));
				FPlatformMisc::RequestExitWithStatus(false, 1);
				return;
			}
			UE_LOG(LogFO, Display, TEXT("SELFTEST HUD CLEANUP PASS: previous widget released"));
		}
		StartGame();
		return;
	}
	if (State == EFOState::Won)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("FOSelfTestRestart")) && !GSelfTestDidRestart)
		{
			if (!Hud.IsValid())
			{
				bSelfTest = false;
				UE_LOG(LogFO, Error, TEXT("SELFTEST FAIL: HUD not created; cannot check restart cleanup"));
				FPlatformMisc::RequestExitWithStatus(false, 1);
				return;
			}
			GSelfTestPreviousHud = Hud;
			GSelfTestDidRestart = true;
			bSelfTest = false;
			UE_LOG(LogFO, Display, TEXT("SELFTEST RESTART: reloading completed level"));
			Restart();
			return;
		}
		bSelfTest = false;
		UE_LOG(LogFO, Display, TEXT("SELFTEST PASS: level %d won, kills %d"), LevelIndex + 1, Kills);
		FPlatformMisc::RequestExitWithStatus(false, 0);
		return;
	}
	if (State != EFOState::Playing || SelfTestClock > 30.f)
	{
		bSelfTest = false;
		UE_LOG(LogFO, Error, TEXT("SELFTEST FAIL: state %d after %.0fs, stage %d"), (int32)State, SelfTestClock, Mission ? Mission->StageIndex() : -1);
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
	// Drive one step per tick: satisfy every active objective of the current stage.
	const int32 StageBefore = Mission ? Mission->StageIndex() : -1;
	if (!Level || !Level->Stages.IsValidIndex(StageBefore))
	{
		bSelfTest = false;
		UE_LOG(LogFO, Error, TEXT("SELFTEST FAIL: invalid mission stage %d"), StageBefore);
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
	for (const FFOObjectiveDef& O : Level->Stages[StageBefore].Objectives)
	{
		switch (O.Type)
		{
		case EFOObjectiveType::Collect: for (int32 i = 0; i < O.Count; i++) ReportEvent(FFOGameEvent(EFOGameEvent::ItemCollected, O.Tag)); break;
		case EFOObjectiveType::Kill:    for (int32 i = 0; i < O.Count; i++) ReportEvent(FFOGameEvent(EFOGameEvent::ZombieKilled)); break;
		case EFOObjectiveType::Reach:
			// Exercise the actual overlap path, not a synthetic ExitReached event.
			if (World && World->ExitTrigger && Player())
			{
				if (!bSelfTestExitPrepared)
				{
					bSelfTestExitPrepared = true;
					Player()->SetActorLocation(World->ExitTrigger->GetComponentLocation(), false, nullptr, ETeleportType::TeleportPhysics);
					UE_LOG(LogFO, Display, TEXT("SELFTEST EXIT OVERLAP: entered open exit"));
				}
			}
			else
			{
				bSelfTest = false;
				UE_LOG(LogFO, Error, TEXT("SELFTEST FAIL: player or exit trigger missing"));
				FPlatformMisc::RequestExitWithStatus(false, 1);
			}
			break;
		case EFOObjectiveType::SolvePuzzle:
			// Regression: solving the gate while already inside its trigger must win
			// without requiring the player to step out and re-enter the volume.
			if (!bSelfTestExitPrepared && World && World->ExitTrigger && Player())
			{
				bSelfTestExitPrepared = true;
				Player()->SetActorLocation(World->ExitTrigger->GetComponentLocation(), false, nullptr, ETeleportType::TeleportPhysics);
				UE_LOG(LogFO, Display, TEXT("SELFTEST EXIT OVERLAP: entered locked exit before puzzle"));
			}
			for (TActorIterator<AFOPuzzleBase> It(GetWorld()); It; ++It) if (It->GetId() == O.Tag) It->DebugSolve();
			break;
		}
		if (Mission->IsComplete() || Mission->StageIndex() != StageBefore) break;
	}
	UE_LOG(LogFO, Display, TEXT("SELFTEST: stage %d -> %d, complete=%d, hint='%s'"), StageBefore + 1, Mission->StageIndex() + 1, Mission->IsComplete(), *Hint);
}

// ------------------------------------------------------------------ screenshot tour
void AFOGameMode::TickShots(float Dt)
{
	if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) { ShotClock = 0.f; return; }
	ShotClock += Dt;
	const float L = World ? World->StreetLength : 9000.f;
	struct FShot { FVector Loc; FRotator Rot; bool bMenu; };
	TArray<FShot> Shots = {
		{ FVector(0.f, 0.f, 100.f), FRotator(0, 0, 0), true },        // menu overlay
		{ FVector(300.f, 0.f, 100.f), FRotator(0, 0, 0), false },     // street ahead
		{ FVector(L * 0.5f, 200.f, 100.f), FRotator(0, -25, 0), false }, // mid-street
	};
	// Puzzle close-up if the level has one: stand 350 cm in front of it, looking at it.
	if (Level && Level->Puzzles.Num() > 0)
	{
		const FFOPuzzleDef& P = Level->Puzzles[0];
		const FVector Face = FRotator(0, P.Yaw, 0).Vector() * -1.f; // puzzle front (-X local)
		const FVector Side = FVector::CrossProduct(Face, FVector::UpVector);
		const FVector ViewPos = P.Location + Face * 420.f + Side * 180.f + FVector(0, 0, 100.f);
		Shots.Add({ ViewPos, (P.Location + FVector(0, 0, 40.f) - ViewPos).Rotation(), false });
	}
	int32 Num = Shots.Num();
	{ int32 Max = 0; if (FParse::Value(FCommandLine::Get(), TEXT("FOShotMax="), Max) && Max > 0) Num = FMath::Min(Num, Max); }
	if (ShotClock < 6.f) return;
	if (ShotIndex >= Num) { FPlatformMisc::RequestExit(false); return; }
	const FShot& S = Shots[ShotIndex];
	if (!S.bMenu && State == EFOState::Menu) StartGame();
	if (AFOCharacter* P0 = Player()) if (P0->HealthComp) P0->HealthComp->bInvulnerable = true; // proof shots without damage flash
	if (AFOCharacter* P = Player())
		if (ShotIndex > 0)
		{
			P->SetActorLocation(S.Loc, false, nullptr, ETeleportType::TeleportPhysics);
			if (APlayerController* PC = Cast<APlayerController>(P->GetController())) PC->SetControlRotation(S.Rot);
		}
	static bool bArmed = false;
	if (!bArmed) { bArmed = true; return; } // capture on the following frame so the teleport is visible
	bArmed = false;
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Shots");
	IFileManager::Get().MakeDirectory(*Dir, true);
	FScreenshotRequest::RequestScreenshot(Dir / FString::Printf(TEXT("shot%02d.png"), ShotIndex), true, false);
	UE_LOG(LogFO, Display, TEXT("Shot %d requested"), ShotIndex);
	ShotIndex++;
	ShotClock = 0.f;
}
