// Copyright Epic Games, Inc. All Rights Reserved.

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
            "Media",            // MediaPlayer
            "MediaAssets",
            "MediaUtils",
            "PakFile",         // ★ pak 마운트
            "AssetRegistry",   // ★ 스캔
            // ⭐ Landscape 관련 모듈들 추가
            "Landscape",                    // Landscape 기본 기능
            // ⭐ 물리 관련 모듈들 추가  
            "PhysicsCore",                  // 물리 시스템
            "Chaos",                        // Chaos 물리 (UE5)

            "WmfMedia",         // Windows Media (선택)
            "AudioMixer",
            "SignalProcessing",
            
            // ⭐ UI 관련 모듈들 추가
            "UMG",                          // UI 위젯
            "Slate",                        // UI 프레임워크
            "SlateCore",                    // UI 코어
            
            // ⭐ 유틸리티 모듈들 추가
            "EngineSettings",               // 엔진 설정
            "RenderCore",                   // 렌더링 (디버그 그리기용)
            "RHI",                           // 렌더링 하드웨어 인터페이스
            "ImageWrapper",
            "Json","JsonUtilities",
            "MoviePlayer",
            "HTTP"

        });

        PrivateDependencyModuleNames.AddRange(new string[] {
             // 런타임 OK
            // ❌ 여기엔 Editor 모듈 두지 않기
        });

        // ★ Editor 전용 모듈은 반드시 에디터 한정
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[] {
                // "UnrealEd",
                "LandscapeEditor",
                "ToolMenus",
                "PropertyEditor",
            });
        }
        // ⭐ UE4/UE5 버전별 조건부 컴파일
        if (Target.Version.MajorVersion == 4)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "ImageWrapper",
                "PhysX",
                "APEX"
            });
        }
        else if (Target.Version.MajorVersion >= 5)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "GeometryCollectionEngine"
            });
        }

        // Sensor 추가

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty", "CR2Adapt", "Include"));
        PublicDelayLoadDLLs.Add("XTparkAdapt64.dll");
        RuntimeDependencies.Add(System.IO.Path.Combine(ModuleDirectory, "../../Binaries/Win64/XTparkAdapt64.dll"));
        // PublicDelayLoadDLLs.Add("XcamAdapt64.dll");
        // RuntimeDependencies.Add(System.IO.Path.Combine(ModuleDirectory, "../../Binaries/Win64/XcamAdapt64.dll"));
        // PublicDelayLoadDLLs.Add("ZparkAdapt64.dll");
        // RuntimeDependencies.Add(System.IO.Path.Combine(ModuleDirectory, "../../Binaries/Win64/ZparkAdapt64.dll"));

        // ═══════════════════════════════════════════════════════════════════
        // ⭐ Supertonic 3 (ONNX Runtime) — 온디바이스 한국어 TTS
        //
        // 폴더 구조 (수동 준비):
        //   Source/ParkDay/ThirdParty/Supertonic/Include/   ← onnxruntime_cxx_api.h 등
        //                                                     (ONNX Runtime 릴리스의 include/ 통째로)
        //   Source/ParkDay/ThirdParty/Supertonic/Lib/onnxruntime.lib
        //   Binaries/Win64/onnxruntime.dll                  ← 릴리스의 lib/onnxruntime.dll
        //   Content/DATA/Supertonic/                        ← HF Supertone/supertonic-3 에셋
        //                                                     (*.onnx, tokenizer, voice_styles/*.json)
        //
        // ⚠️ UE5.7 NNERuntimeORT 플러그인이 엔진 바이너리에 자체 onnxruntime.dll을
        //    포함하고 있음. 이름이 같으므로 delay-load만으로는 어느 쪽이 바인딩될지
        //    보장 안 됨 → SupertonicTTSSubsystem::Initialize()에서 전체 경로
        //    LoadLibraryW를 "ORT 함수 최초 호출 전"에 반드시 먼저 수행할 것.
        //    (CR2 XcamAdapt64.dll 과 동일한 명시적 로드 패턴)
        //
        // ⚠️ Content/DATA/Supertonic 은 defaultGameData.json 과 마찬가지로
        //    쿠커가 추적하지 않음 → 빌드 출력에 수동 복사 필요.
        //    아래 RuntimeDependencies 등록으로 스테이징에 포함시키지만,
        //    최종 확인은 패키징 후 필수.
        // ═══════════════════════════════════════════════════════════════════
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string SupertonicPath = Path.Combine(ModuleDirectory, "ThirdParty", "Supertonic");
            string OrtIncludePath = Path.Combine(SupertonicPath, "Include");
            string OrtLibFile     = Path.Combine(SupertonicPath, "Lib", "onnxruntime.lib");
            string OrtDllFile     = Path.Combine(ModuleDirectory, "../../Binaries/Win64/onnxruntime.dll");

            if (File.Exists(OrtLibFile))
            {
                PublicIncludePaths.Add(OrtIncludePath);
                PublicAdditionalLibraries.Add(OrtLibFile);

                // 로더가 즉시 바인딩하지 않도록 delay-load → 우리가 명시적 LoadLibrary로 선점
                PublicDelayLoadDLLs.Add("onnxruntime.dll");
                RuntimeDependencies.Add(OrtDllFile);

                // ONNX Runtime 부속 DLL이 있으면 함께 스테이징 (버전에 따라 존재)
                string ProvidersShared = Path.Combine(ModuleDirectory, "../../Binaries/Win64/onnxruntime_providers_shared.dll");
                if (File.Exists(ProvidersShared))
                {
                    RuntimeDependencies.Add(ProvidersShared);
                }

                // 모델 에셋 스테이징 (Content/DATA/Supertonic 전체)
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

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // Windows 10 SDK 라이브러리 경로
            string WindowsSDKLibPath = @"C:\Program Files (x86)\Windows Kits\10\Lib";

            string[] SDKVersions = new string[]
            {
                "10.0.19041.0",  // Windows 10 SDK 2004
				"10.0.18362.0",  // Windows 10 SDK 1903
				"10.0.17763.0",  // Windows 10 SDK 1809
				"10.0.17134.0"   // Windows 10 SDK 1803
			};

            bool bFoundSDK = false;
            foreach (string Version in SDKVersions)
            {
                string LibPath = Path.Combine(WindowsSDKLibPath, Version, "um", "x64");
                if (Directory.Exists(LibPath))
                {
                    PublicAdditionalLibraries.Add(Path.Combine(LibPath, "mfplat.lib"));
                    PublicAdditionalLibraries.Add(Path.Combine(LibPath, "mfreadwrite.lib"));
                    PublicAdditionalLibraries.Add(Path.Combine(LibPath, "mfuuid.lib"));
                    PublicAdditionalLibraries.Add(Path.Combine(LibPath, "sapi.lib"));

                    System.Console.WriteLine("Found Windows SDK: " + Version);
                    bFoundSDK = true;
                    break;
                }
            }

            if (!bFoundSDK)
            {
                PublicAdditionalLibraries.Add("mfplat.lib");
                PublicAdditionalLibraries.Add("mfreadwrite.lib");
                PublicAdditionalLibraries.Add("mfuuid.lib");
                PublicAdditionalLibraries.Add("sapi.lib");

                System.Console.WriteLine("Using default Windows SDK libraries");
            }
        }

        // ⭐ Shipping 빌드 최적화 완화 (필요시)
        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            OptimizeCode = CodeOptimization.InShippingBuildsOnly;
        }
    }
}
