#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GolfDataStructures.h"
#include "PlayerInfoSlotWidget.generated.h"

class UTextBlock;
class UImage;
class AInGameMode;

/**
 * 각각 플레이어의 정보를 표시하는 UMG 위젯입니다.
 */
UCLASS()
class PARKDAY_API UPlayerInfoSlotWidget : public UUserWidget
{
    GENERATED_BODY()

        virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float InDeltaTime) override;
public:
    void SetChance(bool bBlinking, int32 Score);
    void UpdateStroke(const FPlayerInfo PlayerInfo);

    bool bIsRuntimeAdded = false;

    // ⭐⭐⭐ 새로 추가: 런타임 추가 플레이어 스타일 설정 ⭐⭐⭐
    UFUNCTION(BlueprintCallable, Category = "Runtime Player")
        void SetRuntimeAddedStyle(bool bIsRuntime);

    // 앱 위젯에 표시할 플레이어 정보를 설정합니다.
    UFUNCTION(BlueprintCallable, Category = "Player Info Slot")
        void SetPlayerInfo(const FPlayerInfo& InPlayerInfo, int32 CurrentHoleIndex, int32 PlayerIdx, float DistanceToHole);

    // 1초에 몇 번 깜빡입니까(사인파)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blink")
        float BlinkFrequency = 0.75f;

    // 최소/최대 투명도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blink")
        float MinOpacity = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blink")
        float MaxOpacity = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "Blink")
        void StartBlink();

    UFUNCTION(BlueprintCallable, Category = "Blink")
        void StopBlink();

    // ⭐ 옵션: 런타임 추가 플레이어 표시용 위젯 (UMG에서 선택적으로 추가)
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadWrite, Category = "Runtime Player")
        UImage* Image_GuestBadge; // 게스트 아이콘

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadWrite, Category = "Runtime Player")
        UTextBlock* TextBlock_GuestLabel; // "GUEST" 라벨

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadWrite, Category = "Runtime Player")
        UImage* Image_RuntimeBorder; // 테두리 이미지

    // UMG 디자이너에 바인딩된 위젯들 (Designer에서 'Is Variable' 체크 필요)
    UPROPERTY(meta = (BindWidget))
        UTextBlock* TextBlock_Name_Off;
    UPROPERTY(meta = (BindWidget))
        UTextBlock* TextBlock_UnderPar_Off;

    UPROPERTY(meta = (BindWidget))
        UTextBlock* TextBlock_Stroke_Off;
    UPROPERTY(meta = (BindWidget))
        UTextBlock* TextBlock_Stroke_Off_st;

    UPROPERTY(meta = (BindWidget))
        UTextBlock* TextBlock_Distance_Off;
    UPROPERTY(meta = (BindWidget))
        UTextBlock* TextBlock_Distance_Off_m;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "Player Info Slot")
        UImage* Image_Player_Background_Off;

    UPROPERTY(meta = (BindWidget))
        UTextBlock* TextBlock_Name_On;
    UPROPERTY(meta = (BindWidget))
        UTextBlock* TextBlock_UnderPar_On;

    UPROPERTY(meta = (BindWidget))
        UTextBlock* TextBlock_Stroke_On;
    UPROPERTY(meta = (BindWidget))
        UTextBlock* TextBlock_Stroke_On_st;

    UPROPERTY(meta = (BindWidget))
        UTextBlock* TextBlock_Distance_On;
    UPROPERTY(meta = (BindWidget))
        UTextBlock* TextBlock_Distance_On_m;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "Player Info Slot")
        UImage* Image_Player_Background_On;

    UPROPERTY(meta = (BindWidgetOptional))
        UImage* CurrentTurnIndicator;

    UPROPERTY(meta = (BindWidget))
        UImage* Image_Chance;

    // ⭐ 홀아웃 상태 표시용 이미지 위젯
    UPROPERTY(meta = (BindWidget))
        UImage* Image_HoleOut;

    UFUNCTION(BlueprintCallable, Category = "Player Info Slot")
    void UpdatePlayerStateDisplay(EPlayerState NewState);

    void UpdateNickName(FString NickName);

    int32 OwningPlayerIndex;
    int32 OwningPlayerSlotIndex;
    // Stable per-widget display order for player images (runtime only).
    int32 DisplayIndex = INDEX_NONE;
    void HideAllStateDisplay();
    bool bBlinking = false;
    float Elapsed = 0.f;

private:
    // ⭐ 런타임 추가 플레이어 깜빡임 애니메이션용
    bool bIsRuntimeBlinking = false;
    float RuntimeBlinkElapsed = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Runtime Style")
        float RuntimeBlinkFrequency = 0.5f; // 초당 0.5회

    UPROPERTY(EditAnywhere, Category = "Runtime Style")
        float RuntimeBlinkDuration = 3.0f; // 3초간 깜빡임

    UPROPERTY(EditAnywhere, Category = "Runtime Style")
        float RuntimeMinOpacity = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Runtime Style")
        float RuntimeMaxOpacity = 0.85f;

protected:
    virtual bool Initialize() override;

    UPROPERTY()
        UTexture2D* CachedHoleOutTexture;

    AInGameMode* GM;
};
