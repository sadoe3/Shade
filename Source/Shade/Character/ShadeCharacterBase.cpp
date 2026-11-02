#include "Character/ShadeCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ShadeGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

UAbilitySystemComponent* AShadeCharacterBase::GetAbilitySystemComponent() const { return AbilitySystem; }
void AShadeCharacterBase::ConnectAbilitySystem(UAbilitySystemComponent* NewAbilitySystem)
{
    DisconnectAbilitySystem();
    AbilitySystem = NewAbilitySystem;
    if (!AbilitySystem) return;
    DeadTagHandle = AbilitySystem->RegisterGameplayTagEvent(ShadeGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &ThisClass::OnDeadTagChanged);
    OnDeadTagChanged(ShadeGameplayTags::State_Dead, AbilitySystem->GetTagCount(ShadeGameplayTags::State_Dead));
}
void AShadeCharacterBase::DisconnectAbilitySystem()
{
    if (AbilitySystem)
    {
        AbilitySystem->RegisterGameplayTagEvent(ShadeGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved).Remove(DeadTagHandle);
    }
    DeadTagHandle.Reset();
    AbilitySystem = nullptr;
}
void AShadeCharacterBase::OnDeadTagChanged(const FGameplayTag Tag, int32 Count)
{
    if (Count > 0)
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
    }
    // Respawn/revival is deliberately not defined by this first Player milestone.
}
void AShadeCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DisconnectAbilitySystem();
    Super::EndPlay(EndPlayReason);
}
