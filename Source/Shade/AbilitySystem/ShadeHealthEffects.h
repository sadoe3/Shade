#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ShadeHealthEffects.generated.h"

UCLASS()
class SHADE_API UShadeInitialHealthEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UShadeInitialHealthEffect();
};

// Signed SetByCaller change: negative damage, positive healing.
UCLASS()
class SHADE_API UShadeHealthChangeEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UShadeHealthChangeEffect();
};

UCLASS()
class SHADE_API UShadeDeadEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UShadeDeadEffect();
};
