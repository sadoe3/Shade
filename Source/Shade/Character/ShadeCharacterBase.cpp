#include "ShadeCharacterBase.h"

#include "AbilitySystemComponent.h"

UAbilitySystemComponent* AShadeCharacterBase::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

void AShadeCharacterBase::ConnectAbilitySystem(UAbilitySystemComponent* NewAbilitySystem)
{
    if (AbilitySystem != NewAbilitySystem)
    {
        DisconnectAbilitySystem();
        AbilitySystem = NewAbilitySystem;
    }
}

void AShadeCharacterBase::DisconnectAbilitySystem()
{
    // An old body must not clear an ASC that has already moved to a new Avatar.
    if (IsValid(AbilitySystem) && AbilitySystem->GetAvatarActor() == this)
    {
        AbilitySystem->ClearActorInfo();
    }
    AbilitySystem = nullptr;
}

void AShadeCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DisconnectAbilitySystem();
    Super::EndPlay(EndPlayReason);
}
