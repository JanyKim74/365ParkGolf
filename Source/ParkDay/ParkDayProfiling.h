// =============================================================================
// ParkDayProfiling.h
// 사용법: 각 .cpp 상단 include에 추가 → Tick/함수 내부에 매크로 삽입
//
// 콘솔 확인: stat ParkDay
// 전체 게임:  stat game
// =============================================================================

#pragma once
#include "Stats/Stats.h"

// -----------------------------------------------------------------------------
// STAT 그룹 선언 (이 헤더에서 1회만 선언)
// -----------------------------------------------------------------------------
DECLARE_STATS_GROUP(TEXT("ParkDay"), STATGROUP_ParkDay, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("ParkDayPhysics"), STATGROUP_ParkDayPhysics, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("ParkDayCamera"), STATGROUP_ParkDayCamera, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("ParkDayWebcam"), STATGROUP_ParkDayWebcam, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("ParkDayUI"), STATGROUP_ParkDayUI, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("ParkDaySensor"), STATGROUP_ParkDaySensor, STATCAT_Advanced);

// =============================================================================
// [1] WebcamCapture — 1순위 병목 (CaptureFrame, 텍스처 변환)
// =============================================================================
DECLARE_CYCLE_STAT(TEXT("WebcamCapture::Tick"), STAT_WebcamTick, STATGROUP_ParkDayWebcam);
DECLARE_CYCLE_STAT(TEXT("WebcamCapture::CaptureFrame"), STAT_WebcamCaptureFrame, STATGROUP_ParkDayWebcam);
DECLARE_CYCLE_STAT(TEXT("WebcamCapture::CaptureCurrentFrame"), STAT_WebcamCaptureCurrentFrame, STATGROUP_ParkDayWebcam);
DECLARE_CYCLE_STAT(TEXT("WebcamCapture::CreateTexture2DFromPixels"), STAT_WebcamCreateTexture, STATGROUP_ParkDayWebcam);
DECLARE_CYCLE_STAT(TEXT("WebcamCapture::DrawMaterialToRT"), STAT_WebcamDrawMaterial, STATGROUP_ParkDayWebcam);
DECLARE_CYCLE_STAT(TEXT("WebcamCapture::UpdateTexture2DAsync"), STAT_WebcamUpdateTextureAsync, STATGROUP_ParkDayWebcam);
DECLARE_CYCLE_STAT(TEXT("WebcamCapture::ProcessDummySwing"), STAT_WebcamDummySwing, STATGROUP_ParkDayWebcam);
DECLARE_CYCLE_STAT(TEXT("WebcamCapture::SaveSwingClipToDisk"), STAT_WebcamSaveClip, STATGROUP_ParkDayWebcam);
DECLARE_CYCLE_STAT(TEXT("WebcamCapture::ExtractSwingFrames"), STAT_WebcamExtractFrames, STATGROUP_ParkDayWebcam);

// =============================================================================
// [2] GolfBall — Tick + 물리 세부 분리
// =============================================================================
DECLARE_CYCLE_STAT(TEXT("GolfBall::Tick"), STAT_GolfBallTick, STATGROUP_ParkDayPhysics);
DECLARE_CYCLE_STAT(TEXT("GolfBall::UpdatePhysicsBasedOnState"), STAT_GolfBallUpdatePhysics, STATGROUP_ParkDayPhysics);
DECLARE_CYCLE_STAT(TEXT("GolfBall::UpdateFlyingPhysics"), STAT_GolfBallFlyPhysics, STATGROUP_ParkDayPhysics);
DECLARE_CYCLE_STAT(TEXT("GolfBall::UpdateRollingPhysics"), STAT_GolfBallRollPhysics, STATGROUP_ParkDayPhysics);
DECLARE_CYCLE_STAT(TEXT("GolfBall::UpdateBouncePhysics"), STAT_GolfBallBouncePhysics, STATGROUP_ParkDayPhysics);
DECLARE_CYCLE_STAT(TEXT("GolfBall::CheckAutoStateTransitions"), STAT_GolfBallStateCheck, STATGROUP_ParkDayPhysics);
DECLARE_CYCLE_STAT(TEXT("GolfBall::CheckGroundType"), STAT_GolfBallGroundType, STATGROUP_ParkDayPhysics);
DECLARE_CYCLE_STAT(TEXT("GolfBall::CheckRealtimeOBCrossing"), STAT_GolfBallOBCheck, STATGROUP_ParkDayPhysics);
DECLARE_CYCLE_STAT(TEXT("GolfBall::UpdateBallTrail"), STAT_GolfBallTrail, STATGROUP_ParkDayPhysics);
DECLARE_CYCLE_STAT(TEXT("GolfBall::ValidatePhysicsState"), STAT_GolfBallValidate, STATGROUP_ParkDayPhysics);
DECLARE_CYCLE_STAT(TEXT("GolfBall::IsNearGround"), STAT_GolfBallIsNearGround, STATGROUP_ParkDayPhysics);

