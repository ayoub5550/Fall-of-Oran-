#include "Mission/FOChallengeComponent.h"
#include "FOGameMode.h"
#include "FOCharacter.h"
#include "FOZombie.h"
#include "FOPickup.h"
#include "FallOfOran.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// ------------------------------------------------------------------ lifecycle
void UFOChallengeComponent::Start(const FFOChallengeDef& InDef)
{
	Def = InDef;
	CurrentPhase = EFOChallengePhase::Idle;
	Reason.Empty();
	WaveIndex = 0; ClearedWaves = 0; SpawnedThisWave = 0; DeadCounted = 0;
	SpawnClock = 0.f; PhaseClock = 0.f; Elapsed = 0.f; ResupplyCount = 0;
	SupplyCollected = 0; Clock = 0.f; AmbientClock = 0.f; LostEnemies = 0;
	// Retry in the same world: destroy anything a previous run left behind before forgetting it.
	ClearSpawnedEnemies();
	for (const TWeakObjectPtr<AFOPickup>& Old : DroppedPickups)
		if (AFOPickup* Pick = Old.Get()) Pick->Destroy();
	DroppedPickups.Empty();
	SpawnLocations.Empty();
	Rng.Initialize(Def.Seed ^ 0x5C4A11);

	if (!Def.IsValid())
	{
		// A malformed or empty challenge must never read as "instantly complete".
		Fail(TEXT("تعريف التحدي غير صالح — لا يمكن اللعب"));
		UE_LOG(LogFO, Error, TEXT("Challenge '%s' definition invalid; refusing to run"), *Def.Id.ToString());
		return;
	}

	if (Def.Mode == EFOFlowMode::Survival)
	{
		EnterCountdown();
	}
	else
	{
		Clock = Def.TimeLimitSeconds;
		AmbientClock = 0.f;
		CurrentPhase = EFOChallengePhase::RunActive;
		OnHint.Broadcast(FString::Printf(TEXT("اجمع %d صناديق إمداد ثم اصل إلى نقطة الإخلاء قبل نهاية الوقت"), Def.SupplyTarget));
	}
	UE_LOG(LogFO, Display, TEXT("Challenge '%s' started (mode %d, %d waves, %d supplies)"),
		*Def.Id.ToString(), (int32)Def.Mode, Def.Waves.Num(), Def.SupplyTarget);
}

void UFOChallengeComponent::EnterCountdown()
{
	CurrentPhase = EFOChallengePhase::Countdown;
	PhaseClock = FMath::Max(0.5f, Def.FirstWaveDelay);
	OnHint.Broadcast(TEXT("تحصّن في الساحة — الموجة الأولى قادمة"));
	DropResupply();
}

void UFOChallengeComponent::Advance(float Dt)
{
	if (!IsActive()) return;
	Dt = FMath::Max(0.f, Dt);
	Elapsed += Dt;
	PruneEnemies();
	if (Def.Mode == EFOFlowMode::Survival) TickSurvival(Dt);
	else TickSupplyRun(Dt);
}

// ------------------------------------------------------------------ survival
void UFOChallengeComponent::TickSurvival(float Dt)
{
	if (CurrentPhase == EFOChallengePhase::Countdown || CurrentPhase == EFOChallengePhase::WaveRest)
	{
		PhaseClock -= Dt;
		if (PhaseClock > 0.f) return;
		const int32 Next = (CurrentPhase == EFOChallengePhase::Countdown) ? 0 : WaveIndex + 1;
		if (!Def.Waves.IsValidIndex(Next)) { FinishCleared(); return; }
		BeginWave(Next);
		return;
	}
	if (CurrentPhase != EFOChallengePhase::WaveActive) return;
	const FFOSurvivalWave& W = Def.Waves[WaveIndex];

	// Spawn trickle, respecting the live cap.
	if (SpawnedThisWave < W.Enemies)
	{
		SpawnClock -= Dt;
		if (SpawnClock <= 0.f)
		{
			if (LiveEnemies() < W.MaxLive)
			{
				SpawnEnemy(W.RunnerChance, W.HeavyChance, W.HpMul, true);
				SpawnClock = FMath::Max(0.15f, W.SpawnInterval);
			}
			else
			{
				SpawnClock = 0.5f; // wait for a slot instead of re-searching every frame
			}
		}
	}

	// Wave clears only when every enemy of the wave has been spawned AND actually died.
	if (SpawnedThisWave >= W.Enemies && DeadCounted >= W.Enemies && LiveEnemies() == 0)
	{
		ClearedWaves = WaveIndex + 1;
		UE_LOG(LogFO, Display, TEXT("Challenge wave %d/%d cleared (%d confirmed dead)"), ClearedWaves, Def.Waves.Num(), DeadCounted);
		if (ClearedWaves >= Def.Waves.Num()) { FinishCleared(); return; }
		EnterRest();
	}
}

