#include "ShadeAssassinateAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "ShadeAttributeSet.h"
#include "ShadeGameplayTags.h"
#include "ShadeStealthTags.h"
#include "Data/ShadeAssassinationData.h"
#include "../Character/ShadeEnemyCharacter.h"
#include "../AbilitySystem/ShadeEnemyTags.h"
#include "Engine/World.h"

UShadeAssassinationEffect::UShadeAssassinationEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UShadeAttributeSet::GetHealthAttribute();
    Modifier.ModifierOp = EGameplayModOp::Override;
    Modifier.ModifierMagnitude = FScalableFloat(0.f);
    Modifiers.Add(Modifier);
}
UShadeAssassinateAbility::UShadeAssassinateAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    SetAssetTags(FGameplayTagContainer(ShadeStealthTags::Ability_Assassinate));
    ActivationBlockedTags.AddTag(ShadeGameplayTags::State_Dead);
    ActivationBlockedTags.AddTag(ShadeStealthTags::State_Action_Assassinating);
    ActivationOwnedTags.AddTag(ShadeStealthTags::State_Action_Assassinating);
    FAbilityTriggerData Trigger;
    Trigger.TriggerTag = ShadeStealthTags::Event_Assassinate;
    Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
    AbilityTriggers.Add(Trigger);
}
bool UShadeAssassinateAbility::IsValidTarget(AActor* Attacker, AActor* Target, float Range)
{
    if (!IsValid(Attacker) || !IsValid(Target) || Attacker == Target || !Attacker->HasAuthority() ||
        !Target->IsA<AShadeEnemyCharacter>() || !FMath::IsFinite(Range) || Range <= 0.f ||
        Attacker->GetWorld() != Target->GetWorld() ||
        FVector::DistSquared(Attacker->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(Range)) return false;
    auto* Source = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Attacker);
    auto* Victim = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
    if (!Source || !Victim || Source->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead) ||
        Victim->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead) ||
        Victim->HasMatchingGameplayTag(ShadeEnemyTags::State_Awareness_Alert) ||
        Victim->HasMatchingGameplayTag(ShadeStealthTags::State_Action_BeingAssassinated) ||
        Source->GetNumericAttribute(UShadeAttributeSet::GetHealthAttribute()) <= 0.f ||
        Victim->GetNumericAttribute(UShadeAttributeSet::GetHealthAttribute()) <= 0.f) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(AssassinationVisibility), false, Attacker);
    Params.AddIgnoredActor(Target);
    return !Attacker->GetWorld()->LineTraceTestByChannel(Attacker->GetActorLocation(), Target->GetActorLocation(),
        ECC_Visibility, Params);
}
bool UShadeAssassinateAbility::ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayEventData* Payload) const
{
    if (!ActorInfo || !Payload) return false;
    const auto* Settings = Cast<UShadeAssassinationData>(GetSourceObject(CurrentSpecHandle, ActorInfo));
    return Super::ShouldAbilityRespondToEvent(ActorInfo, Payload) && Settings &&
        IsValidTarget(ActorInfo->AvatarActor.Get(), const_cast<AActor*>(Payload->Target.Get()), Settings->Range);
}
void UShadeAssassinateAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    const auto* Settings = Cast<UShadeAssassinationData>(GetSourceObject(Handle, ActorInfo));
    AActor* Target = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
    if (!Settings || !ActorInfo || !IsValidTarget(ActorInfo->AvatarActor.Get(), Target, Settings->Range) ||
        !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    auto* Victim = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
    Victim->AddLooseGameplayTag(ShadeStealthTags::State_Action_BeingAssassinated);
    auto* Source = ActorInfo->AbilitySystemComponent.Get();
    const auto Spec = Source->MakeOutgoingSpec(UShadeAssassinationEffect::StaticClass(), 1.f, Source->MakeEffectContext());
    bool bApplied = false;
    if (Spec.IsValid())
        bApplied = Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Victim).WasSuccessfullyApplied();
    Victim->RemoveLooseGameplayTag(ShadeStealthTags::State_Action_BeingAssassinated);
    EndAbility(Handle, ActorInfo, ActivationInfo, true, !bApplied);
}