// =============================================================================
// [3] CameraManager — LineTrace 3회 포함
// =============================================================================
DECLARE_CYCLE_STAT(TEXT("CameraManager::Tick"), STAT_CameraTick, STATGROUP_ParkDayCamera);
DECLARE_CYCLE_STAT(TEXT("CameraManager::UpdateReadyCamera"), STAT_CameraReady, STATGROUP_ParkDayCamera);
DECLARE_CYCLE_STAT(TEXT("CameraManager::UpdateFlyingCamera"), STAT_CameraFlying, STATGROUP_ParkDayCamera);
DECLARE_CYCLE_STAT(TEXT("CameraManager::UpdateFollowingCamera"), STAT_CameraFollowing, STATGROUP_ParkDayCamera);
DECLARE_CYCLE_STAT(TEXT("CameraManager::UpdateStopCamera"), STAT_CameraStop, STATGROUP_ParkDayCamera);
DECLARE_CYCLE_STAT(TEXT("CameraManager::UpdateFixedCamera"), STAT_CameraFixed, STATGROUP_ParkDayCamera);
DECLARE_CYCLE_STAT(TEXT("CameraManager::PositionCameraForAimView"), STAT_CameraAimView, STATGROUP_ParkDayCamera);

// =============================================================================
// [4] GolfPlayerController
// =============================================================================
DECLARE_CYCLE_STAT(TEXT("GolfPlayerController::Tick"), STAT_PCTick, STATGROUP_ParkDay);
DECLARE_CYCLE_STAT(TEXT("GolfPlayerController::SwingMonitor"), STAT_PCSwingMonitor, STATGROUP_ParkDay);
DECLARE_CYCLE_STAT(TEXT("GolfPlayerController::TriggerSwingRecording"), STAT_PCTriggerSwing, STATGROUP_ParkDay);

// =============================================================================
// [5] InGameMode
// =============================================================================
DECLARE_CYCLE_STAT(TEXT("InGameMode::Tick"), STAT_InGameModeTick, STATGROUP_ParkDay);
DECLARE_CYCLE_STAT(TEXT("InGameMode::ProcessStateMachine"), STAT_InGameModeStateMachine, STATGROUP_ParkDay);
DECLARE_CYCLE_STAT(TEXT("InGameMode::FindActorByName"), STAT_InGameModeFindActor, STATGROUP_ParkDay);

// =============================================================================
// [6] CR2SensorManager — DLL 호출 포함
// =============================================================================
DECLARE_CYCLE_STAT(TEXT("CR2Sensor::Tick"), STAT_SensorTick, STATGROUP_ParkDaySensor);
DECLARE_CYCLE_STAT(TEXT("CR2Sensor::CheckSensorStatus"), STAT_SensorCheck, STATGROUP_ParkDaySensor);
DECLARE_CYCLE_STAT(TEXT("CR2Sensor::GetSensorStatus"), STAT_SensorGetStatus, STATGROUP_ParkDaySensor);

// =============================================================================
// [7] VideoBufferComponent — AddFrame, GetFramesInRange
// =============================================================================
DECLARE_CYCLE_STAT(TEXT("VideoBuffer::AddFrame"), STAT_VideoBufferAddFrame, STATGROUP_ParkDayWebcam);
DECLARE_CYCLE_STAT(TEXT("VideoBuffer::GetFramesInRange"), STAT_VideoBufferGetRange, STATGROUP_ParkDayWebcam);

// =============================================================================
// [8] BallNamePlateComponent — 매 볼마다 매 프레임 UI 갱신
// =============================================================================
DECLARE_CYCLE_STAT(TEXT("BallNamePlate::TickComponent"), STAT_NamePlateTick, STATGROUP_ParkDayUI);
DECLARE_CYCLE_STAT(TEXT("BallNamePlate::UpdateWorldPosition"), STAT_NamePlatePosition, STATGROUP_ParkDayUI);
DECLARE_CYCLE_STAT(TEXT("BallNamePlate::UpdateDistanceScale"), STAT_NamePlateScale, STATGROUP_ParkDayUI);

// =============================================================================
// [9] LandscapeChecker — GetLandTypeAtLocation (LineTrace 포함)
// =============================================================================
DECLARE_CYCLE_STAT(TEXT("LandscapeChecker::Tick"), STAT_LandscapeTick, STATGROUP_ParkDay);
DECLARE_CYCLE_STAT(TEXT("LandscapeChecker::GetLandType"), STAT_LandscapeGetLandType, STATGROUP_ParkDay);
DECLARE_CYCLE_STAT(TEXT("LandscapeChecker::CheckGroundAtLocation"), STAT_LandscapeCheckGround, STATGROUP_ParkDay);