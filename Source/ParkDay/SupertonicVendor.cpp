// SupertonicVendor.cpp
// Supertonic 공식 C++ 예제(helper.cpp)를 UE 빌드 환경에서 컴파일하기 위한 래퍼.
//
// helper 구현은 ThirdParty/Supertonic/src/helper.cpp.inl 로 두어
// UBT가 직접 컴파일하지 않게 하고(경고→에러 승격 회피 불가),
// 이 래퍼가 경고 억제 + Windows 타입 가드를 씌운 채 단독으로 컴파일한다.
//
// 원본 대비 유일한 수정: loadOnnx() 의 Windows 와이드 경로 변환
// ([ParkDay 패치] 주석 참조 — 원본은 char* 경로라 Windows 컴파일 불가).
#include "CoreMinimal.h"

#ifndef WITH_SUPERTONIC
#define WITH_SUPERTONIC 0
#endif

#if WITH_SUPERTONIC && PLATFORM_WINDOWS

#include "Windows/AllowWindowsPlatformTypes.h"

#pragma warning(push)
#pragma warning(disable: 4996) // deprecated (std::codecvt 등)
#pragma warning(disable: 4244) // 형 변환 축소
#pragma warning(disable: 4267) // size_t → int
#pragma warning(disable: 4456) // 지역 변수 섀도잉
#pragma warning(disable: 4459) // 전역 섀도잉
#pragma warning(disable: 4668) // 미정의 매크로 #if
#pragma warning(disable: 4583) // union 소멸자 (nlohmann)
#pragma warning(disable: 4582) // union 생성자 (nlohmann)

#include "ThirdParty/Supertonic/src/helper.cpp.inl"

#pragma warning(pop)

#include "Windows/HideWindowsPlatformTypes.h"

#endif // WITH_SUPERTONIC && PLATFORM_WINDOWS
