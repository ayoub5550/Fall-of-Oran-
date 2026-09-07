#include "FOGameInstance.h"
#include "FOLevelRegistry.h"
#include "FOChallengeRegistry.h"
#include "FallOfOran.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

const TCHAR* UFOGameInstance::SlotName = TEXT("fo_progress");
const TCHAR* UFOGameInstance::ChallengeSlotName = TEXT("fo_challenge");

void UFOGameInstance::Init()
{
	Super::Init();
	Load();
	LoadChallenges();
	// Dev/CI override: -FOMode=campaign|survival|supply selects the flow headlessly.
	FString ModeArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("FOMode="), ModeArg))
	{
		if (ModeArg.Equals(TEXT("survival"), ESearchCase::IgnoreCase)) SetFlowMode(EFOFlowMode::Survival);
		else if (ModeArg.StartsWith(TEXT("supply"), ESearchCase::IgnoreCase)) SetFlowMode(EFOFlowMode::SupplyRun);
		else SetFlowMode(EFOFlowMode::Campaign);
	}
	// Dev/CI override: -FOLevel=N starts on level N (0-based) regardless of progress.
	int32 Forced = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("FOLevel="), Forced) && FFOLevelRegistry::Get(Forced))
	{
		Progress.SelectedLevel = Forced;
		Progress.UnlockedLevels = FMath::Max(Progress.UnlockedLevels, Forced + 1);
	}
	UE_LOG(LogFO, Display, TEXT("GameInstance: %d levels, unlocked %d, selected %d"), FFOLevelRegistry::Num(), Progress.UnlockedLevels, Progress.SelectedLevel);
}

UFOGameInstance* UFOGameInstance::Get(const UObject* WorldContext)
{
	return WorldContext ? Cast<UFOGameInstance>(UGameplayStatics::GetGameInstance(WorldContext)) : nullptr;
}

const FFOLevelDef* UFOGameInstance::CurrentLevel() const
{
	const FFOLevelDef* L = FFOLevelRegistry::Get(Progress.SelectedLevel);
	return L ? L : FFOLevelRegistry::Get(0);
}

void UFOGameInstance::SelectLevel(int32 Index)
{
	if (IsUnlocked(Index)) { Progress.SelectedLevel = Index; Save(); }
}

void UFOGameInstance::OnLevelCompleted(int32 Index, int32 Kills, float Seconds)
{
	const int32 N = FFOLevelRegistry::Num();
	Progress.BestKills.SetNumZeroed(N);
	Progress.BestTimeSeconds.SetNumZeroed(N);
	if (Index >= 0 && Index < N)
	{
		Progress.BestKills[Index] = FMath::Max(Progress.BestKills[Index], Kills);
		if (Progress.BestTimeSeconds[Index] <= 0.f || Seconds < Progress.BestTimeSeconds[Index]) Progress.BestTimeSeconds[Index] = Seconds;
		Progress.UnlockedLevels = FMath::Clamp(FMath::Max(Progress.UnlockedLevels, Index + 2), 1, N);
	}
	Save();
}

bool UFOGameInstance::IsCampaignFinished() const { return Progress.SelectedLevel >= FFOLevelRegistry::Num() - 1; }

bool UFOGameInstance::AdvanceToNextLevel()
{
	if (Progress.SelectedLevel + 1 >= FFOLevelRegistry::Num()) return false;
	Progress.SelectedLevel++;
	Save();
	return true;
}

void UFOGameInstance::Save()
{
	UFOSaveGame* SG = Cast<UFOSaveGame>(UGameplayStatics::CreateSaveGameObject(UFOSaveGame::StaticClass()));
	if (!SG) return;
	SG->Progress = Progress;
	UGameplayStatics::SaveGameToSlot(SG, SlotName, 0);
}

void UFOGameInstance::Load()
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
		if (UFOSaveGame* SG = Cast<UFOSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
			Progress = SG->Progress;
	Progress.UnlockedLevels = FMath::Clamp(Progress.UnlockedLevels, 1, FMath::Max(1, FFOLevelRegistry::Num()));
	Progress.SelectedLevel = FMath::Clamp(Progress.SelectedLevel, 0, Progress.UnlockedLevels - 1);
	if (!(Progress.Brightness >= 0.25f && Progress.Brightness <= 4.f)) Progress.Brightness = 1.f;
}

void UFOGameInstance::ResetProgress() { Progress = FFOProgress(); Save(); }

