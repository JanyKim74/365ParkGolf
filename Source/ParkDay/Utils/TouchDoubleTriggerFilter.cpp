#include "TouchDoubleTriggerFilter.h"

#include "Framework/Application/SlateApplication.h"

FTouchDoubleTriggerFilter::FTouchDoubleTriggerFilter()
{
}

bool FTouchDoubleTriggerFilter::ShouldConsumeDown(const FPointerEvent& E) const
{
	if (LastDownAcceptedTime < 0.0)
	{
		return false;
	}

	const double Now = FPlatformTime::Seconds();
	const double Delta = Now - LastDownAcceptedTime;

	const int32 PointerIndex = E.GetPointerIndex();
	const bool bSamePointer = (LastDownPointerIndex == INDEX_NONE) || (PointerIndex == LastDownPointerIndex);

	return bSamePointer && (Delta >= 0.0) && (Delta <= BlockWindowSeconds);
}

bool FTouchDoubleTriggerFilter::ShouldConsumeUp(const FPointerEvent& E) const
{
	if (LastUpAcceptedTime < 0.0)
	{
		return false;
	}

	const double Now = FPlatformTime::Seconds();
	const double Delta = Now - LastUpAcceptedTime;

	const int32 PointerIndex = E.GetPointerIndex();
	const bool bSamePointer = (LastUpPointerIndex == INDEX_NONE) || (PointerIndex == LastUpPointerIndex);

	return bSamePointer && (Delta >= 0.0) && (Delta <= BlockWindowSeconds);
}

void FTouchDoubleTriggerFilter::AcceptDown(const FPointerEvent& E)
{
	LastDownAcceptedTime = FPlatformTime::Seconds();
	LastDownPointerIndex = E.GetPointerIndex();
}

void FTouchDoubleTriggerFilter::AcceptUp(const FPointerEvent& E)
{
	LastUpAcceptedTime = FPlatformTime::Seconds();
	LastUpPointerIndex = E.GetPointerIndex();
}


bool FTouchDoubleTriggerFilter::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	// 터치든 클릭이든 상관 없이 Down 중복을 차단
	if (ShouldConsumeDown(MouseEvent))
	{
		return true; // Consume: UMG까지 이벤트가 내려가지 않음(= 인식 자체 차단)
	}

	AcceptDown(MouseEvent);
	return false;
}

bool FTouchDoubleTriggerFilter::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	// 터치든 클릭이든 상관 없이 Up 중복을 차단
	if (ShouldConsumeUp(MouseEvent))
	{
		return true; // Consume
	}

	AcceptUp(MouseEvent);
	return false;
}

