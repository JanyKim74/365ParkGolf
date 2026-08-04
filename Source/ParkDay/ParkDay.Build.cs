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
        PublicDelayLoadDLLs.Add("XTparkAdapt64.dll");
        RuntimeDependencies.Add(System.IO.Path.Combine(ModuleDirectory, "../../Binaries/Win64/XTparkAdapt64.dll"));
       // PublicDelayLoadDLLs.Add("XcamAdapt64.dll");
       // RuntimeDependencies.Add(System.IO.Path.Combine(ModuleDirectory, "../../Binaries/Win64/XcamAdapt64.dll"));
       // PublicDelayLoadDLLs.Add("ZparkAdapt64.dll");
       // RuntimeDependencies.Add(System.IO.Path.Combine(ModuleDirectory, "../../Binaries/Win64/ZparkAdapt64.dll"));

        // ❌ mfplat/mfreadwrite/mfuuid/sapi .lib 블록 전체 제거
        // WmfMedia 제거 + Electra 사용 시 불필요, 중복 링크 유발

        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            OptimizeCode = CodeOptimization.InShippingBuildsOnly;
        }

        // ═══ Supertonic 3 (ONNX Runtime) — 온디바이스 한국어 TTS ═══
        // ═══ Supertonic 3 (ONNX Runtime) — 온디바이스 한국어 TTS ═══
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string SupertonicPath = Path.Combine(ModuleDirectory, "ThirdParty", "Supertonic");
            string OrtLibFile = Path.Combine(SupertonicPath, "Lib", "onnxruntime.lib");

            if (File.Exists(OrtLibFile))
            {
                PublicIncludePaths.Add(Path.Combine(SupertonicPath, "Include"));
                PublicAdditionalLibraries.Add(OrtLibFile);
                PublicDelayLoadDLLs.Add("onnxruntime.dll");
                RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "../../Binaries/Win64/onnxruntime.dll"));

                string ProvidersShared = Path.Combine(ModuleDirectory, "../../Binaries/Win64/onnxruntime_providers_shared.dll");
                if (File.Exists(ProvidersShared))
                {
                    RuntimeDependencies.Add(ProvidersShared);
                }

                string ModelDir = Path.Combine(ModuleDirectory, "../../Content/DATA/Supertonic");
                if (Directory.Exists(ModelDir))
                {
                    RuntimeDependencies.Add(Path.Combine(ModelDir, "..."), StagedFileType.NonUFS);
                }

                PublicDefinitions.Add("WITH_SUPERTONIC=1");
                System.Console.WriteLine("✅ Supertonic 3 (ONNX Runtime) enabled");
            }
            else
            {
                PublicDefinitions.Add("WITH_SUPERTONIC=0");
                System.Console.WriteLine("⚠️ Supertonic disabled: " + OrtLibFile + " not found");
            }
        }
        else
        {
            PublicDefinitions.Add("WITH_SUPERTONIC=0");
        }
    }
}