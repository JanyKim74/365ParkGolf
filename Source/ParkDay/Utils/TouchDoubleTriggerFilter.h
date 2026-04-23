#pragma once

#include "CoreMinimal.h"

#include "Framework/Application/IInputProcessor.h"

/**
 * 전역 더블클릭/더블탭 방지 필터 (UE4.26 호환)
 *
 * - 터치/마우스 구분 없이 입력 이벤트(Down/Up) 자체를 디바운스하여 중복 입력을 Consume 한다.
 * - 같은 포인터(손가락/마우스)에서 짧은 시간 내 발생한 Down/Up을 차단한다.
 *
 * 권장:
 *   BlockWindowSeconds = 0.08 ~ 0.15
 *   (키오스크/터치 환경이면 0.12 정도가 체감상 안정적)
 */
class FTouchDoubleTriggerFilter
	: public IInputProcessor
{
public:
	FTouchDoubleTriggerFilter();
	virtual ~FTouchDoubleTriggerFilter() = default;

	/** 디바운스 윈도우(초) */
	void SetBlockWindowSeconds(float InSeconds) { BlockWindowSeconds = FMath::Max(0.0f, InSeconds); }

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

	virtual const TCHAR* GetDebugName() const override { return TEXT("FTouchDoubleTriggerFilter"); }

private:
	// Down/Up을 각각 디바운스(장치에 따라 Up이 2번 들어오는 케이스 대응)
	double LastDownAcceptedTime = -1.0;
	double LastUpAcceptedTime = -1.0;

	// 포인터 인덱스(마우스/터치 finger index) 추적
	int32 LastDownPointerIndex = INDEX_NONE;
	int32 LastUpPointerIndex = INDEX_NONE;

	float BlockWindowSeconds = 0.12f;

	bool ShouldConsumeDown(const FPointerEvent& E) const;
	bool ShouldConsumeUp(const FPointerEvent& E) const;

	void AcceptDown(const FPointerEvent& E);
	void AcceptUp(const FPointerEvent& E);
};
