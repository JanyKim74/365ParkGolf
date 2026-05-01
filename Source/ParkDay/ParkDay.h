// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Windows SDK의 PlaySound / PlaySoundW 매크로가
// UE의 EQuartzCommandType::PlaySound 와 충돌하는 것을 방지합니다.
#ifdef PlaySound
#undef PlaySound
#endif
#ifdef PlaySoundW
#undef PlaySoundW
#endif