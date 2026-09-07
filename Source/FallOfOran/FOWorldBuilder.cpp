#include "FOWorldBuilder.h"
#include "FallOfOran.h"
#include "FOZombie.h"
#include "FOPickup.h"
#include "FOCharacter.h"
#include "FOGameMode.h"
#include "Mission/FOMissionComponent.h"
#include "Puzzles/FOBreakerPuzzle.h"
#include "Puzzles/FOKeypadPuzzle.h"
#include "Puzzles/FOGeneratorPuzzle.h"
#include "Puzzles/FONote.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AFOWorldBuilder::AFOWorldBuilder()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
	Rng.Initialize(20260906);
	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	CylMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	ConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	M_PBR = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_PBR.M_PBR"));
	M_Flat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Flat.M_Flat"));
	M_UV = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_UV.M_UV"));
	M_UVMasked = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_UVMasked.M_UVMasked"));
	M_Decal = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Decal.M_Decal"));
	M_Sky = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Sky.M_Sky"));
	ThunderSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/thunder.thunder"));
}

AFOWorldBuilder* AFOWorldBuilder::Get(UWorld* W)
{
	if (!W) return nullptr;
	TActorIterator<AFOWorldBuilder> It(W);
	return It ? *It : nullptr;
}

// ------------------------------------------------------------------ primitives
static UStaticMeshComponent* NewSMC(AActor* Owner, UStaticMesh* Mesh, UMaterialInterface* Mat, bool bCollide)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
	C->SetStaticMesh(Mesh);
	if (Mat) C->SetMaterial(0, Mat);
	C->SetMobility(EComponentMobility::Static);
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bCollide) C->SetCollisionProfileName(TEXT("BlockAll"));
	C->SetupAttachment(Owner->GetRootComponent());
	// NOTE: caller must set the transform (Place) BEFORE RegisterComponent — static components cannot move once registered in-game.
	return C;
}

static void Place(USceneComponent* C, const FTransform& T)
{
	C->SetWorldTransform(T);
	C->RegisterComponent();
}

UStaticMeshComponent* AFOWorldBuilder::Box(const FVector& Center, const FVector& Size, UMaterialInterface* Mat, const FRotator& Rot, bool bCollide)
{
	UStaticMeshComponent* C = NewSMC(this, CubeMesh, Mat, bCollide);
	Place(C, FTransform(Rot, Center, Size / 100.f));
	return C;
}

UStaticMeshComponent* AFOWorldBuilder::Cyl(const FVector& Base, float Radius, float Height, UMaterialInterface* Mat, const FRotator& Rot, bool bCollide)
{
	UStaticMeshComponent* C = NewSMC(this, CylMesh, Mat, bCollide);
	const FVector Center = Base + Rot.RotateVector(FVector(0, 0, Height * 0.5f));
	Place(C, FTransform(Rot, Center, FVector(Radius * 2.f / 100.f, Radius * 2.f / 100.f, Height / 100.f)));
	return C;
}

UStaticMeshComponent* AFOWorldBuilder::Quad(const FVector& Center, const FVector2D& Size, UMaterialInterface* Mat, const FRotator& Rot, bool bCollide)
{
	UStaticMeshComponent* C = NewSMC(this, PlaneMesh, Mat, bCollide);
	Place(C, FTransform(Rot, Center, FVector(Size.X / 100.f, Size.Y / 100.f, 1.f)));
	return C;
}

UStaticMeshComponent* AFOWorldBuilder::Prop(const TCHAR* Path, const FVector& Loc, float Yaw, float TargetHeightCm, UMaterialInterface* Override)
{
	UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Path);
	if (!M) { UE_LOG(LogFO, Warning, TEXT("Prop missing: %s"), Path); return nullptr; }
	UStaticMeshComponent* C = NewSMC(this, M, nullptr, true);
	const FBox B = M->GetBoundingBox();
	const float S = B.GetSize().Z > 1.f ? TargetHeightCm / B.GetSize().Z : 1.f;
	Place(C, FTransform(FRotator(0, Yaw, 0), Loc - FVector(0, 0, B.Min.Z * S), FVector(S)));
	if (Override) for (int32 i = 0; i < C->GetNumMaterials(); i++) C->SetMaterial(i, Override);
	return C;
}

static UTexture* LoadTex(const FString& Path)
{
	UTexture* T = LoadObject<UTexture>(nullptr, *Path);
	if (!T) UE_LOG(LogFO, Warning, TEXT("Texture missing: %s"), *Path);
	return T;
}

UMaterialInstanceDynamic* AFOWorldBuilder::Surface(const FFOSurface& S)
{
	if (!M_PBR) return nullptr;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M_PBR, this);
	const FString N = S.Tex.ToString();
	if (UTexture* T = LoadTex(FString::Printf(TEXT("/Game/Textures/T_%s_color.T_%s_color"), *N, *N))) MID->SetTextureParameterValue(TEXT("BaseColor"), T);
	if (UTexture* T = LoadTex(FString::Printf(TEXT("/Game/Textures/T_%s_normal.T_%s_normal"), *N, *N))) MID->SetTextureParameterValue(TEXT("Normal"), T);
	if (UTexture* T = LoadTex(FString::Printf(TEXT("/Game/Textures/T_%s_rough.T_%s_rough"), *N, *N))) MID->SetTextureParameterValue(TEXT("Rough"), T);
	MID->SetScalarParameterValue(TEXT("TileCm"), S.TileCm);
	MID->SetVectorParameterValue(TEXT("Tint"), S.Tint);
	MID->SetVectorParameterValue(TEXT("Emissive"), S.Emissive);
	MID->SetScalarParameterValue(TEXT("RoughMul"), S.RoughMul);
	return MID;
}

UMaterialInstanceDynamic* AFOWorldBuilder::Flat(const FLinearColor& Tint, float Rough, float Metal, const FLinearColor& Emis)
{
	if (!M_Flat) return nullptr;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M_Flat, this);
	MID->SetVectorParameterValue(TEXT("Tint"), Tint);
	MID->SetVectorParameterValue(TEXT("Emissive"), Emis);
	MID->SetScalarParameterValue(TEXT("Roughness"), Rough);
	MID->SetScalarParameterValue(TEXT("Metallic"), Metal);
	return MID;
}

