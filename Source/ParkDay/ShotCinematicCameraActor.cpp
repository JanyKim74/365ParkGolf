#include "ShotCinematicCameraActor.h"
#include "Camera/CameraComponent.h"

AShotCinematicCameraActor::AShotCinematicCameraActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 기본값: 안 보이게(필요할 때만 ViewTarget으로 전환)
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	if (UCameraComponent* Cam = GetCameraComponent())
	{
		Cam->FieldOfView = FOV;
	}
}

void AShotCinematicCameraActor::SetCinematicActive(bool bActive)
{
	SetActorHiddenInGame(!bActive);

	if (UCameraComponent* Cam = GetCameraComponent())
	{
		Cam->FieldOfView = FOV;
	}
}
