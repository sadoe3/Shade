#include "ShadeAttributeSet.h"

#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

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

void UShadeAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        // Also normalize BaseValue after instant execution; current-value clamping alone is insufficient.
        const float Clamped = FMath::Clamp(GetHealth(), 0.f, GetMaxHealth());
        if (Health.GetBaseValue() != Clamped)
        {
            SetHealth(Clamped);
        }
    }
}
