// ============================================================================
// AimActor.h - Aim point for golf game
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AimActor.generated.h"

UCLASS()
class PARKDAY_API AAimActor : public AActor
{
    GENERATED_BODY()

public:
    AAimActor();

    // Visual component for the aim point (Skeletal Mesh)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
        class USkeletalMeshComponent* AimMesh;

    // Set the position of the aim actor based on origin and direction
    UFUNCTION(BlueprintCallable, Category = "Aim")
        void SetAimPosition(const FVector& Origin, const FVector& Direction, float Distance = 500.0f);

    // Set aim position directly
    UFUNCTION(BlueprintCallable, Category = "Aim")
        void SetAimLocation(const FVector& NewLocation);

    // Control visibility of the aim actor
    UFUNCTION(BlueprintCallable, Category = "Aim")
        void SetAimVisibility(bool bVisible);

    // Enable/Disable debug sphere rendering
    UFUNCTION(BlueprintCallable, Category = "Debug")
        void SetDebugMode(bool bEnabled);

    // ✅ 거리별 스케일 조절
    UFUNCTION(BlueprintCallable, Category = "Aim")
        void SetScaleByDistance(float Distance);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ✅ Blueprint 클래스 저장 (생성자에서 로드, BeginPlay에서 스폰)
    UPROPERTY()
        TSubclassOf<AActor> AimMeshBlueprintClass;

    // ✅ 스폰된 Blueprint 인스턴스 (전체 hierarchy 포함)
    UPROPERTY()
        AActor* SpawnedBlueprintActor = nullptr;

private:
    // Debug mode flag
    bool bDebugMode = false;
};