#include "ShadeAwarenessEffects.h"
#include "ShadeAwarenessSet.h"
#include "ShadeGameplayTags.h"

UShadeInitialAwarenessEffect::UShadeInitialAwarenessEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FSetByCallerFloat Maximum;
    Maximum.DataTag = ShadeGameplayTags::Data_Awareness_Maximum;
    FGameplayModifierInfo MaxModifier;
    MaxModifier.Attribute = UShadeAwarenessSet::GetMaxAlertnessAttribute();
    MaxModifier.ModifierOp = EGameplayModOp::Override;
    MaxModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Maximum);
    Modifiers.Add(MaxModifier);

    // Initial awareness is zero; the maximum must be applied first.
    FGameplayModifierInfo InitialModifier;
    InitialModifier.Attribute = UShadeAwarenessSet::GetAlertnessAttribute();
    InitialModifier.ModifierOp = EGameplayModOp::Override;
    InitialModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.f));
    Modifiers.Add(InitialModifier);
}