UMaterialInstanceDynamic* AFOWorldBuilder::UVTex(const TCHAR* TexPath, const FLinearColor& Tint, float EmisMul, bool bMasked, float Rough)
{
	UMaterialInterface* Base = bMasked ? M_UVMasked : M_UV;
	if (!Base) return nullptr;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
	if (UTexture* T = LoadTex(TexPath)) MID->SetTextureParameterValue(TEXT("Tex"), T);
	MID->SetVectorParameterValue(TEXT("Tint"), Tint);
	MID->SetScalarParameterValue(TEXT("EmisMul"), EmisMul);
	MID->SetScalarParameterValue(TEXT("Roughness"), Rough);
	return MID;
}

UMaterialInstanceDynamic* AFOWorldBuilder::Decal(const TCHAR* TexPath, const FLinearColor& Tint, float Rough, float Metal)
{
	if (!M_Decal) return nullptr;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M_Decal, this);
	if (UTexture* T = LoadTex(TexPath)) MID->SetTextureParameterValue(TEXT("Tex"), T);
	MID->SetVectorParameterValue(TEXT("Tint"), Tint);
	MID->SetScalarParameterValue(TEXT("Roughness"), Rough);
	MID->SetScalarParameterValue(TEXT("Metallic"), Metal);
	return MID;
}

UPointLightComponent* AFOWorldBuilder::Light(const FVector& Loc, const FLinearColor& Color, float Intensity, float Radius, bool bShadow)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetMobility(EComponentMobility::Movable);
	L->SetIntensity(Intensity);
	L->SetLightColor(Color);
	L->SetAttenuationRadius(Radius);
	L->SetCastShadows(bShadow);
	L->bUseInverseSquaredFalloff = false;
	L->LightFalloffExponent = 3.f;
	L->SetupAttachment(RootComponent);
	L->SetWorldLocation(Loc);
	L->RegisterComponent();
	return L;
}

// ------------------------------------------------------------------ world
void AFOWorldBuilder::BuildWorld(const FFOLevelDef& Def)
{
	LevelDef = &Def;
	StreetLength = Def.StreetLength;
	Rng.Initialize(Def.Seed);
	BaseMoon = Def.Lighting.MoonIntensity;
	BaseSky = Def.Lighting.SkyLightIntensity;
	UE_LOG(LogFO, Log, TEXT("Building level '%s' (seed %d, %.0f cm)..."), *Def.Title, Def.Seed, StreetLength);
	BuildLighting();
	BuildSky();
	BuildGround();
	BuildBuildings();
	BuildStreetFurniture();
	BuildCars();
	BuildDebris();
	BuildExitGate();
	SpawnItems();
	SpawnPuzzles();
	// Authored local set dressing around generator objectives, not more random
	// street clutter. The main lane and the front interaction space remain open.
	for (const FFOPuzzleDef& P : Def.Puzzles)
	{
		if (P.Type != EFOPuzzleType::Generator) continue;
		const FVector B = P.Location;
		Prop(TEXT("/Game/Props/Checkpoint/SM_MaintenanceBench/SM_MaintenanceBench.SM_MaintenanceBench"), B + FVector(-350, -150, 0), 0, 85.f);
		Prop(TEXT("/Game/Props/Checkpoint/SM_SupplyCrate/SM_SupplyCrate.SM_SupplyCrate"), B + FVector(-500, -150, 0), 12.f, 55.f);
		Prop(TEXT("/Game/Props/Checkpoint/SM_SupplyBarrel/SM_SupplyBarrel.SM_SupplyBarrel"), B + FVector(280, -120, 0), 0, 90.f);
		Prop(TEXT("/Game/Props/Checkpoint/SM_CheckpointBarrier/SM_CheckpointBarrier.SM_CheckpointBarrier"), B + FVector(-600, 200, 0), 90.f, 85.f);
	}
	SpawnZombies();
	if (USoundBase* Amb = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/ambience.ambience")))
		Ambience = UGameplayStatics::SpawnSound2D(this, Amb, 0.8f, 1.f, 0.f, nullptr, true, true);
	UE_LOG(LogFO, Log, TEXT("Street built: %d components"), GetComponents().Num());
}

void AFOWorldBuilder::BuildLighting()
{
	UWorld* W = GetWorld();
	// Moon: cold key light from down-street
	Moon = W->SpawnActor<ADirectionalLight>(FVector(0, 0, 3000.f), FRotator(-38.f, 35.f, 0.f));
	if (Moon)
	{
		Moon->SetMobility(EComponentMobility::Movable);
		UDirectionalLightComponent* D = Cast<UDirectionalLightComponent>(Moon->GetLightComponent());
		D->SetIntensity(BaseMoon);
		D->SetLightColor(LevelDef ? LevelDef->Lighting.MoonColor : FLinearColor(0.6f, 0.7f, 0.92f));
		D->SetCastShadows(true);
		D->SetDynamicShadowDistanceMovableLight(6000.f);
		D->SetShadowAmount(0.85f);
	}
	Sky = W->SpawnActor<ASkyLight>(FVector(0, 0, 1000.f), FRotator::ZeroRotator);
	if (Sky)
	{
		USkyLightComponent* SC = Sky->GetLightComponent();
		SC->SetMobility(EComponentMobility::Movable);
		SC->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap; // no capture needed headless; falls back to color
		SC->SetIntensity(BaseSky);
		SC->SetLightColor(FLinearColor(0.35f, 0.42f, 0.6f));
		SC->SetLowerHemisphereColor(FLinearColor(0.2f, 0.2f, 0.25f));
	}
	AExponentialHeightFog* Fog = W->SpawnActor<AExponentialHeightFog>(FVector(0, 0, 0), FRotator::ZeroRotator);
	if (Fog)
	{
		UExponentialHeightFogComponent* F = Fog->GetComponent();
		F->SetFogDensity(LevelDef ? LevelDef->Lighting.FogDensity : 0.014f);
		F->SetFogHeightFalloff(0.35f);
		F->SetFogInscatteringColor(LevelDef ? LevelDef->Lighting.FogColor : FLinearColor(0.10f, 0.12f, 0.18f));
		F->SetStartDistance(300.f);
		F->SetFogMaxOpacity(0.96f);
		F->SetDirectionalInscatteringColor(FLinearColor(0.35f, 0.25f, 0.12f));
		F->SetDirectionalInscatteringExponent(12.f);
	}
	APostProcessVolume* PP = W->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (PP)
	{
		PP->bUnbound = true;
		// NOTE: do NOT override exposure (AEM_Manual/AutoExposureBias) or colour grading here.
		// On the mobile ES3.1 path with r.DefaultFeature.AutoExposure=False those overrides
		// blacked out the whole frame (verified with -FOShotDiag). Brightness is tuned via light intensities instead.
		PP->Settings.bOverride_BloomIntensity = true; PP->Settings.BloomIntensity = 0.55f;
		PP->Settings.bOverride_BloomThreshold = true; PP->Settings.BloomThreshold = 0.6f;
	}
}

void AFOWorldBuilder::BuildSky()
{
	// Panorama night sky (Santa Cruz fort, port cranes, moon) on an inverted sphere, unlit.
	if (!M_Sky || !SphereMesh) return;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M_Sky, this);
	if (UTexture* T = LoadTex(TEXT("/Game/Textures/T_sky_pano.T_sky_pano"))) MID->SetTextureParameterValue(TEXT("Tex"), T);
	MID->SetScalarParameterValue(TEXT("EmisMul"), 1.15f);
	UStaticMeshComponent* S = NewSMC(this, SphereMesh, MID, false);
	Place(S, FTransform(FRotator(0, 90.f, 0), FVector(StreetLength * 0.5f, 0, 0), FVector(-600.f, 600.f, 600.f))); // negative X scale flips normals inward
	S->bCastDynamicShadow = false;
	S->SetCastShadow(false);
}

