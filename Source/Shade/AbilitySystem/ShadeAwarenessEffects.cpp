#include "ShadeAwarenessEffects.h"
#include "ShadeAwarenessSet.h"
#include "ShadeGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

namespace
{
    // Configures the GE Granted Tags in UE 5.8; applying/removing the GE grants/removes its tag contribution.
    // SetAndApplyTargetTagChanges updates the GE definition, not a target ASC at construction time.
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

UShadeAwarenessCalmEffect::UShadeAwarenessCalmEffect()
{
    GEComponents.Add(MakeStateTags(*this, ShadeGameplayTags::State_Awareness_Calm));
}

UShadeAwarenessSuspiciousEffect::UShadeAwarenessSuspiciousEffect()
{
    GEComponents.Add(MakeStateTags(*this, ShadeGameplayTags::State_Awareness_Suspicious));
}

UShadeAwarenessAlertEffect::UShadeAwarenessAlertEffect()
{
    GEComponents.Add(MakeStateTags(*this, ShadeGameplayTags::State_Awareness_Alert));
}
