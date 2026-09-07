#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/FOTypes.h"
#include "FOWorldBuilder.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UTexture;
class UPointLightComponent;
class USoundBase;
class UAudioComponent;
class ADirectionalLight;
class UBoxComponent;

/** Textured surface description for the world-aligned PBR master material. */
struct FFOSurface
{
	FName Tex;                 // texture family name, e.g. "plaster" → T_plaster_color / _normal / _rough
	float TileCm = 300.f;      // world-aligned tile size in cm
	FLinearColor Tint = FLinearColor::White;
	FLinearColor Emissive = FLinearColor::Black;
	float RoughMul = 1.f;
};

/**
 * Builds the whole Oran street at runtime from primitive meshes + PBR materials
 * (no hand-authored level needed → everything is reproducible headless).
 * World axes: street runs along +X (0 .. StreetLength), Y across, Z up. Units cm.
 */
UCLASS()
class AFOWorldBuilder : public AActor
{
	GENERATED_BODY()
public:
	AFOWorldBuilder();
	static AFOWorldBuilder* Get(UWorld* W);
	float StreetLength = 9000.f;   // set from FFOLevelDef
	static constexpr float StreetWidth = 1200.f;

	/** Builds the level described by Def: geometry (seeded), lighting preset, items, puzzles, notes, zombies. */
	void BuildWorld(const FFOLevelDef& Def);
	const FFOLevelDef* LevelDef = nullptr;
	void SpawnBloodPool(const FVector& Loc);
	void SpawnBloodSplat(const FVector& Loc, const FVector& Normal);
	virtual void Tick(float Dt) override;

	// --- primitives ---
	UStaticMeshComponent* Box(const FVector& Center, const FVector& Size, UMaterialInterface* Mat, const FRotator& Rot = FRotator::ZeroRotator, bool bCollide = true);
	UStaticMeshComponent* Cyl(const FVector& Base, float Radius, float Height, UMaterialInterface* Mat, const FRotator& Rot = FRotator::ZeroRotator, bool bCollide = false);
	UStaticMeshComponent* Quad(const FVector& Center, const FVector2D& Size, UMaterialInterface* Mat, const FRotator& Rot, bool bCollide = false);
	UStaticMeshComponent* Prop(const TCHAR* Path, const FVector& Loc, float Yaw, float TargetHeightCm, UMaterialInterface* Override = nullptr);
	UMaterialInstanceDynamic* Surface(const FFOSurface& S);
	UMaterialInstanceDynamic* Flat(const FLinearColor& Tint, float Rough = 0.9f, float Metal = 0.f, const FLinearColor& Emis = FLinearColor::Black);
	UMaterialInstanceDynamic* UVTex(const TCHAR* TexPath, const FLinearColor& Tint = FLinearColor::White, float EmisMul = 0.f, bool bMasked = false, float Rough = 0.8f);
	UMaterialInstanceDynamic* Decal(const TCHAR* TexPath, const FLinearColor& Tint = FLinearColor::White, float Rough = 0.3f, float Metal = 0.f);
	UPointLightComponent* Light(const FVector& Loc, const FLinearColor& Color, float Intensity, float Radius, bool bShadow = false);

	// --- world pieces ---
	void BuildLighting();
	void BuildGround();
	void BuildBuildings();
	void BuildBuilding(float X0, float W, int32 Side, int32 Index);
	void BuildStreetFurniture();
	void BuildCars();
	void BuildDebris();
	void BuildPalm(const FVector& Base);
	void BuildExitGate();
	/** Compact authored challenge arena (survival / supply run): cover, resupply station, extraction pad. */
	void BuildChallengeArena();
	/** Invisible blocking box helper for imported props that ship without collision. */
	void Blocker(const FVector& Center, const FVector& Extent);
	void SpawnItems();
	void SpawnPuzzles();
	void SpawnZombies();
	void BuildSky();

	UPROPERTY() UStaticMesh* CubeMesh = nullptr;
	UPROPERTY() UStaticMesh* CylMesh = nullptr;
	UPROPERTY() UStaticMesh* PlaneMesh = nullptr;
	UPROPERTY() UStaticMesh* SphereMesh = nullptr;
	UPROPERTY() UStaticMesh* ConeMesh = nullptr;
	UPROPERTY() UMaterialInterface* M_PBR = nullptr;
	UPROPERTY() UMaterialInterface* M_Flat = nullptr;
	UPROPERTY() UMaterialInterface* M_UV = nullptr;
	UPROPERTY() UMaterialInterface* M_UVMasked = nullptr;
	UPROPERTY() UMaterialInterface* M_Decal = nullptr;
	UPROPERTY() UMaterialInterface* M_Sky = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* BloodMat = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> GrowPools; // spreading blood pools (ticked), see SpawnBloodPool
	UPROPERTY() TArray<UPointLightComponent*> Lamps;
	TArray<float> LampFlicker;
	UPROPERTY() ADirectionalLight* Moon = nullptr;
	UPROPERTY() class ASkyLight* Sky = nullptr;
	// Light budget. Phones without eye adaptation render MUCH brighter than the sandbox CPU driver,
	// so keep bases low and let the player scale them (ApplyBrightness, persisted in the save game).
	float BaseMoon = 2.0f;
	float BaseSky = 1.2f;
	float BaseLamp = 320.f;
	float Bright = 1.f;
	void ApplyBrightness(float Mul);
	float LightningT = 7.f;
	float ThunderDelay = -1.f;
	UPROPERTY() USoundBase* ThunderSound = nullptr;
	UPROPERTY() UAudioComponent* Ambience = nullptr;
	UPROPERTY() UBoxComponent* ExitTrigger = nullptr;
	FRandomStream Rng;
	int32 BloodCount = 0;

	UFUNCTION() void OnExitOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
};
