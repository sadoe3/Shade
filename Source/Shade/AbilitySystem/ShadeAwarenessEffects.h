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

// One active instance per Enemy. The caller supplies a signed delta for each period.
UCLASS()
class SHADE_API UShadeAlertnessTrendEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UShadeAlertnessTrendEffect();
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
