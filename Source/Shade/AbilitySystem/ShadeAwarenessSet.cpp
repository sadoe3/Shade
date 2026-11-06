#include "ShadeAwarenessSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UShadeAwarenessSet::UShadeAwarenessSet()
{
    InitAlertness(0.f);
    InitMaxAlertness(100.f);
}

void UShadeAwarenessSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(UShadeAwarenessSet, Alertness, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UShadeAwarenessSet, MaxAlertness, COND_None, REPNOTIFY_Always);
}

void UShadeAwarenessSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
    if (Attribute == GetAlertnessAttribute())
    {
        NewValue = FMath::IsFinite(NewValue) ? FMath::Clamp(NewValue, 0.f, GetMaxAlertness()) : 0.f;
    }
    else if (Attribute == GetMaxAlertnessAttribute())
    {
        NewValue = FMath::IsFinite(NewValue) ? FMath::Max(NewValue, 1.f) : 1.f;
    }
}

void UShadeAwarenessSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    ClampAttribute(Attribute, NewValue);
}

void UShadeAwarenessSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
    Super::PreAttributeBaseChange(Attribute, NewValue);
    ClampAttribute(Attribute, NewValue);
}

void UShadeAwarenessSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
    Super::PostAttributeChange(Attribute, OldValue, NewValue);
    if (Attribute == GetMaxAlertnessAttribute() && GetAlertness() > NewValue)
    {
        SetAlertness(NewValue);
    }
}

void UShadeAwarenessSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
    if (Data.EvaluatedData.Attribute == GetAlertnessAttribute())
    {
        const float Clamped = FMath::Clamp(GetAlertness(), 0.f, GetMaxAlertness());
        if (Alertness.GetBaseValue() != Clamped)
        {
            SetAlertness(Clamped);
        }
    }
}

void UShadeAwarenessSet::OnRep_Alertness(const FGameplayAttributeData& OldValue) const
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UShadeAwarenessSet, Alertness, OldValue);
}

void UShadeAwarenessSet::OnRep_MaxAlertness(const FGameplayAttributeData& OldValue) const
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UShadeAwarenessSet, MaxAlertness, OldValue);
}
