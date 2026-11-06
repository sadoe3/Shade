#include "ShadeCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "../AbilitySystem/ShadeGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

UAbilitySystemComponent* AShadeCharacterBase::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

bool AShadeCharacterBase::IsDead() const
{
    return IsValid(AbilitySystem) && AbilitySystem->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead);
}

void AShadeCharacterBase::ConnectAbilitySystem(UAbilitySystemComponent* NewAbilitySystem)
{
    if (AbilitySystem == NewAbilitySystem) return;
    DisconnectAbilitySystem();
    AbilitySystem = NewAbilitySystem;
    if (IsValid(AbilitySystem))
    {
        DeadTagHandle = AbilitySystem->RegisterGameplayTagEvent(ShadeGameplayTags::State_Dead)
            .AddUObject(this, &AShadeCharacterBase::HandleDeadTagChanged);
        // The ASC may already be dead when a replacement body connects. Health may still be uninitialized.
        if (IsDead()) HandleDeath();
    }
}

void AShadeCharacterBase::Restart()
{
    Super::Restart();
    // ACharacter::Restart restores the default movement mode after possession.
    if (IsDead()) GetCharacterMovement()->DisableMovement();
}

void AShadeCharacterBase::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0) HandleDeath();
}

void AShadeCharacterBase::HandleDeath()
{
    if (HasAuthority() && IsValid(AbilitySystem)) AbilitySystem->CancelAllAbilities();
    ConsumeMovementInputVector();
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
}

void AShadeCharacterBase::DisconnectAbilitySystem()
{
    if (IsValid(AbilitySystem))
    {
        AbilitySystem->RegisterGameplayTagEvent(ShadeGameplayTags::State_Dead).Remove(DeadTagHandle);
        // An old body must not clear an ASC that has already moved to a new Avatar.
        if (AbilitySystem->GetAvatarActor() == this) AbilitySystem->ClearActorInfo();
    }
    DeadTagHandle.Reset();
    AbilitySystem = nullptr;
}

void AShadeCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DisconnectAbilitySystem();
    Super::EndPlay(EndPlayReason);
}
