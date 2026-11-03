#include "ShadeHealthEffects.h"

#include "ShadeGameplayTags.h"
#include "ShadeHealthSet.h"

UShadeInitialHealthEffect::UShadeInitialHealthEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    FSetByCallerFloat InitialHealth;
    InitialHealth.DataTag = ShadeGameplayTags::Data_Health_Initial;

    FGameplayModifierInfo MaxHealthModifier;
    MaxHealthModifier.Attribute = UShadeHealthSet::GetMaxHealthAttribute();
    MaxHealthModifier.ModifierOp = EGameplayModOp::Override;
    MaxHealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(InitialHealth);
    Modifiers.Add(MaxHealthModifier);

    FGameplayModifierInfo HealthModifier = MaxHealthModifier;
    HealthModifier.Attribute = UShadeHealthSet::GetHealthAttribute();
    Modifiers.Add(HealthModifier);
}
