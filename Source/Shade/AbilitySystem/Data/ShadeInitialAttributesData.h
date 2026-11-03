#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShadeInitialAttributesData.generated.h"

// Shared initialization settings; live Attribute values belong to the AttributeSet.
UCLASS(BlueprintType)
class SHADE_API UShadeInitialAttributesData : public UDataAsset
{
    GENERATED_BODY()

public:
    // Zero means unconfigured. Set a positive starting value on each asset.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes", meta=(ClampMin="1.0"))
    float InitialHealth = 0.f;
};
