using UnrealBuildTool;
public class FallOfOran : ModuleRules
{
	public FallOfOran(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", "Slate", "SlateCore" });
	}
}
