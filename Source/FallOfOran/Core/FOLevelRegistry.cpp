#include "FOLevelRegistry.h"

// Helpers keep the level table readable.
static FFOObjectiveDef Obj(EFOObjectiveType T, FName Tag, int32 Count, const TCHAR* Text, const TCHAR* Start = TEXT(""), const TCHAR* Done = TEXT(""))
{
	FFOObjectiveDef O; O.Type = T; O.Tag = Tag; O.Count = Count; O.Text = Text; O.StartHint = Start; O.DoneHint = Done; return O;
}
static FFOMissionStage Stage(std::initializer_list<FFOObjectiveDef> Objs) { FFOMissionStage S; for (const auto& O : Objs) S.Objectives.Add(O); return S; }
static FFOItemSpawn Item(EFOItem I, FVector Loc, FName Tag = NAME_None, const TCHAR* Text = TEXT("")) { FFOItemSpawn S; S.Item = I; S.Location = Loc; S.Tag = Tag; S.Text = Text; return S; }

static void AddSupplies(FFOLevelDef& L, std::initializer_list<FVector> Health, std::initializer_list<FVector> Ammo)
{
	for (const FVector& V : Health) L.Items.Add(Item(EFOItem::Health, V));
	for (const FVector& V : Ammo) L.Items.Add(Item(EFOItem::Ammo, V));
}

