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

// Signed delta: negative damages, positive heals. Callers supply Data.Health.Change.
UCLASS()
class SHADE_API UShadeHealthChangeEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UShadeHealthChangeEffect();
};

// Death persists on the ASC across body replacement. There is no revival policy yet.
UCLASS()
class SHADE_API UShadeDeadEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UShadeDeadEffect();
};

class UAbilitySystemComponent;
namespace ShadeHealthEffects
{
    // Authority-only application; false means invalid input or rejected effect.
    SHADE_API bool ApplyHealthChange(UAbilitySystemComponent* Target, float Delta);
}
