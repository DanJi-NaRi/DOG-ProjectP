using UnrealBuildTool;
using System.IO;

public class WhisperRuntime : ModuleRules
{
	public WhisperRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public", "Whisper"));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects"
		});

		string PluginRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../"));

		bool bWithWhisper = Target.Platform == UnrealTargetPlatform.Win64
			&& (Target.Type == TargetType.Editor || Target.Type == TargetType.Game || Target.Type == TargetType.Client);
		PrivateDefinitions.Add(bWithWhisper ? "WHISPERRUNTIME_WITH_WHISPER=1" : "WHISPERRUNTIME_WITH_WHISPER=0");

		if (bWithWhisper)
		{
			string MiniaudioPath = Path.Combine(PluginRoot, "ThirdParty", "Miniaudio");
			string MiniaudioIncludePath = Path.Combine(MiniaudioPath, "Include");
			string WhisperPath = Path.Combine(PluginRoot, "ThirdParty", "Whisper");
			string WhisperIncludePath = Path.Combine(WhisperPath, "Include");
			string WhisperGgmlIncludePath = Path.Combine(WhisperIncludePath, "ggml");
			string WhisperLibraryPath = Path.Combine(WhisperPath, "Lib", "Win64");
			string WhisperBinaryPath = Path.Combine(WhisperPath, "Bin", "Win64");

			PrivateDefinitions.Add("WHISPER_SHARED");
			PrivateIncludePaths.Add(MiniaudioIncludePath);
			PrivateIncludePaths.Add(WhisperIncludePath);
			PrivateIncludePaths.Add(WhisperGgmlIncludePath);

			PublicAdditionalLibraries.Add(Path.Combine(WhisperLibraryPath, "whisper.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(WhisperLibraryPath, "ggml.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(WhisperLibraryPath, "ggml-base.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(WhisperLibraryPath, "ggml-cpu.lib"));

			PublicDelayLoadDLLs.Add("whisper.dll");
			PublicDelayLoadDLLs.Add("ggml.dll");
			PublicDelayLoadDLLs.Add("ggml-base.dll");
			PublicDelayLoadDLLs.Add("ggml-cpu.dll");

			RuntimeDependencies.Add("$(BinaryOutputDir)/whisper.dll", Path.Combine(WhisperBinaryPath, "whisper.dll"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/ggml.dll", Path.Combine(WhisperBinaryPath, "ggml.dll"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/ggml-base.dll", Path.Combine(WhisperBinaryPath, "ggml-base.dll"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/ggml-cpu.dll", Path.Combine(WhisperBinaryPath, "ggml-cpu.dll"));
		}

		string DefaultModelPath = Path.Combine(PluginRoot, "Content", "Models", "ggml-base.bin");
		if (File.Exists(DefaultModelPath))
		{
			RuntimeDependencies.Add("$(BinaryOutputDir)/WhisperRuntime/Models/ggml-base.bin", DefaultModelPath);
		}
	}
}
