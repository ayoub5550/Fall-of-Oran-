#pragma once
#include "Puzzles/FOPuzzleBase.h"
#include "FOGeneratorPuzzle.generated.h"

/** A reusable defend-in-place encounter, activated only after its prerequisite stage. */
UCLASS()
class AFOGeneratorPuzzle : public AFOPuzzleBase
{
	GENERATED_BODY()
public:
	AFOGeneratorPuzzle();
	virtual void Setup(const FFOPuzzleDef& InDef, const FRandomStream& Rng) override;
	virtual void Tick(float Dt) override;
	virtual bool CanInteract(AFOCharacter* Who) const override;
	virtual FString GetPrompt() const override { return TEXT("عبّئ الوقود وشغّل المولّد"); }
	virtual void Interact(AFOCharacter* Who) override;
	virtual void DebugSolve() override;
	float StartupProgress() const { return Elapsed; }
	bool IsRunning() const { return bRunning; }
private:
	void FinishStartup();
	bool bRunning = false;
	float Elapsed = 0.f;
	float HintClock = 0.f;
	static constexpr float StartupSeconds = 18.f;
	static constexpr float DefendRadius = 900.f;
	UPROPERTY() UPointLightComponent* StatusLight = nullptr;
};