// ------------------------------------------------------------------ mode selection + challenge records
void UFOGameInstance::SetFlowMode(EFOFlowMode NewMode)
{
	// Challenge modes need a valid definition; otherwise stay in campaign.
	if (NewMode != EFOFlowMode::Campaign)
	{
		const FFOChallengeDef* C = FFOChallengeRegistry::FindByMode(NewMode);
		if (!C || !C->IsValid())
		{
			UE_LOG(LogFO, Warning, TEXT("Flow mode %d has no valid challenge definition; staying in campaign"), (int32)NewMode);
			Mode = EFOFlowMode::Campaign;
			return;
		}
	}
	Mode = NewMode;
	// NOTE: the campaign save slot is intentionally NOT written here. Mode is session state.
}

void UFOGameInstance::CycleFlowMode(int32 Delta)
{
	static const EFOFlowMode Order[] = { EFOFlowMode::Campaign, EFOFlowMode::Survival, EFOFlowMode::SupplyRun };
	const int32 N = UE_ARRAY_COUNT(Order);
	int32 Cur = 0;
	for (int32 i = 0; i < N; i++) if (Order[i] == Mode) Cur = i;
	const int32 Want = ((Cur + Delta) % N + N) % N;
	SetFlowMode(Order[Want]);
}

const FFOChallengeDef* UFOGameInstance::CurrentChallenge() const
{
	return Mode == EFOFlowMode::Campaign ? nullptr : FFOChallengeRegistry::FindByMode(Mode);
}

FFOChallengeRecord& UFOGameInstance::MutableRecord(FName Id)
{
	for (FFOChallengeRecord& R : Challenges.Records) if (R.Id == Id) return R;
	FFOChallengeRecord New; New.Id = Id;
	return Challenges.Records[Challenges.Records.Add(New)];
}

const FFOChallengeRecord* UFOGameInstance::ChallengeRecord(FName Id) const
{
	for (const FFOChallengeRecord& R : Challenges.Records) if (R.Id == Id) return &R;
	return nullptr;
}

void UFOGameInstance::NoteChallengeAttempt(FName Id)
{
	if (Id.IsNone()) return;
	MutableRecord(Id).Attempts++;
	SaveChallenges();
}

void UFOGameInstance::OnChallengeFinished(FName Id, bool bCleared, int32 WavesCleared, int32 Kills, float Seconds, float TimeLeft)
{
	if (Id.IsNone()) return;
	FFOChallengeRecord& R = MutableRecord(Id);
	R.BestWave = FMath::Max(R.BestWave, FMath::Max(0, WavesCleared));
	R.BestKills = FMath::Max(R.BestKills, FMath::Max(0, Kills));
	if (bCleared)
	{
		R.bCleared = true;
		if (Seconds > 0.f && (R.BestClearSeconds <= 0.f || Seconds < R.BestClearSeconds)) R.BestClearSeconds = Seconds;
		R.BestTimeLeft = FMath::Max(R.BestTimeLeft, FMath::Max(0.f, TimeLeft));
	}
	SaveChallenges();
	// Explicitly NOT calling Save()/OnLevelCompleted: challenges never unlock campaign levels.
}

void UFOGameInstance::SaveChallenges()
{
	UFOChallengeSaveGame* SG = Cast<UFOChallengeSaveGame>(UGameplayStatics::CreateSaveGameObject(UFOChallengeSaveGame::StaticClass()));
	if (!SG) return;
	SG->Progress = Challenges;
	UGameplayStatics::SaveGameToSlot(SG, ChallengeSlotName, 0);
}

void UFOGameInstance::LoadChallenges()
{
	Challenges = FFOChallengeProgress();
	if (UGameplayStatics::DoesSaveGameExist(ChallengeSlotName, 0))
		if (UFOChallengeSaveGame* SG = Cast<UFOChallengeSaveGame>(UGameplayStatics::LoadGameFromSlot(ChallengeSlotName, 0)))
			Challenges = SG->Progress;
	// Drop records for challenges that no longer exist and clamp obviously corrupt values.
	for (int32 i = Challenges.Records.Num() - 1; i >= 0; --i)
	{
		FFOChallengeRecord& R = Challenges.Records[i];
		const FFOChallengeDef* Def = FFOChallengeRegistry::Find(R.Id);
		if (!Def) { Challenges.Records.RemoveAt(i); continue; }
		R.BestWave = FMath::Clamp(R.BestWave, 0, FMath::Max(1, Def->Waves.Num()));
		R.BestKills = FMath::Max(0, R.BestKills);
		R.Attempts = FMath::Max(0, R.Attempts);
		if (!FMath::IsFinite(R.BestClearSeconds) || R.BestClearSeconds < 0.f) R.BestClearSeconds = 0.f;
		if (!FMath::IsFinite(R.BestTimeLeft) || R.BestTimeLeft < 0.f) R.BestTimeLeft = 0.f;
	}
}

void UFOGameInstance::ResetChallengeRecords() { Challenges = FFOChallengeProgress(); SaveChallenges(); }
