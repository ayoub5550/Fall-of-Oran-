// Synthetic integration checks for the challenge modes (-FOChallengeTest).
// These are EXPLICIT SYNTHETIC TESTS of the rule engine, save isolation and wave progression.
// They are NOT a playthrough, not an input test and not a graphics/device validation.
#include "FOGameMode.h"
#include "FOCharacter.h"
#include "FOZombie.h"
#include "FallOfOran.h"
#include "Components/FOHealthComponent.h"
#include "Core/FOGameInstance.h"
#include "Core/FOLevelRegistry.h"
#include "Core/FOChallengeRegistry.h"
#include "Mission/FOChallengeComponent.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"

namespace
{
	FFOChallengeDef MakeSurvivalTestDef()
	{
		FFOChallengeDef D;
		D.Id = TEXT("survival_five");           // reuse a registry id so save records stay valid
		D.Mode = EFOFlowMode::Survival;
		D.Seed = 99001;
		D.StreetLength = 3000.f;
		D.SafeSpawnRadius = 1100.f;
		D.ResupplyPoints = { FVector(1200, -300, 45), FVector(1400, 300, 45) };
		D.FirstWaveDelay = 1.f;
		FFOSurvivalWave W1; W1.Enemies = 2; W1.MaxLive = 1; W1.SpawnInterval = 0.2f; W1.RestSeconds = 3.f;
		FFOSurvivalWave W2; W2.Enemies = 3; W2.MaxLive = 2; W2.SpawnInterval = 0.2f; W2.RestSeconds = 0.f; W2.RunnerChance = 1.f;
		D.Waves = { W1, W2 };
		return D;
	}

	FFOChallengeDef MakeSupplyTestDef()
	{
		FFOChallengeDef D;
		D.Id = TEXT("supply_run");
		D.Mode = EFOFlowMode::SupplyRun;
		D.Seed = 99002;
		D.StreetLength = 4000.f;
		D.SafeSpawnRadius = 1200.f;
		D.SupplyPoints = { FVector(900, -300, 55), FVector(2600, 300, 55) };
		D.SupplyTarget = 2;
		D.TimeLimitSeconds = 30.f;
		D.AmbientLive = 2;
		D.AmbientRespawnSeconds = 1.f;
		D.ResupplyPoints = { FVector(1500, 200, 45) };
		return D;
	}
}

