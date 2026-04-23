// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ParkDayEditorTarget : TargetRules
{
    public ParkDayEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor; // 중복 제거
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

       // bOverrideBuildEnvironment = true;

        ExtraModuleNames.Add("ParkDay");
    }
}