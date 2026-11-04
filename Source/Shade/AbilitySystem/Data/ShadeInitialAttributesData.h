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

    // Authored movement limits; the Character applies these to CharacterMovement.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1.0", Units="cm/s"))
    float WalkSpeed = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1.0", Units="cm/s"))
    float RunSpeed = 0.f;

    // Constant walking braking deceleration, in cm/s^2. Zero is allowed.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0.0"))
    float BrakingDecelerationWalking = 0.f;
};
