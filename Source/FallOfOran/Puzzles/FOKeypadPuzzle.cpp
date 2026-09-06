#include "Puzzles/FOKeypadPuzzle.h"
#include "FOGameMode.h"
#include "FOCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"

void AFOKeypadPuzzle::Setup(const FFOPuzzleDef& InDef, const FRandomStream& Rng)
{
	Super::Setup(InDef, Rng);
	if (Def.Code.IsEmpty()) { for (int32 i = 0; i < FMath::Clamp(Def.Size, 3, 6); i++) Def.Code.AppendChar(TCHAR('0' + Rng.RandRange(0, 9))); }
	// Post + panel + glowing screen
	MakeBox(FVector(0, 0, 70.f), FVector(16.f, 16.f, 140.f), FLinearColor(0.1f, 0.1f, 0.11f));
	MakeBox(FVector(0, 0, 150.f), FVector(14.f, 44.f, 60.f), FLinearColor(0.08f, 0.08f, 0.09f));
	Screen = MakeBox(FVector(-8.f, 0, 165.f), FVector(2.f, 34.f, 18.f), FLinearColor(0.02f, 0.05f, 0.1f), FLinearColor(0.1f, 0.3f, 1.f) * 2.5f, false);
	for (int32 r = 0; r < 3; r++) for (int32 c = 0; c < 3; c++)
		MakeBox(FVector(-8.f, -12.f + c * 12.f, 145.f - r * 9.f), FVector(2.f, 9.f, 7.f), FLinearColor(0.6f, 0.6f, 0.62f), FLinearColor(0.15f, 0.15f, 0.15f), false);
	ScreenLight = MakeLight(FVector(-40.f, 0, 165.f), FLinearColor(0.2f, 0.4f, 1.f), 10.f, 400.f);
}

FString AFOKeypadPuzzle::GetHintText(int32 NoteIndex) const
{
	if (!Def.Code.IsValidIndex(NoteIndex)) return TEXT("ورقة ممزقة… لا شيء مقروء.");
	static const TCHAR* Ordinal[] = { TEXT("الأول"), TEXT("الثاني"), TEXT("الثالث"), TEXT("الرابع"), TEXT("الخامس"), TEXT("السادس") };
	return FString::Printf(TEXT("مذكرة الحراسة — الرقم %s من شيفرة البوابة: %c"), Ordinal[FMath::Min(NoteIndex, 5)], Def.Code[NoteIndex]);
}

void AFOKeypadPuzzle::Interact(AFOCharacter*)
{
	if (ClickSound) UGameplayStatics::PlaySound2D(this, ClickSound);
	if (AFOGameMode* G = GM()) G->OpenKeypad(this);
}

bool AFOKeypadPuzzle::Submit(const FString& Entered)
{
	Attempts++;
	if (Entered == Def.Code)
	{
		if (Screen) if (auto* M = Cast<UMaterialInstanceDynamic>(Screen->GetMaterial(0))) M->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.1f, 1.f, 0.2f) * 2.5f);
		if (ScreenLight) ScreenLight->SetLightColor(FLinearColor(0.2f, 1.f, 0.3f));
		Solve();
		return true;
	}
	Fail(TEXT("شيفرة خاطئة! صفّارة الإنذار جذبت الزومبي ⚠"));
	return false;
}
