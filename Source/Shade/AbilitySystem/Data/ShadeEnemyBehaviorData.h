#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShadeEnemyBehaviorData.generated.h"

// Tuning shared by Enemy instances; live Alertness remains in each ASC.
UCLASS(BlueprintType)
class SHADE_API UShadeEnemyBehaviorData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Awareness", meta=(ClampMin="1.0"))
    float MaxAlertness = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Awareness", meta=(ClampMin="0.01"))
    float AlertnessGainPerSecond = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Awareness", meta=(ClampMin="0.01"))
    float AlertnessDecayPerSecond = 25.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Awareness", meta=(ClampMin="0.0", Units="s"))
    float AlertnessDecayDelay = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Awareness", meta=(ClampMin="0.01", Units="s"))
    float AlertnessUpdatePeriod = 0.1f;
};
