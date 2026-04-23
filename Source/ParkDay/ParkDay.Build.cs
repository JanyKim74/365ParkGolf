// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO; // <<--- 이 줄을 추가합니다.

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
            "MediaAssets", // <<-- 이 줄이 있는지 확인하고 없다면 추가합니다.
            "MediaUtils",  // <<-- 이 줄도 같이 있는지 확인하고 없다면 추가합니다.
            "PakFile",         // ★ pak 마운트
            "AssetRegistry",   // ★ 스캔
            // ⭐ Landscape 관련 모듈들 추가
            "Landscape",                    // Landscape 기본 기능
            // ⭐ 물리 관련 모듈들 추가  
            "PhysicsCore",                  // 물리 시스템
            "Chaos",                        // Chaos 물리 (UE5)

            "WmfMedia",         // Windows Media (선택)
            "AudioMixer",   // ← 없으면 추가
            "SignalProcessing", // ← 없으면 추가
            
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
                // 필요시 "UMGEditor","LevelEditor" 등도 여기에
            });
        }
        // ⭐ UE4/UE5 버전별 조건부 컴파일
        if (Target.Version.MajorVersion == 4)
        {
            // UE4 전용 모듈들
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "ImageWrapper",  // ✅ 추가
                "PhysX",                    // PhysX 물리 엔진
                "APEX"                      // APEX 시스템
            });
        }
        else if (Target.Version.MajorVersion >= 5)
        {
            // UE5 전용 모듈들
            PrivateDependencyModuleNames.AddRange(new string[]
            {
              //  "ChaosVehicles",           // Chaos 차량 시스템
                "GeometryCollectionEngine" // 지오메트리 컬렉션
            });
        }

        // Sensor 추가
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty", "CR2Adapt", "Include"));
        PublicDelayLoadDLLs.Add("XcamAdapt64.dll");
        RuntimeDependencies.Add(System.IO.Path.Combine(ModuleDirectory, "../../Binaries/Win64/XcamAdapt64.dll"));

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // Windows 10 SDK 라이브러리 경로
            string WindowsSDKLibPath = @"C:\Program Files (x86)\Windows Kits\10\Lib";

            // 여러 SDK 버전 시도
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
                // SDK 경로를 찾지 못한 경우 기본 링크 방식 사용
                PublicAdditionalLibraries.Add("mfplat.lib");
                PublicAdditionalLibraries.Add("mfreadwrite.lib");
                PublicAdditionalLibraries.Add("mfuuid.lib");
                PublicAdditionalLibraries.Add("sapi.lib");

                System.Console.WriteLine("Using default Windows SDK libraries");
            }

            // 또는 더 간단한 방법 (UE4가 자동으로 SDK 경로를 찾음)
            // PublicSystemLibraries.AddRange(new string[]
            // {
            // 	"mfplat.lib",
            // 	"mfreadwrite.lib",
            // 	"mfuuid.lib"
            // });


        }
        // ⭐ Shipping 빌드 최적화 완화 (필요시)
        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            OptimizeCode = CodeOptimization.InShippingBuildsOnly;
        }
        /*
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // OpenCV 경로 설정 (프로젝트 ThirdParty 폴더)
            string OpenCVPath = Path.Combine(ModuleDirectory, "../../ThirdParty/OpenCV");

            if (Directory.Exists(OpenCVPath))
            {
                // Include 경로
                string IncludePath = Path.Combine(OpenCVPath, "include");
                PublicIncludePaths.Add(IncludePath);

                // Library 경로
                string LibPath = Path.Combine(OpenCVPath, "lib");
                PublicAdditionalLibraries.Add(Path.Combine(LibPath, "opencv_world4120.lib"));  // 버전에 맞게 수정

                // DLL 경로 (런타임)
                string DllPath = Path.Combine(OpenCVPath, "bin");
                RuntimeDependencies.Add(Path.Combine(DllPath, "opencv_world4120.dll"));  // 버전에 맞게 수정

                PublicDefinitions.Add("WITH_OPENCV=1");
            }
            else
            {
                System.Console.WriteLine("WARNING: OpenCV not found at " + OpenCVPath);
                PublicDefinitions.Add("WITH_OPENCV=0");
            }
        }
        else
        {
            PublicDefinitions.Add("WITH_OPENCV=0");
        }
        */
    }
}