using UnrealBuildTool;
using System.Collections.Generic;
public class FallOfOranEditorTarget : TargetRules
{
	public FallOfOranEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("FallOfOran");
	}
}