void UFOChallengeComponent::BeginWave(int32 Index)
{
	WaveIndex = Index;
	SpawnedThisWave = 0;
	DeadCounted = 0;
	WaveEnemies.Empty();
	SpawnClock = 0.f;
	CurrentPhase = EFOChallengePhase::WaveActive;
	const FFOSurvivalWave& W = Def.Waves[WaveIndex];
	OnHint.Broadcast(W.Banner.IsEmpty() ? FString::Printf(TEXT("الموجة %d/%d"), WaveIndex + 1, Def.Waves.Num()) : W.Banner);
	UE_LOG(LogFO, Display, TEXT("Challenge wave %d/%d begins: %d enemies, max live %d"), WaveIndex + 1, Def.Waves.Num(), W.Enemies, W.MaxLive);
}

void UFOChallengeComponent::EnterRest()
{
	CurrentPhase = EFOChallengePhase::WaveRest;
	const float Rest = Def.Waves.IsValidIndex(WaveIndex) ? Def.Waves[WaveIndex].RestSeconds : 10.f;
	PhaseClock = FMath::Max(3.f, Rest);
	// Explicit reset: no leftovers from the cleared wave, fresh supplies, full magazine reserve top-up.
	ClearSpawnedEnemies();
	DropResupply();
	if (AFOGameMode* GM = Cast<AFOGameMode>(GetOwner()))
		if (AFOCharacter* P = GM->Player()) P->AddAmmo(24);
	OnHint.Broadcast(FString::Printf(TEXT("هدنة %d ثانية — تزوّد بالذخيرة والإسعافات"), FMath::CeilToInt(PhaseClock)));
}

// ------------------------------------------------------------------ supply run
void UFOChallengeComponent::TickSupplyRun(float Dt)
{
	Clock -= Dt;
	if (Clock <= 0.f)
	{
		Clock = 0.f;
		Fail(FString::Printf(TEXT("انتهى الوقت — %d/%d صناديق، لم تصل إلى الإخلاء"), SupplyCollected, Def.SupplyTarget));
		return;
	}
	// Keep a capped ambient pressure alive; roamers respawn slowly, always outside the safe radius.
	AmbientClock -= Dt;
	if (AmbientClock <= 0.f && LiveEnemies() < Def.AmbientLive)
	{
		SpawnEnemy(Def.AmbientRunnerChance, 0.2f, Def.AmbientHpMul, false);
		AmbientClock = FMath::Max(1.f, Def.AmbientRespawnSeconds);
	}
}

bool UFOChallengeComponent::IsExtractionOpen() const
{
	return Def.Mode == EFOFlowMode::SupplyRun && CurrentPhase == EFOChallengePhase::RunActive && SupplyCollected >= Def.SupplyTarget;
}

// ------------------------------------------------------------------ events
void UFOChallengeComponent::HandleEvent(const FFOGameEvent& E)
{
	if (!IsActive()) return;
	switch (E.Type)
	{
	case EFOGameEvent::ItemCollected:
		if (Def.Mode == EFOFlowMode::SupplyRun && E.Tag == TEXT("supply"))
		{
			SupplyCollected = FMath::Min(SupplyCollected + FMath::Max(1, E.Count), Def.SupplyTarget);
			if (SupplyCollected >= Def.SupplyTarget)
				OnHint.Broadcast(TEXT("كل الصناديق معك — اركض إلى نقطة الإخلاء! 🟢"));
			else
				OnHint.Broadcast(FString::Printf(TEXT("صندوق إمداد %d/%d"), SupplyCollected, Def.SupplyTarget));
		}
		break;
	case EFOGameEvent::ExitReached:
		if (Def.Mode != EFOFlowMode::SupplyRun) { OnHint.Broadcast(TEXT("لا مخرج في هذا الطور — اصمد في الساحة")); break; }
		if (!IsExtractionOpen())
		{
			OnHint.Broadcast(FString::Printf(TEXT("نقطة الإخلاء مقفلة — الصناديق %d/%d 🔒"), SupplyCollected, Def.SupplyTarget));
			break;
		}
		FinishCleared();
		break;
	case EFOGameEvent::PlayerDied:
		NotifyPlayerDied();
		break;
	default: break;
	}
}

void UFOChallengeComponent::NotifyPlayerDied()
{
	if (!IsActive()) return;
	Fail(Def.Mode == EFOFlowMode::Survival
		? FString::Printf(TEXT("سقطت في الموجة %d/%d"), WaveIndex + 1, Def.Waves.Num())
		: FString::Printf(TEXT("سقطت وأنت تحمل %d/%d صناديق"), SupplyCollected, Def.SupplyTarget));
}

