// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ParkDayTarget : TargetRules
{
    public ParkDayTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

    //    bOverrideBuildEnvironment = true; // ← Unique보다 반드시 먼저
    //    BuildEnvironment = TargetBuildEnvironment.Unique;

        ExtraModuleNames.Add("ParkDay");

        bUseUnityBuild = true;
        bUsePCHFiles = true;
    }
}