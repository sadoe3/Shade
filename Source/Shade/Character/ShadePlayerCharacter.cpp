#include "ShadePlayerCharacter.h"

#include "../Player/ShadePlayerState.h"

void AShadePlayerCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitializeAbilitySystem();
}

void AShadePlayerCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    InitializeAbilitySystem();
}

void AShadePlayerCharacter::InitializeAbilitySystem()
{
    auto* State = GetPlayerState<AShadePlayerState>();
    if (!IsValid(State))
    {
        DisconnectAbilitySystem();
        return;
    }

    ConnectAbilitySystem(State->GetAbilitySystemComponent());
    State->InitializeAbilitySystem(this);
}

void AShadePlayerCharacter::UnPossessed()
{
    DisconnectAbilitySystem();
    Super::UnPossessed();
}
