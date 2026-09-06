using UnrealBuildTool;
public class FallOfOran : ModuleRules
{
	public FallOfOran(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory); // allow "Core/..", "Mission/.." style includes from any subfolder
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", "Slate", "SlateCore" });
	}
}
