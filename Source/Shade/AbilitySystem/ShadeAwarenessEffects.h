#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ShadeAwarenessEffects.generated.h"

UCLASS()
class SHADE_API UShadeInitialAwarenessEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UShadeInitialAwarenessEffect();
};

UCLASS()
class SHADE_API UShadeAwarenessCalmEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UShadeAwarenessCalmEffect();
};

UCLASS()
class SHADE_API UShadeAwarenessSuspiciousEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UShadeAwarenessSuspiciousEffect();
};

UCLASS()
class SHADE_API UShadeAwarenessAlertEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UShadeAwarenessAlertEffect();
};
// SetByCaller magnitude is the signed change per period, not change per second.
UCLASS()
class SHADE_API UShadeAlertnessTrendEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UShadeAlertnessTrendEffect();
};