void AFOGameMode::RunChallengeValidation()
{
	bool bPass = true;
	auto Check = [&](bool bOK, const TCHAR* Name)
	{
		if (!bOK) bPass = false;
		UE_LOG(LogFO, Display, TEXT("CHALLENGE CHECK %s: %s"), Name, bOK ? TEXT("PASS") : TEXT("FAIL"));
	};
	UWorld* W = GetWorld();
	UFOGameInstance* GI = UFOGameInstance::Get(this);
	if (!W || !GI)
	{
		UE_LOG(LogFO, Error, TEXT("CHALLENGE FAIL: no world / game instance"));
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
	AFOCharacter* P = Player();
	if (P && P->HealthComp) P->HealthComp->bInvulnerable = true;
	const FVector PlayerLoc = P ? P->GetActorLocation() : FVector(0, 0, 100.f);
	auto ClearWorldZombies = [&]() { for (TActorIterator<AFOZombie> It(W); It; ++It) It->Destroy(); };
	auto CountZombies = [&]() { int32 N = 0; for (TActorIterator<AFOZombie> It(W); It; ++It) if (!It->bDying) N++; return N; };
	// Real lethal path: Die() sets bDying (what the component observes). Distinct from Destroy(),
	// which is a non-combat disappearance and must never count as a kill.
	auto KillAllZombies = [&]() { for (TActorIterator<AFOZombie> It(W); It; ++It) if (!It->bDying) It->Die(nullptr); };
	auto DestroyAllZombies = [&]() { for (TActorIterator<AFOZombie> It(W); It; ++It) It->Destroy(); };
	auto NewChallenge = [&]() -> UFOChallengeComponent*
	{
		UFOChallengeComponent* C = NewObject<UFOChallengeComponent>(this);
		AddInstanceComponent(C);
		C->RegisterComponent();
		return C;
	};

	// ---------------------------------------------------------------- 1. definitions
	{
		Check(FFOChallengeRegistry::Num() == 2, TEXT("two challenge modes registered"));
		const FFOChallengeDef* S = FFOChallengeRegistry::FindByMode(EFOFlowMode::Survival);
		const FFOChallengeDef* R = FFOChallengeRegistry::FindByMode(EFOFlowMode::SupplyRun);
		Check(S && S->IsValid() && S->Waves.Num() == 5, TEXT("survival is a valid bounded 5-wave definition"));
		bool bEscalates = S != nullptr;
		if (S)
			for (int32 i = 1; i < S->Waves.Num(); i++)
				bEscalates &= S->Waves[i].Enemies > S->Waves[i - 1].Enemies
					&& S->Waves[i].MaxLive >= S->Waves[i - 1].MaxLive
					&& S->Waves[i].MaxLive <= S->Waves[i].Enemies;
		Check(bEscalates, TEXT("survival waves escalate with a live cap below the wave size"));
		bool bRest = S != nullptr;
		if (S) for (int32 i = 0; i + 1 < S->Waves.Num(); i++) bRest &= S->Waves[i].RestSeconds > 0.f;
		Check(bRest, TEXT("every non-final wave has a resupply window"));
		Check(R && R->IsValid() && R->SupplyTarget >= 4 && R->SupplyPoints.Num() >= R->SupplyTarget && R->TimeLimitSeconds > 60.f,
			TEXT("supply run has a timed collect target with enough crates"));
		bool bNoFuelTag = true;
		if (R)
		{
			const FFOLevelDef LD = FFOChallengeRegistry::MakeLevelDef(*R);
			int32 Supplies = 0;
			for (const FFOItemSpawn& It : LD.Items)
			{
				if (It.Tag == TEXT("fuel")) bNoFuelTag = false;
				if (It.Item == EFOItem::Supply && It.Tag == TEXT("supply")) Supplies++;
			}
			Check(Supplies == R->SupplyTarget, TEXT("arena spawns exactly the tagged supply crates"));
			Check(LD.bChallengeArena && LD.Stages.Num() == 0 && LD.ZombieCount == 0,
				TEXT("challenge level def is a compact arena with no campaign stages"));
			Check(LD.StreetLength <= 4500.f, TEXT("compact layout (no 90 m empty journey)"));
		}
		Check(bNoFuelTag, TEXT("no supply item reuses the campaign fuel ambush tag"));
		Check(FFOLevelRegistry::Num() == 3, TEXT("campaign registry untouched (3 levels)"));
	}

	// ---------------------------------------------------------------- 2. invalid definition never completes
	{
		UFOChallengeComponent* C = NewChallenge();
		FFOChallengeDef Empty; Empty.Id = TEXT("survival_five"); Empty.Mode = EFOFlowMode::Survival; Empty.Waves.Empty();
		C->Start(Empty);
		Check(C->IsFailed() && !C->IsCleared(), TEXT("empty wave list fails instead of instantly completing"));
		C->Advance(120.f);
		Check(!C->IsCleared(), TEXT("failed challenge cannot later self-complete"));
		C->DestroyComponent();
	}

	// ---------------------------------------------------------------- 3. survival wave progression
	int32 SpawnedTotal = 0;
	{
		ClearWorldZombies();
		State = EFOState::Playing;
		UFOChallengeComponent* C = NewChallenge();
		bool bCleared = false; FString FailReason;
		C->OnCleared.AddLambda([&bCleared]() { bCleared = true; });
		C->OnFailed.AddLambda([&FailReason](const FString& R) { FailReason = R; });
		const FFOChallengeDef Def = MakeSurvivalTestDef();
		C->Start(Def);
		Check(C->Phase() == EFOChallengePhase::Countdown && !C->IsCleared(), TEXT("survival starts in the pre-wave countdown"));
		const int32 DropsAfterStart = C->ResupplyDrops();
		Check(DropsAfterStart == 1, TEXT("resupply dropped before the first wave"));

		C->Advance(1.1f);
		Check(C->Phase() == EFOChallengePhase::WaveActive && C->WaveNumber() == 1, TEXT("countdown enters wave 1"));

		// Spoofed kill events must not advance anything.
		for (int32 i = 0; i < 50; i++) C->HandleEvent(FFOGameEvent(EFOGameEvent::ZombieKilled));
		Check(C->WavesCleared() == 0 && C->Phase() == EFOChallengePhase::WaveActive,
			TEXT("spoofed ZombieKilled events cannot progress a wave"));

		// Live cap: simulate long enough that all wave enemies would spawn if uncapped.
		bool bCapHeld = true, bFairSpawn = true;
		for (int32 i = 0; i < 40; i++)
		{
			C->Advance(0.25f);
			bCapHeld &= C->LiveEnemies() <= 1;               // wave 1 MaxLive
		}
		for (const FVector& S : C->SpawnLog())
			bFairSpawn &= FVector::Dist2D(S, PlayerLoc) >= Def.SafeSpawnRadius - 1.f
				&& S.X >= 0.f && S.X <= Def.StreetLength && FMath::Abs(S.Y) <= 500.f;
		Check(bCapHeld, TEXT("live enemies never exceed the wave cap"));
		Check(bFairSpawn && C->SpawnLog().Num() > 0, TEXT("spawns stay inside the arena and outside the safe radius"));
		Check(C->WavesCleared() == 0, TEXT("wave does not clear while enemies are alive"));

		// Non-combat disappearance: budget is restored, no kill is credited.
		const int32 DeadBefore = C->DeadThisWave();
		DestroyAllZombies();
		C->Advance(0.1f);
		Check(C->DeadThisWave() == DeadBefore && C->LostEnemyCount() > 0,
			TEXT("externally destroyed enemies are not counted as kills"));
		C->Advance(0.6f);
		Check(C->WavesCleared() == 0, TEXT("lost enemies do not clear the wave"));

		// Now actually kill them: progression must follow real deaths.
		for (int32 Round = 0; Round < 8 && C->WavesCleared() == 0; Round++)
		{
			KillAllZombies();
			C->Advance(0.3f);
		}
		Check(C->WavesCleared() == 1 && C->Phase() == EFOChallengePhase::WaveRest,
			TEXT("wave clears on actual dead count and enters the resupply window"));
		Check(C->ResupplyDrops() == DropsAfterStart + 1, TEXT("inter-wave resupply drop happens once"));
		Check(CountZombies() == 0, TEXT("inter-wave reset leaves no live enemies"));

		C->Advance(3.2f);
		Check(C->Phase() == EFOChallengePhase::WaveActive && C->WaveNumber() == 2, TEXT("rest window ends into the final wave"));
		for (int32 Round = 0; Round < 40 && !bCleared; Round++)
		{
			C->Advance(0.3f);
			KillAllZombies();
		}
		SpawnedTotal = C->SpawnLog().Num();
		Check(bCleared && C->IsCleared(), TEXT("clearing the final wave wins the challenge"));
		Check(SpawnedTotal >= 5, TEXT("all bounded wave enemies were really spawned"));
		Check(C->DeadThisWave() >= 3, TEXT("final wave cleared on confirmed deaths"));
		Check(FailReason.IsEmpty(), TEXT("victory path fires no failure"));
		Check(CountZombies() == 0, TEXT("victory tears down remaining enemies"));

		// Retry: Start() again fully resets the run.
		C->Start(Def);
		Check(C->Phase() == EFOChallengePhase::Countdown && C->WavesCleared() == 0 && C->SpawnLog().Num() == 0 && C->DeadThisWave() == 0,
			TEXT("retry resets waves, kills and spawn history"));
		C->ClearSpawnedEnemies();
		C->DestroyComponent();
		ClearWorldZombies();
	}

	// ---------------------------------------------------------------- 4. survival failure path
	{
		UFOChallengeComponent* C = NewChallenge();
		FString Reason;
		C->OnFailed.AddLambda([&Reason](const FString& R) { Reason = R; });
		C->Start(MakeSurvivalTestDef());
		C->Advance(1.1f);
		C->NotifyPlayerDied();
		Check(C->IsFailed() && !C->IsCleared() && !Reason.IsEmpty(), TEXT("player death fails the survival run"));
		Check(CountZombies() == 0, TEXT("failure clears spawned enemies"));
		C->DestroyComponent();
		ClearWorldZombies();
	}

	// ---------------------------------------------------------------- 5. supply run rules
	// NOTE: extraction is exercised through the ExitReached event that AFOWorldBuilder's exit trigger
	// emits, i.e. the same code path, but this is a SYNTHETIC rule test, not a physical overlap /
	// input test. AFOGameMode::IsExitUsable() gates the real overlap; verify that on device.
	{
		UFOChallengeComponent* C = NewChallenge();
		bool bCleared = false;
		C->OnCleared.AddLambda([&bCleared]() { bCleared = true; });
		const FFOChallengeDef Def = MakeSupplyTestDef();
		C->Start(Def);
		Check(C->Phase() == EFOChallengePhase::RunActive && !C->IsExtractionOpen(), TEXT("supply run starts with extraction locked"));
		C->HandleEvent(FFOGameEvent(EFOGameEvent::ExitReached));
		Check(!bCleared && C->Phase() == EFOChallengePhase::RunActive, TEXT("reaching extraction early does not complete the run"));
		C->HandleEvent(FFOGameEvent(EFOGameEvent::ItemCollected, TEXT("supply")));
		Check(C->SuppliesHeld() == 1 && !C->IsExtractionOpen(), TEXT("partial collection keeps extraction locked"));
		C->HandleEvent(FFOGameEvent(EFOGameEvent::ItemCollected, TEXT("fuel")));
		Check(C->SuppliesHeld() == 1, TEXT("campaign fuel tag does not count as a supply crate"));
		C->HandleEvent(FFOGameEvent(EFOGameEvent::ItemCollected, TEXT("supply")));
		Check(C->SuppliesHeld() == Def.SupplyTarget && C->IsExtractionOpen(), TEXT("full collection opens extraction"));
		Check(!bCleared, TEXT("collecting everything alone does not win"));
		const float Left = C->TimeRemaining();
		C->HandleEvent(FFOGameEvent(EFOGameEvent::ExitReached));
		Check(bCleared && C->IsCleared() && Left > 0.f, TEXT("reaching extraction with all supplies wins before the deadline"));
		bool bFair = true;
		for (const FVector& S : C->SpawnLog()) bFair &= FVector::Dist2D(S, PlayerLoc) >= Def.SafeSpawnRadius - 1.f;
		Check(bFair, TEXT("ambient roamers respect the fair spawn radius"));
		C->DestroyComponent();
		ClearWorldZombies();
	}

	// ---------------------------------------------------------------- 6. supply run deadline
	{
		UFOChallengeComponent* C = NewChallenge();
		bool bCleared = false; FString Reason;
		C->OnCleared.AddLambda([&bCleared]() { bCleared = true; });
		C->OnFailed.AddLambda([&Reason](const FString& R) { Reason = R; });
		const FFOChallengeDef Def = MakeSupplyTestDef();
		C->Start(Def);
		bool bCapHeld = true;
		for (int32 i = 0; i < 20; i++) { C->Advance(1.f); bCapHeld &= C->LiveEnemies() <= Def.AmbientLive; }
		Check(bCapHeld, TEXT("ambient roamers stay under the live cap"));
		C->HandleEvent(FFOGameEvent(EFOGameEvent::ItemCollected, TEXT("supply")));
		C->HandleEvent(FFOGameEvent(EFOGameEvent::ItemCollected, TEXT("supply")));
		Check(C->IsExtractionOpen(), TEXT("supplies collected before the deadline"));
		C->Advance(20.f);
		Check(C->IsFailed() && !bCleared && !Reason.IsEmpty(), TEXT("deadline expiry fails even holding every supply"));
		Check(C->TimeRemaining() <= 0.f, TEXT("clock stops at zero"));
		C->DestroyComponent();
		ClearWorldZombies();
	}

	// ---------------------------------------------------------------- 7. save isolation (campaign untouched)
	{
		const FFOProgress Before = GI->Progress;
		const int32 AttemptsBefore = GI->ChallengeRecord(TEXT("survival_five")) ? GI->ChallengeRecord(TEXT("survival_five"))->Attempts : 0;
		GI->NoteChallengeAttempt(TEXT("survival_five"));
		GI->OnChallengeFinished(TEXT("survival_five"), true, 5, 33, 240.f, 0.f);
		GI->OnChallengeFinished(TEXT("supply_run"), false, 0, 7, 0.f, 0.f);
		const FFOProgress After = GI->Progress;
		Check(After.UnlockedLevels == Before.UnlockedLevels && After.SelectedLevel == Before.SelectedLevel,
			TEXT("challenge results never unlock or move campaign levels"));
		bool bBestsUntouched = After.BestKills.Num() == Before.BestKills.Num() && After.BestTimeSeconds.Num() == Before.BestTimeSeconds.Num();
		for (int32 i = 0; i < After.BestKills.Num() && bBestsUntouched; i++) bBestsUntouched &= After.BestKills[i] == Before.BestKills[i];
		Check(bBestsUntouched, TEXT("campaign best scores unchanged by challenge results"));
		const FFOChallengeRecord* Rec = GI->ChallengeRecord(TEXT("survival_five"));
		Check(Rec && Rec->bCleared && Rec->BestWave == 5 && Rec->BestKills >= 33 && Rec->Attempts == AttemptsBefore + 1,
			TEXT("challenge personal best recorded in its own slot"));
		const FFOChallengeRecord* Fail = GI->ChallengeRecord(TEXT("supply_run"));
		Check(Fail && !Fail->bCleared && Fail->BestClearSeconds == 0.f, TEXT("failed run records no clear time"));
		Check(UFOGameInstance::ChallengeSlotName != UFOGameInstance::SlotName
			&& FString(UFOGameInstance::ChallengeSlotName) != FString(UFOGameInstance::SlotName),
			TEXT("challenge data uses a separate save slot"));

		// Reload from disk: records survive and stay clamped; campaign load path is independent.
		GI->LoadChallenges();
		const FFOChallengeRecord* Reloaded = GI->ChallengeRecord(TEXT("survival_five"));
		Check(Reloaded && Reloaded->bCleared && Reloaded->BestWave <= 5, TEXT("challenge slot reloads clamped records"));
		GI->Load();
		Check(GI->Progress.UnlockedLevels == Before.UnlockedLevels, TEXT("campaign reload unaffected"));
	}

	// ---------------------------------------------------------------- 8. mode selection semantics
	{
		const EFOFlowMode Original = GI->FlowMode();
		GI->SetFlowMode(EFOFlowMode::Campaign);
		GI->CycleFlowMode(1);
		Check(GI->FlowMode() == EFOFlowMode::Survival, TEXT("cycling from campaign selects survival"));
		GI->CycleFlowMode(1);
		Check(GI->FlowMode() == EFOFlowMode::SupplyRun, TEXT("cycling again selects supply run"));
		GI->CycleFlowMode(1);
		Check(GI->FlowMode() == EFOFlowMode::Campaign, TEXT("cycling wraps back to campaign"));
		GI->CycleFlowMode(-1);
		Check(GI->FlowMode() == EFOFlowMode::SupplyRun && GI->CurrentChallenge() != nullptr, TEXT("backwards cycling works and resolves a definition"));
		GI->SetFlowMode(EFOFlowMode::Campaign);
		Check(GI->CurrentChallenge() == nullptr, TEXT("campaign mode exposes no challenge definition"));
		GI->SetFlowMode(Original);
	}

	// The live GameMode component must stay idle unless a challenge was started.
	Check(Challenge != nullptr && (Flow != EFOFlowMode::Campaign || Challenge->Phase() == EFOChallengePhase::Idle),
		TEXT("campaign flow leaves the challenge component idle"));

	UE_LOG(LogFO, Display, TEXT("CHALLENGE %s: synthetic rule/save integration checks, not device/input validation"),
		bPass ? TEXT("PASS") : TEXT("FAIL"));
	FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
}
