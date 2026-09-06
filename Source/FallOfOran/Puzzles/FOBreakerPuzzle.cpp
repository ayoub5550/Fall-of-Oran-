#include "Puzzles/FOBreakerPuzzle.h"
#include "FOCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"

static const TCHAR* GColorNames[] = { TEXT("أحمر"), TEXT("أزرق"), TEXT("أخضر"), TEXT("أصفر"), TEXT("بنفسجي"), TEXT("برتقالي") };
static const FLinearColor GColors[] = { FLinearColor(1, 0.1f, 0.05f), FLinearColor(0.1f, 0.35f, 1), FLinearColor(0.1f, 1, 0.25f), FLinearColor(1, 0.9f, 0.1f), FLinearColor(0.7f, 0.1f, 1), FLinearColor(1, 0.5f, 0.05f) };
const TCHAR* AFOBreakerPuzzle::ColorName(int32 i) { return GColorNames[i % UE_ARRAY_COUNT(GColorNames)]; }
FLinearColor AFOBreakerPuzzle::Color(int32 i) { return GColors[i % UE_ARRAY_COUNT(GColors)]; }

void AFOBreakerPuzzle::Setup(const FFOPuzzleDef& InDef, const FRandomStream& Rng)
{
	Super::Setup(InDef, Rng);
	const int32 N = FMath::Clamp(Def.Size, 2, 6);
	// Seeded shuffle -> same order every time for this level (players can learn it).
	for (int32 i = 0; i < N; i++) Order.Add(i);
	for (int32 i = N - 1; i > 0; i--) Order.Swap(i, Rng.RandRange(0, i));

	// Cabinet: dark metal box against the wall, breakers in a row, status lamp on top.
	const float W = N * 70.f + 60.f;
	MakeBox(FVector(0, 0, 120.f), FVector(30.f, W, 220.f), FLinearColor(0.12f, 0.13f, 0.14f));
	MakeBox(FVector(-16.f, 0, 120.f), FVector(4.f, W - 20.f, 190.f), FLinearColor(0.05f, 0.06f, 0.07f), FLinearColor::Black, false);
	StatusPanel = MakeBox(FVector(-18.f, 0, 235.f), FVector(6.f, 60.f, 20.f), FLinearColor(0.3f, 0.02f, 0.02f), FLinearColor(1.f, 0.05f, 0.05f) * 2.f, false);
	StatusLight = MakeLight(FVector(-60.f, 0, 235.f), FLinearColor(1, 0.1f, 0.1f), 20.f, 500.f);
	// Warning sign texture if present
	for (int32 i = 0; i < N; i++)
	{
		const FVector Loc = GetActorTransform().TransformPosition(FVector(-24.f, -W * 0.5f + 50.f + i * 70.f, 120.f));
		AFOPuzzlePart* S = GetWorld()->SpawnActor<AFOPuzzlePart>(AFOPuzzlePart::StaticClass(), Loc, GetActorRotation());
		if (!S) continue;
		S->Parent = this; S->Index = i;
		S->Prompt = FString::Printf(TEXT("قاطع %s"), ColorName(i));
		S->Mesh->SetRelativeScale3D(FVector(0.14f, 0.40f, 0.60f));
		S->Mat = MakeFlat(Color(i) * 0.5f, Color(i) * 0.4f, 0.4f);
		S->Mesh->SetMaterial(0, S->Mat);
		S->Lamp->SetLightColor(Color(i));
		S->Lamp->SetRelativeLocation(FVector(-250.f, 0, 0));
		S->OnUse = [this, i](AFOCharacter*) { Press(i); };
		Switches.Add(S);
	}
}

FString AFOBreakerPuzzle::GetHintText(int32) const
{
	FString T = TEXT("ورقة الصيانة — ترتيب إعادة التيار:\n");
	for (int32 k = 0; k < Order.Num(); k++) { if (k) T += TEXT(" ← "); T += ColorName(Order[k]); }
	return T;
}

void AFOBreakerPuzzle::Press(int32 Index)
{
	if (IsSolved()) return;
	if (ClickSound) UGameplayStatics::PlaySound2D(this, ClickSound);
	if (Order.IsValidIndex(Progress) && Order[Progress] == Index)
	{
		if (AFOPuzzlePart* S = Switches[Index]) { S->Mat->SetVectorParameterValue(TEXT("Emissive"), Color(Index) * 3.f); S->Lamp->SetIntensity(15.f); }
		Progress++;
		if (Progress >= Order.Num())
		{
			if (StatusPanel) if (auto* M = Cast<UMaterialInstanceDynamic>(StatusPanel->GetMaterial(0))) { M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.02f, 0.3f, 0.05f)); M->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.1f, 1.f, 0.2f) * 2.f); }
			if (StatusLight) StatusLight->SetLightColor(FLinearColor(0.2f, 1, 0.3f));
			Solve();
		}
		return;
	}
	ResetSwitches();
	Fail(TEXT("ترتيب خاطئ! انطلق الإنذار… ⚠"));
}

void AFOBreakerPuzzle::ResetSwitches()
{
	Progress = 0;
	for (int32 i = 0; i < Switches.Num(); i++)
		if (AFOPuzzlePart* S = Switches[i]) { S->Mat->SetVectorParameterValue(TEXT("Emissive"), Color(i) * 0.4f); S->Lamp->SetIntensity(0.f); }
}
