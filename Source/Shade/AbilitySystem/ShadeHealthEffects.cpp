#include "AbilitySystem/ShadeHealthEffects.h"
#include "AbilitySystem/ShadeGameplayTags.h"
#include "AbilitySystem/ShadeHealthSet.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UShadeInitialHealthEffect::UShadeInitialHealthEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FSetByCallerFloat Initial;
    Initial.DataTag = ShadeGameplayTags::Data_Health_Initial;
    FGameplayModifierInfo Maximum;
    Maximum.Attribute = UShadeHealthSet::GetMaxHealthAttribute();
    Maximum.ModifierOp = EGameplayModOp::Override;
    Maximum.ModifierMagnitude = FGameplayEffectModifierMagnitude(Initial);
    Modifiers.Add(Maximum);
    FGameplayModifierInfo Current = Maximum;
    Current.Attribute = UShadeHealthSet::GetHealthAttribute();
    Modifiers.Add(Current);
}

UShadeHealthChangeEffect::UShadeHealthChangeEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FSetByCallerFloat Change;
    Change.DataTag = ShadeGameplayTags::Data_Health_Change;
    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UShadeHealthSet::GetHealthAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Change);
    Modifiers.Add(Modifier);
}

UShadeDeadEffect::UShadeDeadEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    auto* Tags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("DeadTag"));
    GEComponents.Add(Tags);
    FInheritedTagContainer Changes;
    Changes.AddTag(ShadeGameplayTags::State_Dead);
    Tags->SetAndApplyTargetTagChanges(Changes);
}
