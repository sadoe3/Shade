#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ShadeHealthEffects.generated.h"

// Initializes both Health attributes from the caller's Data.Health.Initial value.
UCLASS()
class SHADE_API UShadeInitialHealthEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UShadeInitialHealthEffect();
};
