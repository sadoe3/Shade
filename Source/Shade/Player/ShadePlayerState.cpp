#include "ShadePlayerState.h"

#include "AbilitySystemComponent.h"
#include "../AbilitySystem/ShadeAttributeSet.h"

AShadePlayerState::AShadePlayerState()
{
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UShadeAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AShadePlayerState::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

UShadeAttributeSet* AShadePlayerState::GetAttributeSet() const
{
    return AttributeSet;
}
