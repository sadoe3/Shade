#include "ShadeAttributeSet.h"

#include "Net/UnrealNetwork.h"

UShadeAttributeSet::UShadeAttributeSet()
{
    // GameplayEffects will initialize the live player's health in a later step.
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