void AFOWorldBuilder::BuildGround()
{
	const float L = StreetLength + 3000.f, Cx = StreetLength * 0.5f;
	// Asphalt base (rain-slick)
	Box(FVector(Cx, 0, -20.f), FVector(L, StreetWidth + 2400.f, 40.f), Surface({ TEXT("asphalt"), 350.f, FLinearColor(0.42f, 0.42f, 0.48f), FLinearColor::Black, 0.6f }));
	// Marked road strip with lane paint (world-aligned, texture pre-rotated so lanes run along X)
	Quad(FVector(Cx, 0, 1.2f), FVector2D(L, StreetWidth), Surface({ TEXT("road"), 1400.f, FLinearColor(0.62f, 0.62f, 0.68f), FLinearColor::Black, 0.55f }), FRotator::ZeroRotator);
	// Sidewalks + curbs
	UMaterialInstanceDynamic* Paving = Surface({ TEXT("paving"), 240.f, FLinearColor(0.55f, 0.52f, 0.5f) });
	UMaterialInstanceDynamic* Curb = Surface({ TEXT("concrete"), 200.f, FLinearColor(0.5f, 0.5f, 0.5f) });
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const float Y = Side * (StreetWidth * 0.5f + 150.f);
		Box(FVector(Cx, Y, 12.f), FVector(L, 300.f, 24.f), Paving);
		Box(FVector(Cx, Side * (StreetWidth * 0.5f + 6.f), 13.f), FVector(L, 14.f, 26.f), Curb);
	}
	// Puddles (mirror-like)
	UMaterialInstanceDynamic* Pud = Decal(TEXT("/Game/Textures/T_puddle.T_puddle"), FLinearColor(0.6f, 0.65f, 0.75f), 0.04f, 1.f);
	for (int32 i = 0; i < 16; i++)
	{
		const float S = Rng.FRandRange(180.f, 380.f);
		Quad(FVector(Rng.FRandRange(500.f, StreetLength - 300.f), Rng.FRandRange(-StreetWidth * 0.45f, StreetWidth * 0.45f), 2.2f + 0.4f * (i % 3)),
			FVector2D(S, S * Rng.FRandRange(0.6f, 1.f)), Pud, FRotator(0, Rng.FRandRange(0, 360.f), 0));
	}
	// Old blood stains
	BloodMat = Decal(TEXT("/Game/Textures/T_blood.T_blood"), FLinearColor(0.45f, 0.02f, 0.02f), 0.25f);
	for (int32 i = 0; i < 10; i++)
	{
		const float S = Rng.FRandRange(90.f, 220.f);
		Quad(FVector(Rng.FRandRange(500.f, StreetLength - 400.f), Rng.FRandRange(-550.f, 550.f), 3.f + 0.3f * (i % 3)), FVector2D(S, S), BloodMat, FRotator(0, Rng.FRandRange(0, 360.f), 0));
	}
	// Invisible boundary walls
	UMaterialInstanceDynamic* Wall = Surface({ TEXT("concrete"), 200.f, FLinearColor(0.35f, 0.35f, 0.38f) });
	Box(FVector(-600.f, 0, 600.f), FVector(100.f, 4000.f, 1200.f), Wall);
	Box(FVector(Cx, -(StreetWidth * 0.5f + 1200.f), 600.f), FVector(L, 100.f, 1200.f), Wall);
	Box(FVector(Cx, StreetWidth * 0.5f + 1200.f, 600.f), FVector(L, 100.f, 1200.f), Wall);
}

void AFOWorldBuilder::BuildBuildings()
{
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		float X = -400.f; int32 Idx = 0;
		while (X < StreetLength + 600.f)
		{
			const float W = Rng.FRandRange(650.f, 1050.f);
			BuildBuilding(X, W, Side, Idx++);
			X += W + Rng.FRandRange(0.f, 40.f); // colonial blocks are contiguous
		}
	}
}

