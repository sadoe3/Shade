#include "ShadeAttributeSet.h"

#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "ShadeGameplayTags.h"
#include "ShadeHealthEffects.h"

UShadeAttributeSet::UShadeAttributeSet()
{
    // Zero Health is the uninitialized state; the initial GE supplies the live values.
    InitMaxHealth(100.f);
    InitHealth(0.f);
}

void UShadeAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(UShadeAttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UShadeAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UShadeAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue) const
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UShadeAttributeSet, Health, OldValue);
}

void UShadeAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue) const
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UShadeAttributeSet, MaxHealth, OldValue);
}

void UShadeAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::IsFinite(NewValue) ? FMath::Clamp(NewValue, 0.f, GetMaxHealth()) : 0.f;
    }
    else if (Attribute == GetMaxHealthAttribute())
    {
        NewValue = FMath::IsFinite(NewValue) ? FMath::Max(NewValue, 1.f) : 1.f;
    }
}

void UShadeAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    ClampAttribute(Attribute, NewValue);
}

void UShadeAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
    Super::PreAttributeBaseChange(Attribute, NewValue);
    ClampAttribute(Attribute, NewValue);
}

void UShadeAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
    Super::PostAttributeChange(Attribute, OldValue, NewValue);
    // Current Health has no duration modifier in this project.
    if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
    {
        SetHealth(NewValue);
    }
}

bool UShadeAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
    // Revival is not implemented: a dead ASC cannot receive damage or healing modifiers.
    if (Data.EvaluatedData.Attribute == GetHealthAttribute() &&
        Data.Target.HasMatchingGameplayTag(ShadeGameplayTags::State_Dead)) return false;
    return Super::PreGameplayEffectExecute(Data);
}

void UShadeAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
    // GAS already applied the Health modifier; the Pre callbacks enforce its bounds.
    // An uninitialized zero is not death. Only a completed Health GE enters this path.
    if (Data.EvaluatedData.Attribute == GetHealthAttribute() && GetHealth() <= 0.f &&
        Data.Target.IsOwnerActorAuthoritative() &&
        !Data.Target.HasMatchingGameplayTag(ShadeGameplayTags::State_Dead))
    {
        Data.Target.ApplyGameplayEffectToSelf(GetDefault<UShadeDeadEffect>(), 1.f,
            Data.EffectSpec.GetContext());
    }
}
