#include "Puzzles/FOPuzzleBase.h"
#include "FOGameMode.h"
#include "FOCharacter.h"
#include "FallOfOran.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AFOPuzzleBase::AFOPuzzleBase()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	FlatMaster = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Flat.M_Flat"));
	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	SolvedSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/pickup.pickup"));
	FailSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/click.click"));
	ClickSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/click.click"));
}

void AFOPuzzleBase::Setup(const FFOPuzzleDef& InDef, const FRandomStream&)
{
	Def = InDef;
	BuildWorkLight();
}

void AFOPuzzleBase::BuildWorkLight()
{
	// Maintenance floodlight on a tripod in front of the puzzle (-X local = the side the player approaches from).
	// v2.0 proof shots showed both puzzles almost black: the street lamps are 20 m apart and the cabinets sit
	// against the dark building faces. A warm local light makes the puzzle readable and acts as a beacon.
	const FLinearColor Warm(1.f, 0.82f, 0.55f);
	MakeBox(FVector(-170.f, 0, 90.f), FVector(10.f, 10.f, 180.f), FLinearColor(0.08f, 0.08f, 0.09f));             // tripod pole
	MakeBox(FVector(-170.f, 0, 8.f), FVector(70.f, 70.f, 16.f), FLinearColor(0.08f, 0.08f, 0.09f));               // base
	MakeBox(FVector(-160.f, 0, 190.f), FVector(30.f, 44.f, 30.f), FLinearColor(0.1f, 0.1f, 0.11f), FLinearColor::Black, false); // housing
	MakeBox(FVector(-144.f, 0, 190.f), FVector(3.f, 36.f, 24.f), Warm, Warm * 8.f, false);                       // glowing face
	WorkLight = MakeLight(FVector(-120.f, 0, 200.f), Warm, 90.f, 1100.f);
	// Hazard stripes on the ground in front of the puzzle so it reads from far away.
	for (int32 i = 0; i < 4; i++)
		MakeBox(FVector(-40.f - i * 40.f, 0, 1.f), FVector(16.f, 220.f, 2.f), (i % 2) ? FLinearColor(0.9f, 0.75f, 0.1f) : FLinearColor(0.05f, 0.05f, 0.05f), (i % 2) ? FLinearColor(0.9f, 0.75f, 0.1f) * 0.6f : FLinearColor::Black, false);
}

AFOGameMode* AFOPuzzleBase::GM() const { return GetWorld() ? GetWorld()->GetAuthGameMode<AFOGameMode>() : nullptr; }

bool AFOPuzzleBase::CanInteract(AFOCharacter*) const
{
	const AFOGameMode* G = GM();
	return !IsSolved() && G && G->State == EFOState::Playing && G->IsPuzzleActive(Def.Id);
}

void AFOPuzzleBase::Solve()
{
	if (IsSolved()) return;
	State = EFOPuzzleState::Solved;
	if (SolvedSound) UGameplayStatics::PlaySound2D(this, SolvedSound);
	UE_LOG(LogFO, Display, TEXT("Puzzle %s solved"), *Def.Id.ToString());
	if (AFOGameMode* G = GM()) G->ReportEvent(FFOGameEvent(EFOGameEvent::PuzzleSolved, Def.Id));
}

void AFOPuzzleBase::Fail(const FString& Message)
{
	if (FailSound) UGameplayStatics::PlaySound2D(this, FailSound, 1.f, 0.6f);
	if (AFOGameMode* G = GM()) G->OnPuzzleFailed(Message, Def.PenaltyZombies);
}

UMaterialInstanceDynamic* AFOPuzzleBase::MakeFlat(const FLinearColor& Tint, const FLinearColor& Emissive, float Rough)
{
	if (!FlatMaster) return nullptr;
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(FlatMaster, this);
	M->SetVectorParameterValue(TEXT("Tint"), Tint);
	M->SetVectorParameterValue(TEXT("Emissive"), Emissive);
	M->SetScalarParameterValue(TEXT("Roughness"), Rough);
	return M;
}

UStaticMeshComponent* AFOPuzzleBase::MakeBox(const FVector& RelLoc, const FVector& Size, const FLinearColor& Tint, const FLinearColor& Emissive, bool bCollide)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(CubeMesh);
	C->SetupAttachment(Root);
	C->SetRelativeLocation(RelLoc);
	C->SetRelativeScale3D(Size / 100.f);
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->SetMaterial(0, MakeFlat(Tint, Emissive));
	C->RegisterComponent();
	return C;
}

UPointLightComponent* AFOPuzzleBase::MakeLight(const FVector& RelLoc, const FLinearColor& Color, float Intensity, float Radius)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetupAttachment(Root);
	L->SetRelativeLocation(RelLoc);
	L->SetLightColor(Color);
	L->SetIntensity(Intensity);
	L->SetAttenuationRadius(Radius);
	L->SetCastShadows(false);
	L->bUseInverseSquaredFalloff = false;
	L->RegisterComponent();
	return L;
}

// ---------------------------------------------------------------- part
AFOPuzzlePart::AFOPuzzlePart()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Lamp = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(Mesh);
	Lamp->SetCastShadows(false);
	Lamp->bUseInverseSquaredFalloff = false;
	Lamp->SetAttenuationRadius(260.f);
	Lamp->SetIntensity(0.f);
}

bool AFOPuzzlePart::CanInteract(AFOCharacter* Who) const { return Parent.IsValid() && Parent->CanInteract(Who); }