static TArray<FFOLevelDef> BuildCampaign()
{
	TArray<FFOLevelDef> Levels;

	// ------------------------------------------------------------ 1. Boulevard
	{
		FFOLevelDef L;
		L.Id = TEXT("boulevard");
		L.Title = TEXT("شارع العربي بن مهيدي");
		L.Intro = TEXT("سقطت وهران. اجمع الوقود وافتح بوابة الميناء.");
		L.Seed = 20260906; L.StreetLength = 9000.f; L.ZombieCount = 16; L.RunnerChance = 0.2f;
		L.Items.Add(Item(EFOItem::Fuel, FVector(1800, -420, 50), TEXT("fuel")));
		L.Items.Add(Item(EFOItem::Fuel, FVector(4800, 400, 50), TEXT("fuel")));
		L.Items.Add(Item(EFOItem::Fuel, FVector(7800, -300, 50), TEXT("fuel")));
		AddSupplies(L, { {2800, 250, 40}, {5800, -450, 40}, {7200, 350, 40} },
		               { {1400, -200, 40}, {3800, 450, 40}, {4400, -400, 40}, {6600, 150, 40} });
		L.Stages.Add(Stage({ Obj(EFOObjectiveType::Collect, TEXT("fuel"), 3, TEXT("اجمع عبوات الوقود {n}/{max} ⛽"), TEXT("الهدف: اجمع 3 عبوات وقود لفتح بوابة الميناء"), TEXT("البوابة فُتحت! اهرب إلى الميناء 🟢")) }));
		L.Stages.Add(Stage({ Obj(EFOObjectiveType::Reach, NAME_None, 1, TEXT("اهرب إلى بوابة الميناء 🟢")) }));
		L.OutroText = TEXT("عبرت البوابة… لكن الميناء ليس آمنًا بعد.");
		Levels.Add(L);
	}

	// ------------------------------------------------------------ 2. Power station
	{
		FFOLevelDef L;
		L.Id = TEXT("power");
		L.Title = TEXT("محطة كهرباء سيدي الهواري");
		L.Intro = TEXT("البوابة الكهربائية معطّلة. أعد التيار بترتيب القواطع الصحيح.");
		L.Seed = 7712; L.StreetLength = 8000.f; L.ZombieCount = 20; L.RunnerChance = 0.3f;
		L.Lighting.FogColor = FLinearColor(0.10f, 0.06f, 0.05f); L.Lighting.FogDensity = 0.016f;
		L.Lighting.MoonColor = FLinearColor(0.7f, 0.62f, 0.55f);
		FFOPuzzleDef P; P.Type = EFOPuzzleType::BreakerSequence; P.Id = TEXT("breakers");
		P.Location = FVector(7300, -480, 0); P.Yaw = -90.f; // front (-X local) faces +Y = the street
		P.Size = 4; P.PenaltyZombies = 2;
		P.NoteLocations = { FVector(2600, 380, 60), FVector(5200, -420, 60) }; // two copies of the same hint
		L.Puzzles.Add(P);
		AddSupplies(L, { {3200, -300, 40}, {6400, 420, 40} },
		               { {1500, 300, 40}, {2900, -450, 40}, {4600, 450, 40}, {5900, -150, 40}, {7000, 300, 40} });
		L.Stages.Add(Stage({ Obj(EFOObjectiveType::Kill, NAME_None, 6, TEXT("طهّر الشارع {n}/{max} 🔫"), TEXT("الهدف: اقتل 6 زومبي ثم اعثر على ورقة ترتيب القواطع")) }));
		L.Stages.Add(Stage({ Obj(EFOObjectiveType::SolvePuzzle, TEXT("breakers"), 1, TEXT("أعد التيار: اضغط القواطع بالترتيب الصحيح ⚡"), TEXT("ابحث عن ملاحظة الترتيب قرب الجدران ثم اذهب إلى لوحة القواطع"), TEXT("عاد التيار! البوابة تعمل 🟢")) }));
		L.Stages.Add(Stage({ Obj(EFOObjectiveType::Reach, NAME_None, 1, TEXT("اعبر البوابة الكهربائية 🟢")) }));
		L.OutroText = TEXT("عاد النور إلى سيدي الهواري. الميناء أمامك.");
		Levels.Add(L);
	}

	// ------------------------------------------------------------ 3. Port
	{
		FFOLevelDef L;
		L.Id = TEXT("port");
		L.Title = TEXT("ميناء وهران");
		L.Intro = TEXT("آخر قارب. بوابة الحاويات مقفلة بشيفرة مكوّنة من 4 أرقام مبعثرة في الميناء.");
		L.Seed = 3391; L.StreetLength = 10000.f; L.ZombieCount = 24; L.RunnerChance = 0.4f; L.ZombieHpMul = 1.2f;
		L.Lighting.FogColor = FLinearColor(0.04f, 0.08f, 0.12f); L.Lighting.FogDensity = 0.02f;
		L.Lighting.MoonIntensity = 1.7f; L.Lighting.SkyLightIntensity = 1.4f; // harbour: dimmer moon, more sky bounce
		FFOPuzzleDef P; P.Type = EFOPuzzleType::Keypad; P.Id = TEXT("gatecode");
		P.Location = FVector(10000.f + 60.f, 330, 0); P.Yaw = 0.f; // front faces -X = the player approaching the gate
		P.Size = 4; P.Code = TEXT("3714"); P.PenaltyZombies = 3;
		P.NoteLocations = { FVector(1900, -400, 60), FVector(4300, 420, 60), FVector(6500, -430, 60), FVector(8600, 380, 60) };
		L.Puzzles.Add(P);
		AddSupplies(L, { {2500, 300, 40}, {5500, -300, 40}, {8000, 200, 40} },
		               { {1200, -300, 40}, {3000, 450, 40}, {4800, -450, 40}, {6000, 400, 40}, {7300, -200, 40}, {9000, -400, 40} });
		L.Stages.Add(Stage({ Obj(EFOObjectiveType::Collect, TEXT("note"), 4, TEXT("اعثر على أرقام الشيفرة {n}/{max} 📄"), TEXT("الهدف: اعثر على 4 ملاحظات تحمل أرقام الشيفرة")) }));
		L.Stages.Add(Stage({ Obj(EFOObjectiveType::SolvePuzzle, TEXT("gatecode"), 1, TEXT("أدخل الشيفرة في لوحة البوابة 🔢"), TEXT("اذهب إلى لوحة المفاتيح عند بوابة الحاويات"), TEXT("البوابة مفتوحة! اركض إلى القارب 🟢")) }));
		L.Stages.Add(Stage({ Obj(EFOObjectiveType::Reach, NAME_None, 1, TEXT("اركض إلى القارب 🟢")) }));
		L.OutroText = TEXT("غادر القارب الميناء. نجوت من سقوط وهران.");
		Levels.Add(L);
	}
	return Levels;
}

const TArray<FFOLevelDef>& FFOLevelRegistry::All()
{
	static const TArray<FFOLevelDef> Levels = BuildCampaign();
	return Levels;
}
const FFOLevelDef* FFOLevelRegistry::Get(int32 Index) { return All().IsValidIndex(Index) ? &All()[Index] : nullptr; }
const FFOLevelDef* FFOLevelRegistry::Find(FName Id) { for (const FFOLevelDef& L : All()) if (L.Id == Id) return &L; return nullptr; }