void AFOWorldBuilder::BuildBuilding(float X0, float W, int32 Side, int32 Index)
{
	// Oran colonial (French 1900s) facade: ochre/white plaster, tall shuttered windows, wrought-iron
	// balconies, cornices between floors, ground-floor shops with roller shutters and signboards.
	const int32 Floors = Rng.RandRange(2, 4);
	const float GroundH = 420.f, FloorH = 340.f;
	const float H = GroundH + Floors * FloorH + 60.f;
	const float D = Rng.FRandRange(700.f, 900.f);
	const float FaceY = Side * (StreetWidth * 0.5f + 300.f);           // facade plane
	const float Cy = FaceY + Side * D * 0.5f;
	const float Cx = X0 + W * 0.5f;
	const float Out = -Side; // direction from facade towards the street (unit, along Y)

	static const FLinearColor Plasters[6] = { {0.86f,0.80f,0.66f}, {0.92f,0.90f,0.84f}, {0.78f,0.66f,0.48f}, {0.70f,0.72f,0.70f}, {0.88f,0.74f,0.60f}, {0.62f,0.60f,0.58f} };
	const FLinearColor PC = Plasters[Rng.RandRange(0, 5)] * Rng.FRandRange(0.75f, 1.f);
	const bool bBrick = Rng.FRand() < 0.15f;
	UMaterialInstanceDynamic* WallMat = bBrick ? Surface({ TEXT("bricks"), 260.f, FLinearColor(0.6f, 0.45f, 0.4f) }) : Surface({ TEXT("plaster"), 280.f, PC });
	UMaterialInstanceDynamic* TrimMat = Surface({ TEXT("plaster"), 200.f, FLinearColor(0.9f, 0.88f, 0.84f) });
	UMaterialInstanceDynamic* Shutter = Flat(Rng.FRand() < 0.6f ? FLinearColor(0.16f, 0.30f, 0.42f) : FLinearColor(0.35f, 0.28f, 0.2f), 0.7f);
	UMaterialInstanceDynamic* Iron = Flat(FLinearColor(0.03f, 0.03f, 0.035f), 0.55f, 0.6f);
	UMaterialInstanceDynamic* Glass = UVTex(TEXT("/Game/Textures/T_window_lit.T_window_lit"), FLinearColor(1.f, 0.85f, 0.6f), 2.2f, false, 0.2f);
	UMaterialInstanceDynamic* Dark = UVTex(TEXT("/Game/Textures/T_window_dark.T_window_dark"), FLinearColor(0.5f, 0.55f, 0.65f), 0.f, false, 0.15f);
	UMaterialInstanceDynamic* Metal = Surface({ TEXT("metal"), 120.f, FLinearColor(0.45f, 0.47f, 0.45f) });

	// main volume
	Box(FVector(Cx, Cy, H * 0.5f), FVector(W, D, H), WallMat);
	// roof parapet + cornices
	Box(FVector(Cx, FaceY + Out * 12.f, H - 20.f), FVector(W + 10.f, 24.f, 40.f), TrimMat, FRotator::ZeroRotator, false);
	for (int32 f = 0; f <= Floors; f++)
	{
		const float Z = GroundH + f * FloorH;
		Box(FVector(Cx, FaceY + Out * 9.f, Z - 8.f), FVector(W + 6.f, 18.f, 16.f), TrimMat, FRotator::ZeroRotator, false);
	}
	// upper floors: windows, shutters, balconies
	const int32 NWin = FMath::Max(2, (int32)(W / 300.f));
	const float Pitch = W / NWin;
	const bool bBalconies = Rng.FRand() < 0.7f;
	for (int32 f = 0; f < Floors; f++)
	{
		const float Zb = GroundH + f * FloorH + 40.f; // window sill
		const float WinH = 210.f, WinW = 110.f;
		for (int32 w = 0; w < NWin; w++)
		{
			const float Wx = X0 + Pitch * (w + 0.5f);
			const bool bLit = Rng.FRand() < 0.28f;
			const bool bShut = !bLit && Rng.FRand() < 0.45f;
			// glass / closed shutter panel flush with facade
			Quad(FVector(Wx, FaceY + Out * 1.5f, Zb + WinH * 0.5f), FVector2D(WinW, WinH), bShut ? Shutter : (bLit ? Glass : Dark), FRotator(Side > 0 ? 90.f : -90.f, 0, 0));
			// frame (lintel + sill)
			Box(FVector(Wx, FaceY + Out * 6.f, Zb + WinH + 8.f), FVector(WinW + 30.f, 12.f, 16.f), TrimMat, FRotator::ZeroRotator, false);
			Box(FVector(Wx, FaceY + Out * 8.f, Zb - 6.f), FVector(WinW + 30.f, 16.f, 12.f), TrimMat, FRotator::ZeroRotator, false);
			// open shutters beside the window
			if (!bShut)
			{
				Box(FVector(Wx - WinW * 0.5f - 22.f, FaceY + Out * 4.f, Zb + WinH * 0.5f), FVector(40.f, 6.f, WinH), Shutter, FRotator::ZeroRotator, false);
				Box(FVector(Wx + WinW * 0.5f + 22.f, FaceY + Out * 4.f, Zb + WinH * 0.5f), FVector(40.f, 6.f, WinH), Shutter, FRotator::ZeroRotator, false);
			}
			if (bLit) Light(FVector(Wx, FaceY + Out * 60.f, Zb + WinH * 0.6f), FLinearColor(1.f, 0.72f, 0.42f), 24.f, 560.f);
			// wrought-iron balcony on the first two floors
			if (bBalconies && f < 2)
			{
				const float Bd = 70.f;
				Box(FVector(Wx, FaceY + Out * Bd * 0.5f, Zb - 4.f), FVector(WinW + 80.f, Bd, 10.f), TrimMat, FRotator::ZeroRotator, false);
				Box(FVector(Wx, FaceY + Out * (Bd - 2.f), Zb + 90.f), FVector(WinW + 80.f, 3.f, 3.f), Iron, FRotator::ZeroRotator, false); // handrail
				for (int32 b = 0; b < 7; b++)
					Box(FVector(Wx - (WinW + 80.f) * 0.5f + b * (WinW + 80.f) / 6.f, FaceY + Out * (Bd - 2.f), Zb + 45.f), FVector(2.5f, 2.5f, 90.f), Iron, FRotator::ZeroRotator, false);
			}
		}
	}
	// ground floor: shop fronts
	const int32 NShops = W > 850.f ? 2 : 1;
	const float ShopW = W / NShops;
	for (int32 s = 0; s < NShops; s++)
	{
		const float Sx = X0 + ShopW * (s + 0.5f);
		const bool bOpen = Rng.FRand() < 0.25f; // broken-in shop: dark interior
		UMaterialInstanceDynamic* Front = bOpen ? Flat(FLinearColor(0.01f, 0.01f, 0.012f), 0.9f) : Metal;
		Box(FVector(Sx, FaceY + Out * 3.f, 150.f), FVector(ShopW - 90.f, 6.f, 300.f), Front, FRotator::ZeroRotator, false); // roller shutter / dark opening
		// signboard with Arabic/French shop name
		const int32 SignIdx = Rng.RandRange(0, 11);
		const FString SignPath = FString::Printf(TEXT("/Game/Textures/Signs/T_sign_%02d.T_sign_%02d"), SignIdx, SignIdx);
		const bool bNeon = Rng.FRand() < 0.35f;
		Box(FVector(Sx, FaceY + Out * 10.f, 350.f), FVector(ShopW - 70.f, 20.f, 80.f), Flat(FLinearColor(0.08f, 0.07f, 0.06f), 0.8f), FRotator::ZeroRotator, false);
		Quad(FVector(Sx, FaceY + Out * 21.f, 350.f), FVector2D(ShopW - 80.f, 72.f), UVTex(*SignPath, FLinearColor::White, bNeon ? 1.6f : 0.15f, false, 0.6f), FRotator(Side > 0 ? 90.f : -90.f, 0, 0));
		if (bNeon) Light(FVector(Sx, FaceY + Out * 90.f, 330.f), FLinearColor(1.f, 0.8f, 0.55f), 36.f, 700.f);
		// awning on some shops
		if (Rng.FRand() < 0.4f)
			Box(FVector(Sx, FaceY + Out * 70.f, 305.f), FVector(ShopW - 100.f, 140.f, 6.f), Flat(FLinearColor(0.35f, 0.12f, 0.1f), 0.95f), FRotator(Side > 0 ? -18.f : 18.f, 0, 0), false);
		// pilaster between shops
		Box(FVector(X0 + ShopW * s, FaceY + Out * 12.f, GroundH * 0.5f), FVector(40.f, 24.f, GroundH), TrimMat, FRotator::ZeroRotator, false);
	}
	Box(FVector(X0 + W, FaceY + Out * 12.f, GroundH * 0.5f), FVector(40.f, 24.f, GroundH), TrimMat, FRotator::ZeroRotator, false);
	// drain pipe + AC units
	Cyl(FVector(X0 + 25.f, FaceY + Out * 10.f, 0.f), 6.f, H - 30.f, Iron);
	for (int32 a = 0; a < Rng.RandRange(1, 3); a++)
		Box(FVector(X0 + Rng.FRandRange(60.f, W - 60.f), FaceY + Out * 25.f, GroundH + Rng.RandRange(0, Floors - 1) * FloorH + 250.f), FVector(70.f, 40.f, 55.f), Metal, FRotator::ZeroRotator, false);
}

