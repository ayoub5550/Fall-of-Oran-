#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FOGameMode.generated.h"

class AFOCharacter;
class AFOZombie;
class SFOHud;

UENUM()
enum class EFOState : uint8 { Menu, Playing, Dead, Won };

/** Game flow: menu → playing (collect 3 fuel cans, reach the port gate) → dead / won. */
UCLASS()
class AFOGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AFOGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

	void StartGame();
	void Restart();
	void OnZombieKilled(AFOCharacter* Killer);
	void OnPlayerDamaged();
	void OnPlayerDied();
	void OnFuelCollected();
	void OnExitReached();
	void Win();
	void SpawnZombiesBehindPlayer(int32 Count);
	void SetObjective(const FString& Text, float Seconds = 0.f);
	/** Player-facing brightness control (persisted). Step is multiplicative, e.g. 1.25 or 0.8. */
	void AdjustBrightness(float Step);
	float CurrentBrightness() const;

	EFOState State = EFOState::Menu;
	int32 Kills = 0;
	int32 Fuel = 0;
	static constexpr int32 FuelNeeded = 3;
	float DamageFlash = 0.f;
	float HintTimer = 0.f;
	FString Objective;
	FString DefaultObjective;
	FString Subtitle;
	TSharedPtr<SFOHud> Hud;
	class AFOWorldBuilder* World = nullptr;
	AFOCharacter* Player() const;
	float RestartTimer = 0.f;

	// Automated screenshot tour (-FOShots on the command line): used for headless CI captures.
	bool bShotMode = false;
	float ShotClock = 0.f;
	int32 ShotIndex = 0;
	void TickShots(float Dt);
};
