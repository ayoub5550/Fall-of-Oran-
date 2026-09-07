#include "FOChallengeRegistry.h"

static FFOSurvivalWave Wave(int32 Enemies, int32 MaxLive, float RunnerChance, float HeavyChance, float HpMul, float SpawnInterval, float Rest, const TCHAR* Banner)
{
	FFOSurvivalWave W;
	W.Enemies = Enemies; W.MaxLive = MaxLive; W.RunnerChance = RunnerChance; W.HeavyChance = HeavyChance;
	W.HpMul = HpMul; W.SpawnInterval = SpawnInterval; W.RestSeconds = Rest; W.Banner = Banner;
	return W;
}

static TArray<FFOChallengeDef> BuildChallenges()
{
	TArray<FFOChallengeDef> Out;

	// ------------------------------------------------------------ 1. Survival: five bounded waves
	{
		FFOChallengeDef C;
		C.Id = TEXT("survival_five");
		C.Mode = EFOFlowMode::Survival;
		C.Title = TEXT("الصمود: خمس موجات");
		C.Intro = TEXT("ساحة صغيرة محصّنة قرب المرسى. خمس موجات فقط — بينها هدنة قصيرة للتزوّد.");
		C.Rules = TEXT("اصمد أمام 5 موجات. الموجة تُحتسب فقط عندما يموت كل أعدائها فعليًا. بين الموجات تصل ذخيرة وإسعافات. الموت = إعادة المحاولة، ولا يؤثر على الحملة.");
		C.OutroText = TEXT("صمدت أمام الموجات الخمس. الساحة آمنة حتى الفجر.");
		C.Seed = 51001;
		C.StreetLength = 3000.f;              // compact arena: no 90 m empty walk
		C.SafeSpawnRadius = 1100.f;
		C.Lighting.FogDensity = 0.017f;
		C.Lighting.FogColor = FLinearColor(0.05f, 0.06f, 0.10f);
		C.ResupplyPoints = { FVector(1200, -320, 45), FVector(1500, 330, 45), FVector(1850, -280, 45), FVector(900, 300, 45), FVector(2200, 300, 45), FVector(700, -300, 45) };
		// Escalation: count, simultaneous pressure, speed, then durability.
		C.Waves.Add(Wave(4,  3, 0.00f, 0.00f, 1.00f, 1.6f, 14.f, TEXT("الموجة 1/5 — شاردون بطيئون")));
		C.Waves.Add(Wave(6,  4, 0.20f, 0.15f, 1.00f, 1.4f, 13.f, TEXT("الموجة 2/5 — أسرع، وأول مصاب بزي الشرطة")));
		C.Waves.Add(Wave(8,  5, 0.35f, 0.25f, 1.10f, 1.2f, 12.f, TEXT("الموجة 3/5 — ضغط مزدوج")));
		C.Waves.Add(Wave(10, 6, 0.50f, 0.30f, 1.20f, 1.0f, 12.f, TEXT("الموجة 4/5 — عدّاؤون كثر")));
		C.Waves.Add(Wave(12, 7, 0.55f, 0.40f, 1.35f, 0.9f,  0.f, TEXT("الموجة الأخيرة 5/5 — اصمد!")));
		C.FirstWaveDelay = 5.f;
		Out.Add(C);
	}

	// ------------------------------------------------------------ 2. Supply Run: timed collect + extract
	{
		FFOChallengeDef C;
		C.Id = TEXT("supply_run");
		C.Mode = EFOFlowMode::SupplyRun;
		C.Title = TEXT("خط الإمداد");
		C.Intro = TEXT("أربع صناديق إمداد موزّعة في السوق. اجمعها كلها ثم اصعد إلى نقطة الإخلاء قبل نهاية الوقت.");
		C.Rules = TEXT("اجمع 4 صناديق إمداد ثم اصل فعليًا إلى نقطة الإخلاء قبل انتهاء المؤقّت. لا تُحتسب المهمة بمجرد الجمع — لا بد من الوصول. انتهاء الوقت أو الموت = فشل، دون أي تأثير على الحملة.");
		C.OutroText = TEXT("وصلت الإمدادات إلى نقطة الإخلاء في الوقت المحدّد.");
		C.Seed = 51002;
		C.StreetLength = 4000.f;
		C.SafeSpawnRadius = 1200.f;
		C.Lighting.FogDensity = 0.015f;
		C.Lighting.FogColor = FLinearColor(0.07f, 0.07f, 0.11f);
		C.SupplyPoints = { FVector(900, -380, 55), FVector(1700, 400, 55), FVector(2500, -400, 55), FVector(3300, 360, 55) };
		C.SupplyTarget = 4;
		C.TimeLimitSeconds = 210.f;
		C.ResupplyPoints = { FVector(1300, 300, 45), FVector(2900, -300, 45) };
		C.AmbientLive = 5;
		C.AmbientRunnerChance = 0.35f;
		C.AmbientHpMul = 1.f;
		C.AmbientRespawnSeconds = 7.f;
		Out.Add(C);
	}
	return Out;
}

const TArray<FFOChallengeDef>& FFOChallengeRegistry::All()
{
	static const TArray<FFOChallengeDef> Defs = BuildChallenges();
	return Defs;
}
const FFOChallengeDef* FFOChallengeRegistry::Get(int32 Index) { return All().IsValidIndex(Index) ? &All()[Index] : nullptr; }
const FFOChallengeDef* FFOChallengeRegistry::Find(FName Id) { for (const FFOChallengeDef& C : All()) if (C.Id == Id) return &C; return nullptr; }
const FFOChallengeDef* FFOChallengeRegistry::FindByMode(EFOFlowMode Mode) { for (const FFOChallengeDef& C : All()) if (C.Mode == Mode) return &C; return nullptr; }

FFOLevelDef FFOChallengeRegistry::MakeLevelDef(const FFOChallengeDef& Def)
{
	FFOLevelDef L;
	L.Id = Def.Id;
	L.Title = Def.Title;
	L.Intro = Def.Intro;
	L.Seed = Def.Seed;
	L.StreetLength = Def.StreetLength;
	L.ZombieCount = 0;          // UFOChallengeComponent owns every enemy spawn (capped + fair)
	L.RunnerChance = 0.f;
	L.ZombieHpMul = 1.f;
	L.Lighting = Def.Lighting;
	L.OutroText = Def.OutroText;
	L.bChallengeArena = true;
	// No Stages: the challenge component, not UFOMissionComponent, owns the rules.
	// Survival: the component drops (and refreshes) the resupply crates at every wave break, so the
	// world must NOT pre-place duplicates on the same points. Supply Run has no wave breaks, so its
	// two support crates are placed once with the level.
	for (int32 i = 0; Def.Mode == EFOFlowMode::SupplyRun && i < Def.ResupplyPoints.Num(); i++)
	{
		FFOItemSpawn S;
		S.Item = (i % 2 == 0) ? EFOItem::Ammo : EFOItem::Health;
		S.Location = Def.ResupplyPoints[i];
		S.Tag = (i % 2 == 0) ? FName(TEXT("ammo")) : FName(TEXT("health"));
		L.Items.Add(S);
	}
	if (Def.Mode == EFOFlowMode::SupplyRun)
	{
		for (const FVector& V : Def.SupplyPoints)
		{
			FFOItemSpawn S;
			S.Item = EFOItem::Supply;
			S.Location = V;
			S.Tag = TEXT("supply");   // deliberately NOT "fuel" (that tag triggers the campaign ambush)
			L.Items.Add(S);
		}
	}
	return L;
}