void AFOWorldBuilder::BuildStreetFurniture()
{
	UMaterialInstanceDynamic* Iron = Flat(FLinearColor(0.04f, 0.04f, 0.045f), 0.5f, 0.7f);
	UMaterialInstanceDynamic* LampGlass = Flat(FLinearColor(1.f, 0.7f, 0.35f), 0.3f, 0.f, FLinearColor(1.f, 0.62f, 0.25f) * 6.f);
	// Sodium street lamps, alternating sides, every 20 m, all the way to the exit gate.
	// (v2.0 hard-coded 5 lamps: level 2 had one behind the exit wall and level 3's last 20 m were unlit.)
	for (int32 i = 0; 900.f + i * 2000.f < StreetLength - 300.f; i++)
	{
		const float X = 900.f + i * 2000.f;
		const int32 Side = (i % 2 == 0) ? 1 : -1;
		const float Y = Side * (StreetWidth * 0.5f + 70.f);
		Cyl(FVector(X, Y, 0), 9.f, 720.f, Iron, FRotator::ZeroRotator, true);
		Cyl(FVector(X, Y, 715.f), 6.f, 220.f, Iron, FRotator(0, 0, Side * 85.f)); // arm towards the street
		const FVector Head(X, Y - Side * 215.f, 730.f);
		Box(Head, FVector(60.f, 30.f, 22.f), Iron, FRotator::ZeroRotator, false);
		Box(Head - FVector(0, 0, 14.f), FVector(50.f, 22.f, 8.f), LampGlass, FRotator::ZeroRotator, false);
		UPointLightComponent* L = Light(Head - FVector(0, 0, 40.f), FLinearColor(1.f, 0.66f, 0.3f), BaseLamp, 2200.f, true);
		Lamps.Add(L);
		LampFlicker.Add(Rng.FRand() < 0.5f ? Rng.FRandRange(0.5f, 1.f) : 0.f);
	}
	// Hanging cables across the street (three-segment sag), all the way down the street
	for (int32 i = 0; 700.f + i * 1450.f < StreetLength - 500.f; i++)
	{
		const float X = 700.f + i * 1450.f + Rng.FRandRange(-200.f, 200.f);
		const float Z0 = Rng.FRandRange(640.f, 820.f), Sag = Rng.FRandRange(60.f, 120.f);
		const float Y0 = -(StreetWidth * 0.5f + 300.f), Y1 = StreetWidth * 0.5f + 300.f;
		FVector P[4] = { FVector(X, Y0, Z0), FVector(X, Y0 * 0.33f, Z0 - Sag * 0.85f), FVector(X, Y1 * 0.33f, Z0 - Sag * 0.85f), FVector(X, Y1, Z0) };
		for (int32 s = 0; s < 3; s++)
		{
			const FVector Dir = P[s + 1] - P[s];
			Cyl(P[s], 1.4f, Dir.Size(), Iron, FRotationMatrix::MakeFromZ(Dir).Rotator());
		}
	}
	// Palms along the sidewalks (Oran boulevard look)
	for (int32 i = 0; 800.f + i * 1500.f < StreetLength - 600.f; i++)
	{
		const int32 Side = (i % 2 == 0) ? 1 : -1;
		BuildPalm(FVector(800.f + i * 1500.f + Rng.FRandRange(-200.f, 200.f), Side * (StreetWidth * 0.5f + 230.f), 24.f));
	}
	// CC0 props from the Godot build (traffic lights, road signs, street lights)
	Prop(TEXT("/Game/Props/TrafficLight.TrafficLight"), FVector(1400.f, -(StreetWidth * 0.5f + 120.f), 24.f), 0.f, 460.f);
	Prop(TEXT("/Game/Props/TrafficLight_2.TrafficLight_2"), FVector(5800.f, StreetWidth * 0.5f + 120.f, 24.f), 180.f, 460.f);
	const TCHAR* Signs[3] = { TEXT("/Game/Props/Sign_Stop.Sign_Stop"), TEXT("/Game/Props/Sign_NoParking.Sign_NoParking"), TEXT("/Game/Props/Sign_Triangle.Sign_Triangle") };
	for (int32 i = 0; 1000.f + i * 1300.f < StreetLength - 600.f; i++)
	{
		const int32 Side = (i % 2 == 0) ? -1 : 1;
		Prop(Signs[i % 3], FVector(1000.f + i * 1300.f, Side * (StreetWidth * 0.5f + 160.f), 24.f), Rng.FRandRange(-30.f, 30.f) + (Side > 0 ? 180.f : 0.f), 260.f);
	}
	// Dumpsters
	UMaterialInstanceDynamic* Metal = Surface({ TEXT("metal"), 120.f, FLinearColor(0.3f, 0.4f, 0.32f) });
	for (int32 i = 0; i < 4; i++)
	{
		const int32 Side = Rng.FRand() < 0.5f ? -1 : 1;
		Box(FVector(Rng.FRandRange(1200.f, StreetLength - 800.f), Side * (StreetWidth * 0.5f + 90.f), 60.f), FVector(190.f, 100.f, 120.f), Metal, FRotator(0, Rng.FRandRange(-10.f, 10.f), 0));
	}
}

