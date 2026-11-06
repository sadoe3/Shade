#include "ShadeAwarenessEffects.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "ShadeAwarenessSet.h"
#include "ShadeEnemyTags.h"

namespace
{
    UTargetTagsGameplayEffectComponent* MakeStateTags(UGameplayEffect& Effect, const FGameplayTag& Tag)
    {
        Effect.DurationPolicy = EGameplayEffectDurationType::Infinite;
        FInheritedTagContainer Tags;
        Tags.AddTag(Tag);
        auto* Component = Effect.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("StateTags"));
        Component->SetAndApplyTargetTagChanges(Tags);
        return Component;
    }
}

UShadeInitialAwarenessEffect::UShadeInitialAwarenessEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FSetByCallerFloat Maximum;
    Maximum.DataTag = ShadeEnemyTags::Data_Awareness_Maximum;
    FGameplayModifierInfo MaxModifier;
    MaxModifier.Attribute = UShadeAwarenessSet::GetMaxAlertnessAttribute();
    MaxModifier.ModifierOp = EGameplayModOp::Override;
    MaxModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Maximum);
    Modifiers.Add(MaxModifier);
    FGameplayModifierInfo InitialModifier;
    InitialModifier.Attribute = UShadeAwarenessSet::GetAlertnessAttribute();
    InitialModifier.ModifierOp = EGameplayModOp::Override;
    InitialModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.f));
    Modifiers.Add(InitialModifier);
}

UShadeAlertnessTrendEffect::UShadeAlertnessTrendEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    Period = FScalableFloat(0.1f);
    bExecutePeriodicEffectOnApplication = false;
    FSetByCallerFloat Change;
    Change.DataTag = ShadeEnemyTags::Data_Awareness_Change;
    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UShadeAwarenessSet::GetAlertnessAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Change);
    Modifiers.Add(Modifier);
}

UShadeAwarenessCalmEffect::UShadeAwarenessCalmEffect()
{
    GEComponents.Add(MakeStateTags(*this, ShadeEnemyTags::State_Awareness_Calm));
}

UShadeAwarenessSuspiciousEffect::UShadeAwarenessSuspiciousEffect()
{
    GEComponents.Add(MakeStateTags(*this, ShadeEnemyTags::State_Awareness_Suspicious));
}

UShadeAwarenessAlertEffect::UShadeAwarenessAlertEffect()
{
    GEComponents.Add(MakeStateTags(*this, ShadeEnemyTags::State_Awareness_Alert));
}
