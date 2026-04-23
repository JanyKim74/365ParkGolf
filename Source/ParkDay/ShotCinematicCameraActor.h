#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "ShotCinematicCameraActor.generated.h"

/**
 * 연출용 임시 카메라 액터
 * - 재사용을 위해 Spawn 후 숨겨두고 필요할 때 위치/회전만 갱신
 */
UCLASS()
class PARKDAY_API AShotCinematicCameraActor : public ACameraActor
{
	GENERATED_BODY()

public:
	AShotCinematicCameraActor();

	/** 연출 카메라 파라미터(필요시 에디터/런타임에서 조정) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	float FOV = 60.f;

	/** 카메라 활성/비활성(단순 Hidden 토글용) */
	void SetCinematicActive(bool bActive);
};
