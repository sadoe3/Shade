#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "ShadeAssassinateAbility.generated.h"
UCLASS()
class SHADE_API UShadeAssassinationEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UShadeAssassinationEffect();
};

// Immediate prototype execution; montage/cancellation timing is deliberately not implied.
UCLASS()
class SHADE_API UShadeAssassinateAbility : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UShadeAssassinateAbility();
    static bool IsValidTarget(AActor* Attacker, AActor* Target, float Range);
    virtual bool ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayEventData* Payload) const override;
protected:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
