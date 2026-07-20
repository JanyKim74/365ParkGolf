using UnrealBuildTool;
using System.IO;

public class ParkDay : ModuleRules
{
    public ParkDay(ReadOnlyTargetRules Target) : base(Target)
    {
        bEnableExceptions = true;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "Media",
            "MediaAssets",
            "MediaUtils",
            "PakFile",
            "AssetRegistry",
            "Landscape",
            "PhysicsCore",
            "UMG",
            "Slate",
            "SlateCore",
            "EngineSettings",
            "RenderCore",
            "RHI",
            "ImageWrapper",
            "Json",
            "JsonUtilities",
            "MoviePlayer",
            "HTTP",
            "EZSensorSDK"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AudioMixer",           // ← Private으로 이동
            "SignalProcessing",     // ← Private으로 이동
            "ElectraPlayerPlugin",  // ← Private으로 이동 (플러그인 모듈)
            "ElectraCodecFactory",  // ← Private으로 이동 (플러그인 모듈)
        });

        PublicDefinitions.Add("MMNOSOUND");

        // Editor 전용
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "LandscapeEditor",
                "ToolMenus",
                "PropertyEditor",
            });
        }

        // Sensor DLL
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty", "CR2Adapt", "Include"));
        //PublicDelayLoadDLLs.Add("XTparkAdapt64.dll");
       // RuntimeDependencies.Add(System.IO.Path.Combine(ModuleDirectory, "../../Binaries/Win64/XTparkAdapt64.dll"));
       // PublicDelayLoadDLLs.Add("XcamAdapt64.dll");
       // RuntimeDependencies.Add(System.IO.Path.Combine(ModuleDirectory, "../../Binaries/Win64/XcamAdapt64.dll"));
        PublicDelayLoadDLLs.Add("ZparkAdapt64.dll");
        RuntimeDependencies.Add(System.IO.Path.Combine(ModuleDirectory, "../../Binaries/Win64/ZparkAdapt64.dll"));

        // ❌ mfplat/mfreadwrite/mfuuid/sapi .lib 블록 전체 제거
        // WmfMedia 제거 + Electra 사용 시 불필요, 중복 링크 유발

        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            OptimizeCode = CodeOptimization.InShippingBuildsOnly;
        }
    }
}