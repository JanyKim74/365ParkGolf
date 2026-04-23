#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShotDetector.generated.h"

class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShotDetected, float, ShotTime);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PARKDAY_API UShotDetector : public UActorComponent
{
    GENERATED_BODY()

private:
    bool bIsMonitoring = false;

    UPROPERTY(EditAnywhere, Category = "Shot Detection")
        float MotionThreshold = 0.3f; // Threshold for motion detection (e.g., average pixel difference)

    UPROPERTY()
        UTexture2D* PreviousFrame = nullptr; // Store the previous frame for difference calculation

public:
    // Sets default values for this component's properties
    UShotDetector();

    UPROPERTY(BlueprintAssignable, Category = "Shot Detection")
        FShotDetected OnShotDetected;

    UFUNCTION(BlueprintCallable, Category = "Shot Detection")
        void StartMonitoring();

    UFUNCTION(BlueprintCallable, Category = "Shot Detection")
        void StopMonitoring();

    UFUNCTION(BlueprintCallable, Category = "Shot Detection")
        void AnalyzeFrame(UTexture2D* CurrentFrame, float CurrentTime);

private:
    // Helper function to calculate motion difference between two frames
    // This implementation is highly simplified. Real-world motion detection is complex.
    float CalculateMotionDifference(UTexture2D* Frame1, UTexture2D* Frame2);

protected:
    virtual void BeginPlay() override;
};