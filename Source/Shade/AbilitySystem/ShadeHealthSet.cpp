#include "ShadeHealthSet.h"

#include "Net/UnrealNetwork.h"

UShadeHealthSet::UShadeHealthSet()
{
    // GameplayEffects will initialize the live player's health in a later step.
    InitMaxHealth(100.f);
    InitHealth(0.f);
}

void UShadeHealthSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(UShadeHealthSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UShadeHealthSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UShadeHealthSet::OnRep_Health(const FGameplayAttributeData& OldValue) const
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UShadeHealthSet, Health, OldValue);
}

void UShadeHealthSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue) const
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UShadeHealthSet, MaxHealth, OldValue);
}
