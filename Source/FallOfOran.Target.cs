using UnrealBuildTool;
using System.Collections.Generic;
public class FallOfOranTarget : TargetRules
{
	public FallOfOranTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("FallOfOran");
	}
}
