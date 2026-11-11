#include "ShadeAwarenessComponent.h"
#include "AbilitySystemComponent.h"
#include "../AbilitySystem/ShadeAwarenessEffects.h"
#include "../AbilitySystem/ShadeAwarenessSet.h"
#include "../AbilitySystem/ShadeGameplayTags.h"

bool UShadeAwarenessComponent::Initialize(UAbilitySystemComponent* InAbilitySystem)
{
    if (AbilitySystem.IsValid()) return AbilitySystem.Get() == InAbilitySystem;
    if (!IsValid(InAbilitySystem) || !InAbilitySystem->GetOwner() || !InAbilitySystem->GetOwner()->HasAuthority()) return false;
    if (InAbilitySystem->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead)) return false;
    if (!ensure(InAbilitySystem->GetSet<UShadeAwarenessSet>())) return false;
    // The owner initializes values first. This observer never resets live attributes.
    AbilitySystem = InAbilitySystem;
    AlertnessHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(UShadeAwarenessSet::GetAlertnessAttribute())
        .AddUObject(this, &ThisClass::OnAttributeChanged);
    // MaxAlertness alone can change the state: 100/100 is Alert, but 100/200 is Suspicious.
    // Raising the maximum preserves Alertness, so its change delegate will not notify us.
    MaximumHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(UShadeAwarenessSet::GetMaxAlertnessAttribute())
        .AddUObject(this, &ThisClass::OnAttributeChanged);
    DeadHandle = InAbilitySystem->RegisterGameplayTagEvent(ShadeGameplayTags::State_Dead)
        .AddUObject(this, &ThisClass::OnDeadTagChanged);
    UpdateState();
    return true;
}
void UShadeAwarenessComponent::OnAttributeChanged(const FOnAttributeChangeData& Change)
{
    UpdateState();
}

void UShadeAwarenessComponent::UpdateState()
{
    auto* ASC = AbilitySystem.Get();
    const auto* Attributes = ASC ? ASC->GetSet<UShadeAwarenessSet>() : nullptr;
    if (!Attributes) return;
    const auto NewState = Attributes->GetAlertness() <= 0.f ? EShadeAwarenessState::Calm :
        Attributes->GetAlertness() >= Attributes->GetMaxAlertness() ? EShadeAwarenessState::Alert : EShadeAwarenessState::Suspicious;
    if (StateEffect.IsValid() && NewState == CurrentState) return;
    if (StateEffect.IsValid()) ASC->RemoveActiveGameplayEffect(StateEffect);
    StateEffect.Invalidate();
    CurrentState = NewState;
    const TSubclassOf<UGameplayEffect> EffectClass = NewState == EShadeAwarenessState::Calm ? UShadeAwarenessCalmEffect::StaticClass() :
        NewState == EShadeAwarenessState::Alert ? UShadeAwarenessAlertEffect::StaticClass() : UShadeAwarenessSuspiciousEffect::StaticClass();
    auto Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, ASC->MakeEffectContext());
    if (ensure(Spec.IsValid())) StateEffect = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UShadeAwarenessComponent::OnDeadTagChanged(const FGameplayTag Tag, int32 Count)
{
    if (Count > 0) Shutdown();
}

void UShadeAwarenessComponent::Shutdown()
{
    auto* ASC = AbilitySystem.Get();
    if (ASC)
    {
        ASC->GetGameplayAttributeValueChangeDelegate(UShadeAwarenessSet::GetAlertnessAttribute()).Remove(AlertnessHandle);
        ASC->GetGameplayAttributeValueChangeDelegate(UShadeAwarenessSet::GetMaxAlertnessAttribute()).Remove(MaximumHandle);
        ASC->RegisterGameplayTagEvent(ShadeGameplayTags::State_Dead).Remove(DeadHandle);

        if (StateEffect.IsValid()) ASC->RemoveActiveGameplayEffect(StateEffect);
    }
    StateEffect.Invalidate();
    AlertnessHandle.Reset();
    MaximumHandle.Reset();
    DeadHandle.Reset();
    AbilitySystem.Reset();

}

void UShadeAwarenessComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Shutdown();
    Super::EndPlay(EndPlayReason);
}
