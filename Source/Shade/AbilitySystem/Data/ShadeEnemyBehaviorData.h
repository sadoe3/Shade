#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShadeEnemyBehaviorData.generated.h"

// Shared tuning only; each Enemy stores live awareness in its own AttributeSet.
UCLASS(BlueprintType)
class SHADE_API UShadeEnemyBehaviorData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Awareness", meta=(ClampMin="1.0"))
    float MaxAlertness = 100.f;
};