void UFOChallengeComponent::FinishCleared()
{
	CurrentPhase = EFOChallengePhase::Cleared;
	ClearSpawnedEnemies();
	UE_LOG(LogFO, Display, TEXT("Challenge '%s' cleared in %.1fs"), *Def.Id.ToString(), Elapsed);
	OnCleared.Broadcast();
}

void UFOChallengeComponent::Fail(const FString& InReason)
{
	CurrentPhase = EFOChallengePhase::Failed;
	Reason = InReason;
	ClearSpawnedEnemies();
	UE_LOG(LogFO, Display, TEXT("Challenge '%s' failed: %s"), *Def.Id.ToString(), *Reason);
	OnFailed.Broadcast(Reason);
}

// ------------------------------------------------------------------ enemies
void UFOChallengeComponent::PruneEnemies()
{
	for (int32 i = WaveEnemies.Num() - 1; i >= 0; --i)
	{
		AFOZombie* Z = WaveEnemies[i].Get();
		if (!Z)
		{
			// Vanished without dying (streamed out, external Destroy, teardown). This is NOT a kill:
			// give the wave its budget back so a disappearance can never fake progress.
			WaveEnemies.RemoveAt(i);
			LostEnemies++;
			if (CurrentPhase == EFOChallengePhase::WaveActive && SpawnedThisWave > 0) SpawnedThisWave--;
			UE_LOG(LogFO, Warning, TEXT("Challenge: enemy disappeared without dying; wave budget restored"));
			continue;
		}
		if (Z->bDying)
		{
			DeadCounted++;      // observed death of a spawned enemy, not a reported kill event
			WaveEnemies.RemoveAt(i);
		}
	}
}

int32 UFOChallengeComponent::LiveEnemies() const
{
	int32 N = 0;
	for (const TWeakObjectPtr<AFOZombie>& W : WaveEnemies)
	{
		AFOZombie* Z = W.Get();
		if (Z && !Z->bDying && !Z->IsActorBeingDestroyed()) N++;
	}
	return N;
}

int32 UFOChallengeComponent::DeadThisWave() const { return DeadCounted; }

bool UFOChallengeComponent::ChooseSpawnLocation(FVector& Out) const
{
	const AFOGameMode* GM = Cast<AFOGameMode>(GetOwner());
	const AFOCharacter* P = GM ? GM->Player() : nullptr;
	const FVector Player = P ? P->GetActorLocation() : FVector(0, 0, 100.f);
	const float MinX = 200.f, MaxX = FMath::Max(400.f, Def.StreetLength - 200.f);
	const UWorld* W = GetWorld();
	// Reject candidates that would spawn inside authored cover / props: the AI has no pathfinding,
	// so an enemy stuck inside geometry would stall the wave forever.
	auto IsFree = [W](const FVector& C)
	{
		if (!W) return true;
		FCollisionQueryParams Params(FName(TEXT("FOChallengeSpawn")), false);
		return !W->OverlapBlockingTestByChannel(C, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(44.f, 92.f), Params);
	};
	for (int32 Try = 0; Try < 16; ++Try)
	{
		const FVector C(Rng.FRandRange(MinX, MaxX), Rng.FRandRange(-450.f, 450.f), 100.f);
		if (FVector::Dist2D(C, Player) >= Def.SafeSpawnRadius && IsFree(C)) { Out = C; return true; }
	}
	// Deterministic fallback: whichever arena end is further from the player, still outside the radius.
	const FVector A(MinX, Rng.FRandRange(-350.f, 350.f), 100.f);
	const FVector B(MaxX, Rng.FRandRange(-350.f, 350.f), 100.f);
	const FVector Pick = FVector::Dist2D(A, Player) > FVector::Dist2D(B, Player) ? A : B;
	Out = Pick;
	return FVector::Dist2D(Pick, Player) >= Def.SafeSpawnRadius && IsFree(Pick);
}

void UFOChallengeComponent::SpawnEnemy(float RunnerChance, float HeavyChance, float HpMul, bool bWaveEnemy)
{
	UWorld* W = GetWorld();
	if (!W) return;
	FVector Loc;
	if (!ChooseSpawnLocation(Loc))
	{
		// Never cheat a spawn on top of the player: skip this attempt instead.
		UE_LOG(LogFO, Warning, TEXT("Challenge: no fair spawn point available this tick"));
		return;
	}
	const FTransform T(FRotator(0, Rng.FRandRange(0.f, 360.f), 0), Loc);
	AFOZombie* Z = W->SpawnActorDeferred<AFOZombie>(AFOZombie::StaticClass(), T);
	if (!Z) return;
	const bool bHeavy = Rng.FRand() < HeavyChance;
	Z->Variant = bHeavy ? 2 : Rng.RandRange(0, 3);
	if (Z->Variant == 2 && !bHeavy) Z->Variant = 0;
	Z->bRunner = !bHeavy && Rng.FRand() < RunnerChance;
	Z->HpMul = HpMul;
	Z->bChasing = true;
	UGameplayStatics::FinishSpawningActor(Z, T);
	WaveEnemies.Add(Z);
	AllSpawned.Add(Z);
	SpawnLocations.Add(Loc);
	if (bWaveEnemy) SpawnedThisWave++;
}

