#include "FOGameInstance.h"
#include "FOLevelRegistry.h"
#include "FallOfOran.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

const TCHAR* UFOGameInstance::SlotName = TEXT("fo_progress");

void UFOGameInstance::Init()
{
	Super::Init();
	Load();
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