void AFOWorldBuilder::BuildPalm(const FVector& Base)
{
	UMaterialInstanceDynamic* Trunk = Surface({ TEXT("bricks"), 80.f, FLinearColor(0.28f, 0.22f, 0.16f), FLinearColor::Black, 1.f });
	UMaterialInstanceDynamic* Frond = UVTex(TEXT("/Game/Textures/T_palm_frond.T_palm_frond"), FLinearColor(0.35f, 0.45f, 0.25f), 0.f, true, 0.9f);
	const float H = Rng.FRandRange(450.f, 620.f);
	const float Lean = Rng.FRandRange(-6.f, 6.f);
	FVector P = Base;
	const int32 Segs = 5;
	for (int32 i = 0; i < Segs; i++)
	{
		const float SegH = H / Segs;
		const FRotator R(Lean * (i + 1) / Segs, 0, 0);
		Cyl(P, 18.f - i * 2.2f, SegH + 6.f, Trunk, R, i == 0);
		P += R.RotateVector(FVector(0, 0, SegH));
	}
	for (int32 f = 0; f < 9; f++)
	{
		const FRotator R(Rng.FRandRange(-38.f, -14.f), f * 40.f + Rng.FRandRange(-14.f, 14.f), 0);
		Quad(P + R.RotateVector(FVector(155.f, 0, 0)), FVector2D(340.f, 150.f), Frond, R);
	}
}

void AFOWorldBuilder::BuildCars()
{
	const TCHAR* Cars[5] = { TEXT("/Game/Props/car_NormalCar1.car_NormalCar1"), TEXT("/Game/Props/car_NormalCar2.car_NormalCar2"), TEXT("/Game/Props/car_SUV.car_SUV"), TEXT("/Game/Props/car_Taxi.car_Taxi"), TEXT("/Game/Props/car_Cop.car_Cop") };
	UMaterialInstanceDynamic* Burnt = Surface({ TEXT("metal"), 90.f, FLinearColor(0.12f, 0.11f, 0.1f), FLinearColor::Black, 1.3f });
	for (int32 i = 0; i < 9; i++)
	{
		const float X = Rng.FRandRange(700.f, StreetLength - 800.f);
		const float Y = Rng.FRandRange(-StreetWidth * 0.35f, StreetWidth * 0.35f);
		bool bNearPuzzle = false;
		for (const FFOPuzzleDef& P : LevelDef->Puzzles)
			if (FMath::Abs(X - P.Location.X) < 1000.f) { bNearPuzzle = true; break; }
		if (bNearPuzzle) continue; // keep objective approaches free of parked-car collisions
		const bool bBurnt = Rng.FRand() < 0.35f;
		UStaticMeshComponent* C = Prop(Cars[i % 5], FVector(X, Y, 0), Rng.FRandRange(0, 360.f), 150.f, bBurnt ? Burnt : nullptr);
		if (C && bBurnt) Light(FVector(X, Y, 90.f), FLinearColor(1.f, 0.35f, 0.08f), 6.f, 300.f); // embers
	}
}

void AFOWorldBuilder::BuildDebris()
{
	UMaterialInstanceDynamic* Concrete = Surface({ TEXT("concrete"), 120.f, FLinearColor(0.45f, 0.42f, 0.4f) });
	UMaterialInstanceDynamic* Bags = Flat(FLinearColor(0.02f, 0.02f, 0.025f), 0.45f);
	UMaterialInstanceDynamic* Cone = Flat(FLinearColor(0.9f, 0.3f, 0.05f), 0.6f);
	UMaterialInstanceDynamic* Sand = Surface({ TEXT("ground"), 60.f, FLinearColor(0.6f, 0.52f, 0.4f) });
	// rubble
	for (int32 i = 0; i < 26; i++)
	{
		const float S = Rng.FRandRange(25.f, 80.f);
		const FVector Loc(Rng.FRandRange(500.f, StreetLength - 300.f), Rng.FRandRange(-600.f, 600.f), S * 0.3f);
		bool bNearPuzzle = false;
		for (const FFOPuzzleDef& P : LevelDef->Puzzles)
			if (FMath::Abs(Loc.X - P.Location.X) < 1000.f) { bNearPuzzle = true; break; }
		if (!bNearPuzzle) Box(Loc, FVector(S, S * 0.8f, S * 0.6f), Concrete, FRotator(Rng.FRandRange(-10, 10), Rng.FRandRange(0, 360.f), Rng.FRandRange(-10, 10)));
	}
	// trash bags on the sidewalks
	for (int32 i = 0; i < 14; i++)
	{
		const int32 Side = Rng.FRand() < 0.5f ? -1 : 1;
		UStaticMeshComponent* B = NewSMC(this, SphereMesh, Bags, false);
		Place(B, FTransform(FRotator(0, Rng.FRandRange(0, 360.f), 0), FVector(Rng.FRandRange(600.f, StreetLength - 400.f), Side * (StreetWidth * 0.5f + Rng.FRandRange(60.f, 240.f)), 45.f), FVector(0.75f, 0.6f, 0.5f)));
	}
	// traffic cones
	for (int32 i = 0; i < 6; i++)
	{
		UStaticMeshComponent* C = NewSMC(this, ConeMesh, Cone, false);
		Place(C, FTransform(FRotator(0, 0, Rng.FRand() < 0.3f ? 90.f : 0.f), FVector(Rng.FRandRange(800.f, StreetLength - 600.f), Rng.FRandRange(-500.f, 500.f), 24.f), FVector(0.45f, 0.45f, 0.7f)));
	}
	// Army checkpoint: sandbag wall + concrete barriers halfway down the street
	const float Cx = FMath::RoundToFloat(StreetLength * 0.5f); // halfway, whatever the level length
	for (int32 r = 0; r < 3; r++)
		for (int32 c = 0; c < 7 - r; c++)
		{
			UStaticMeshComponent* S = NewSMC(this, SphereMesh, Sand, true);
			Place(S, FTransform(FRotator::ZeroRotator, FVector(Cx + (r % 2) * 20.f, -450.f + c * 60.f + r * 30.f, 22.f + r * 34.f), FVector(0.8f, 0.6f, 0.4f)));
		}
	Box(FVector(Cx + 200.f, 380.f, 45.f), FVector(90.f, 260.f, 90.f), Concrete, FRotator(0, 12.f, 0));
	Box(FVector(Cx + 60.f, 120.f, 45.f), FVector(90.f, 260.f, 90.f), Concrete, FRotator(0, -25.f, 0));
}