void UFOChallengeComponent::ClearSpawnedEnemies()
{
	// Drop the wave list first: a deliberate teardown must not be accounted as a lost enemy.
	WaveEnemies.Empty();
	for (const TWeakObjectPtr<AFOZombie>& W : AllSpawned)
		if (AFOZombie* Z = W.Get()) Z->Destroy();
	AllSpawned.Empty();
}

void UFOChallengeComponent::DropResupply()
{
	UWorld* W = GetWorld();
	if (!W || Def.ResupplyPoints.Num() == 0) return;
	// Remove any uncollected leftovers first so drops cannot stack up over five waves.
	for (const TWeakObjectPtr<AFOPickup>& P : DroppedPickups)
		if (AFOPickup* Old = P.Get()) Old->Destroy();
	DroppedPickups.Empty();
	for (int32 i = 0; i < Def.ResupplyPoints.Num(); i++)
	{
		const FTransform T(Def.ResupplyPoints[i]);
		if (AFOPickup* Pick = W->SpawnActorDeferred<AFOPickup>(AFOPickup::StaticClass(), T))
		{
			const bool bAmmo = (i % 2 == 0);
			Pick->Item = bAmmo ? EFOItem::Ammo : EFOItem::Health;
			Pick->Tag = bAmmo ? FName(TEXT("ammo")) : FName(TEXT("health"));
			// Challenge economy: a wave of 10-12 (some heavy) needs well over a magazine.
			Pick->AmmoAmount = 30;
			Pick->HealAmount = 55.f;
			UGameplayStatics::FinishSpawningActor(Pick, T);
			DroppedPickups.Add(Pick);
		}
	}
	ResupplyCount++;
}

// ------------------------------------------------------------------ HUD text
FString UFOChallengeComponent::GetObjectiveText() const
{
	if (Def.Mode == EFOFlowMode::Survival)
	{
		switch (CurrentPhase)
		{
		case EFOChallengePhase::Countdown:
			return FString::Printf(TEXT("• الموجة 1/%d بعد %d ث — تزوّد الآن"), Def.Waves.Num(), FMath::CeilToInt(PhaseClock));
		case EFOChallengePhase::WaveActive:
			return FString::Printf(TEXT("• الموجة %d/%d\n• القضاء على %d/%d — أحياء الآن %d"),
				WaveIndex + 1, Def.Waves.Num(), DeadCounted, WaveTarget(), LiveEnemies());
		case EFOChallengePhase::WaveRest:
			return FString::Printf(TEXT("✓ الموجة %d/%d اكتملت\n• هدنة %d ث — خذ الذخيرة والإسعافات"),
				ClearedWaves, Def.Waves.Num(), FMath::CeilToInt(PhaseClock));
		case EFOChallengePhase::Cleared: return TEXT("✓ صمدت أمام كل الموجات");
		case EFOChallengePhase::Failed:  return FString::Printf(TEXT("✗ %s"), *Reason);
		default: return TEXT("• جاري التحضير");
		}
	}
	if (CurrentPhase == EFOChallengePhase::Failed) return FString::Printf(TEXT("✗ %s"), *Reason);
	if (CurrentPhase == EFOChallengePhase::Cleared) return TEXT("✓ تم الإخلاء بالإمدادات");
	const int32 M = FMath::FloorToInt(Clock) / 60, S = FMath::FloorToInt(Clock) % 60;
	return FString::Printf(TEXT("• صناديق الإمداد %d/%d\n• %s\n• الوقت المتبقي %d:%02d"),
		SupplyCollected, Def.SupplyTarget,
		IsExtractionOpen() ? TEXT("نقطة الإخلاء مفتوحة — اذهب إليها 🟢") : TEXT("نقطة الإخلاء مقفلة 🔒"),
		M, S);
}

FString UFOChallengeComponent::GetStatusLine() const
{
	if (Def.Mode == EFOFlowMode::Survival)
		return FString::Printf(TEXT("موجة %d/%d — أحياء %d"), FMath::Clamp(WaveIndex + 1, 1, FMath::Max(1, Def.Waves.Num())), Def.Waves.Num(), LiveEnemies());
	const int32 M = FMath::FloorToInt(Clock) / 60, S = FMath::FloorToInt(Clock) % 60;
	return FString::Printf(TEXT("⏱ %d:%02d — إمداد %d/%d"), M, S, SupplyCollected, Def.SupplyTarget);
}
