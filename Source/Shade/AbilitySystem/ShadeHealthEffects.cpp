#include "ShadeHealthEffects.h"

#include "ShadeGameplayTags.h"
#include "ShadeAttributeSet.h"

UShadeInitialHealthEffect::UShadeInitialHealthEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    FSetByCallerFloat InitialHealth;
    InitialHealth.DataTag = ShadeGameplayTags::Data_Health_Initial;

    FGameplayModifierInfo MaxHealthModifier;
    MaxHealthModifier.Attribute = UShadeAttributeSet::GetMaxHealthAttribute();
    MaxHealthModifier.ModifierOp = EGameplayModOp::Override;
    MaxHealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(InitialHealth);
    Modifiers.Add(MaxHealthModifier);

    FGameplayModifierInfo HealthModifier = MaxHealthModifier;
    HealthModifier.Attribute = UShadeAttributeSet::GetHealthAttribute();
    Modifiers.Add(HealthModifier);
}