void AFOWorldBuilder::BuildExitGate()
{
	const float Gx = StreetLength + 200.f;
	UMaterialInstanceDynamic* Dark = Flat(FLinearColor(0.05f, 0.05f, 0.05f), 0.6f, 0.5f);
	UMaterialInstanceDynamic* Panel = Flat(FLinearColor(0.03f, 0.06f, 0.04f), 0.6f, 0.f, FLinearColor(0.05f, 0.28f, 0.1f) * 1.2f);
	Box(FVector(Gx, 0, 500.f), FVector(40.f, 450.f, 20.f), Dark);
	Box(FVector(Gx, -220.f, 250.f), FVector(40.f, 40.f, 500.f), Dark);
	Box(FVector(Gx, 220.f, 250.f), FVector(40.f, 40.f, 500.f), Dark);
	Box(FVector(Gx, 0, 230.f), FVector(15.f, 380.f, 460.f), Panel, FRotator::ZeroRotator, false);
	// PORT sign
	Quad(FVector(Gx - 30.f, 0, 560.f), FVector2D(420.f, 70.f), UVTex(TEXT("/Game/Textures/Signs/T_sign_port.T_sign_port"), FLinearColor::White, 1.2f), FRotator(90.f, 0, 0));
	Light(FVector(Gx - 150.f, 0, 250.f), FLinearColor(0.2f, 1.f, 0.4f), 12.f, 900.f);
	ExitTrigger = NewObject<UBoxComponent>(this);
	ExitTrigger->SetupAttachment(RootComponent);
	ExitTrigger->SetBoxExtent(FVector(120.f, 250.f, 200.f));
	ExitTrigger->SetWorldLocation(FVector(Gx - 120.f, 0, 150.f));
	ExitTrigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	ExitTrigger->OnComponentBeginOverlap.AddDynamic(this, &AFOWorldBuilder::OnExitOverlap);
	ExitTrigger->RegisterComponent();
	// Wall behind the gate so nobody walks past it
	Box(FVector(Gx + 60.f, 0, 600.f), FVector(60.f, 4000.f, 1200.f), Surface({ TEXT("concrete"), 200.f, FLinearColor(0.35f, 0.35f, 0.38f) }));
}

void AFOWorldBuilder::ApplyBrightness(float Mul)
{
	Bright = FMath::Clamp(Mul, 0.25f, 4.f);
	if (Moon) Moon->GetLightComponent()->SetIntensity(BaseMoon * Bright);
	if (Sky) Sky->GetLightComponent()->SetIntensity(BaseSky * Bright);
	for (UPointLightComponent* L : Lamps) if (L) L->SetIntensity(BaseLamp * Bright);
	UE_LOG(LogFO, Display, TEXT("Brightness x%.2f"), Bright);
}

void AFOWorldBuilder::OnExitOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (Cast<AFOCharacter>(Other))
		if (AFOGameMode* GM = GetWorld()->GetAuthGameMode<AFOGameMode>()) GM->ReportEvent(FFOGameEvent(EFOGameEvent::ExitReached));
}

void AFOWorldBuilder::SpawnItems()
{
	if (!LevelDef) return;
	for (const FFOItemSpawn& It : LevelDef->Items)
	{
		if (It.Item == EFOItem::Note)
		{
			if (AFONote* N = GetWorld()->SpawnActor<AFONote>(AFONote::StaticClass(), It.Location, FRotator(0, Rng.FRandRange(0, 360.f), 0)))
			{ N->Text = It.Text; if (!It.Tag.IsNone()) N->Tag = It.Tag; }
			continue;
		}
		if (AFOPickup* P = GetWorld()->SpawnActorDeferred<AFOPickup>(AFOPickup::StaticClass(), FTransform(It.Location)))
		{
			P->Item = It.Item;
			P->Tag = It.Tag.IsNone() ? FName(It.Item == EFOItem::Fuel ? TEXT("fuel") : It.Item == EFOItem::Health ? TEXT("health") : TEXT("ammo")) : It.Tag;
			UGameplayStatics::FinishSpawningActor(P, FTransform(It.Location));
		}
	}
}

