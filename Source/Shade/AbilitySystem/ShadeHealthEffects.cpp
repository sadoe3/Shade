#include "ShadeHealthEffects.h"

#include "ShadeGameplayTags.h"
#include "ShadeAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

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

UShadeHealthChangeEffect::UShadeHealthChangeEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FSetByCallerFloat Delta;
    Delta.DataTag = ShadeGameplayTags::Data_Health_Change;
    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UShadeAttributeSet::GetHealthAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Delta);
    Modifiers.Add(Modifier);
}

UShadeDeadEffect::UShadeDeadEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    FInheritedTagContainer GrantedTags;
    GrantedTags.AddTag(ShadeGameplayTags::State_Dead);
    auto* DeadTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("DeadTags"));
    GEComponents.Add(DeadTags);
    DeadTags->SetAndApplyTargetTagChanges(GrantedTags);
}

bool ShadeHealthEffects::ApplyHealthChange(UAbilitySystemComponent* Target, float Delta)
{
    if (!IsValid(Target) || !Target->IsOwnerActorAuthoritative() || !FMath::IsFinite(Delta) ||
        !Target->GetSet<UShadeAttributeSet>() ||
        Target->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead)) return false;
    FGameplayEffectSpecHandle Spec = Target->MakeOutgoingSpec(
        UShadeHealthChangeEffect::StaticClass(), 1.f, Target->MakeEffectContext());
    if (!Spec.IsValid()) return false;
    Spec.Data->SetSetByCallerMagnitude(ShadeGameplayTags::Data_Health_Change, Delta);
    return Target->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();
}