void AFOWorldBuilder::SpawnPuzzles()
{
	if (!LevelDef) return;
	for (const FFOPuzzleDef& PD : LevelDef->Puzzles)
	{
		UClass* Cls = nullptr;
		switch (PD.Type)
		{
		case EFOPuzzleType::BreakerSequence: Cls = AFOBreakerPuzzle::StaticClass(); break;
		case EFOPuzzleType::Keypad:          Cls = AFOKeypadPuzzle::StaticClass(); break;
		case EFOPuzzleType::Generator:       Cls = AFOGeneratorPuzzle::StaticClass(); break;
		}
		if (!Cls) continue;
		AFOPuzzleBase* P = GetWorld()->SpawnActor<AFOPuzzleBase>(Cls, PD.Location, FRotator(0, PD.Yaw, 0));
		if (!P) continue;
		P->Setup(PD, Rng);
		// Notes carrying this puzzle's hints
		for (int32 i = 0; i < PD.NoteLocations.Num(); i++)
			if (AFONote* N = GetWorld()->SpawnActor<AFONote>(AFONote::StaticClass(), PD.NoteLocations[i], FRotator(0, Rng.FRandRange(0, 360.f), 0)))
				N->Text = P->GetHintText(i);
		UE_LOG(LogFO, Display, TEXT("Puzzle '%s' spawned with %d notes"), *PD.Id.ToString(), PD.NoteLocations.Num());
	}
}

void AFOWorldBuilder::SpawnZombies()
{
	if (!LevelDef) return;
	// Seeded, evenly spread along the street (never in the first 15 m so the player gets a breath).
	const int32 N = LevelDef->ZombieCount;
	for (int32 i = 0; i < N; i++)
	{
		const float Frac = (i + 0.5f) / N;
		const float X = FMath::Lerp(1500.f, StreetLength - 300.f, Frac) + Rng.FRandRange(-300.f, 300.f);
		const FVector Loc(X, Rng.FRandRange(-450.f, 450.f), 100.f);
		const FTransform T(FRotator(0, Rng.FRandRange(0, 360.f), 0), Loc);
		if (AFOZombie* Z = GetWorld()->SpawnActorDeferred<AFOZombie>(AFOZombie::StaticClass(), T))
		{
			Z->Variant = i % 4;
			Z->bRunner = Rng.FRand() < LevelDef->RunnerChance;
			Z->HpMul = LevelDef->ZombieHpMul;
			UGameplayStatics::FinishSpawningActor(Z, T);
		}
	}
}

void AFOWorldBuilder::SpawnBloodPool(const FVector& Loc)
{
	if (!BloodMat || BloodCount > 60) return;
	BloodCount++;
	UStaticMeshComponent* Q = Quad(FVector(Loc.X, Loc.Y, 4.5f + 0.1f * (BloodCount % 5)), FVector2D(170.f, 170.f), BloodMat, FRotator(0, Rng.FRandRange(0, 360.f), 0));
	Q->SetMobility(EComponentMobility::Movable);
	Q->SetWorldScale3D(FVector(0.2f, 0.2f, 1.f));
	GrowPools.Add(Q);
}

void AFOWorldBuilder::SpawnBloodSplat(const FVector& Loc, const FVector& Normal)
{
	if (!BloodMat || BloodCount > 60) return;
	BloodCount++;
	const FRotator R = FRotationMatrix::MakeFromZ(Normal).Rotator();
	Quad(Loc + Normal * 1.5f, FVector2D(60.f, 60.f), BloodMat, R);
}

void AFOWorldBuilder::Tick(float Dt)
{
	Super::Tick(Dt);
	// A puzzle can unlock the exit while the player is already inside the box;
	// BeginOverlap alone would require leaving and re-entering to finish.
	if (ExitTrigger)
		if (AFOGameMode* GM = GetWorld()->GetAuthGameMode<AFOGameMode>())
			if (GM->State == EFOState::Playing && GM->Mission && GM->Mission->IsExitOpen())
				if (AFOCharacter* P = GM->Player())
					if (ExitTrigger->IsOverlappingActor(P))
						GM->ReportEvent(FFOGameEvent(EFOGameEvent::ExitReached));
	// Blood pools spread (own list: scanning the ~3000 world components every frame was a phone CPU hog)
	for (int32 i = GrowPools.Num() - 1; i >= 0; i--)
	{
		UStaticMeshComponent* S = GrowPools[i];
		if (!S) { GrowPools.RemoveAtSwap(i); continue; }
		const FVector Sc = S->GetComponentScale();
		if (Sc.X < 1.7f) S->SetWorldScale3D(FVector(Sc.X + Dt * 0.5f, Sc.Y + Dt * 0.5f, 1.f));
		else GrowPools.RemoveAtSwap(i); // fully spread, stop ticking it
	}
	// Lamp flicker
	const float T = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < Lamps.Num(); i++)
		if (IsValid(Lamps[i]) && LampFlicker.IsValidIndex(i) && LampFlicker[i] > 0.f)
		{
			const float N = FMath::PerlinNoise1D(T * 6.f + i * 13.7f);
			const float F = N > 0.55f * LampFlicker[i] ? 0.15f : 1.f;
			Lamps[i]->SetIntensity(BaseLamp * Bright * F);
		}
	// Lightning: flash the moon, thunder follows
	if (LevelDef && !LevelDef->Lighting.bLightning) LightningT = 1e9f;
	LightningT -= Dt;
	if (LightningT <= 0.f && Moon)
	{
		LightningT = Rng.FRandRange(9.f, 22.f);
		Moon->GetLightComponent()->SetIntensity(BaseMoon * Bright * 4.f);
		Moon->GetLightComponent()->SetLightColor(FLinearColor(0.8f, 0.85f, 1.f));
		ThunderDelay = Rng.FRandRange(0.5f, 1.4f);
	}
	else if (Moon && Moon->GetLightComponent()->Intensity > BaseMoon * Bright + 0.01f)
	{
		Moon->GetLightComponent()->SetIntensity(FMath::Max(BaseMoon * Bright, Moon->GetLightComponent()->Intensity - Dt * BaseMoon * 7.f));
		// restore the LEVEL's moon colour (v2.0 hard-coded the cold boulevard tint, so level 2's warm moon turned blue after the first flash)
		if (Moon->GetLightComponent()->Intensity <= BaseMoon * Bright + 0.05f) Moon->GetLightComponent()->SetLightColor(LevelDef ? LevelDef->Lighting.MoonColor : FLinearColor(0.6f, 0.7f, 0.92f));
	}
	if (ThunderDelay >= 0.f)
	{
		ThunderDelay -= Dt;
		if (ThunderDelay < 0.f && ThunderSound) UGameplayStatics::PlaySound2D(this, ThunderSound, 0.9f, Rng.FRandRange(0.8f, 1.1f));
	}
}
