#include "ShadeAwarenessComponent.h"

#include "AbilitySystemComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "../AbilitySystem/Data/ShadeEnemyBehaviorData.h"
#include "../AbilitySystem/ShadeAwarenessEffects.h"
#include "../AbilitySystem/ShadeAwarenessSet.h"
#include "../AbilitySystem/ShadeGameplayTags.h"
#include "../AbilitySystem/ShadeEnemyTags.h"

bool UShadeAwarenessComponent::Initialize(UAbilitySystemComponent* InAbilitySystem, UShadeEnemyBehaviorData* InSettings)
{
    if (AbilitySystem.IsValid())
    {
        return AbilitySystem.Get() == InAbilitySystem && Settings == InSettings;
    }
    if (!IsValid(InAbilitySystem) || !InAbilitySystem->GetOwner() || !InAbilitySystem->GetOwner()->HasAuthority()) return false;
    if (InAbilitySystem->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead)) return false;
    if (!IsValid(InSettings) || !InAbilitySystem->GetSet<UShadeAwarenessSet>())
    {
        UE_LOG(LogTemp, Error, TEXT("%s: Enemy awareness requires BehaviorData and an owned AwarenessSet."), *GetNameSafe(GetOwner()));
        return false;
    }
    if (!FMath::IsFinite(InSettings->MaxAlertness) || InSettings->MaxAlertness < 1.f ||
        !FMath::IsFinite(InSettings->AlertnessGainPerSecond) || InSettings->AlertnessGainPerSecond <= 0.f ||
        !FMath::IsFinite(InSettings->AlertnessDecayPerSecond) || InSettings->AlertnessDecayPerSecond <= 0.f ||
        !FMath::IsFinite(InSettings->AlertnessDecayDelay) || InSettings->AlertnessDecayDelay < 0.f ||
        !FMath::IsFinite(InSettings->AlertnessUpdatePeriod) || InSettings->AlertnessUpdatePeriod <= 0.f)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: Enemy awareness settings are invalid."), *GetNameSafe(GetOwner()));
        return false;
    }
    AbilitySystem = InAbilitySystem;
    Settings = InSettings;
    auto Spec = InAbilitySystem->MakeOutgoingSpec(UShadeInitialAwarenessEffect::StaticClass(), 1.f,
        InAbilitySystem->MakeEffectContext());
    if (!Spec.IsValid())
    {
        Shutdown();
        return false;
    }
    Spec.Data->SetSetByCallerMagnitude(ShadeEnemyTags::Data_Awareness_Maximum, Settings->MaxAlertness);
    if (!InAbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied())
    {
        Shutdown();
        return false;
    }
    AlertnessHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(UShadeAwarenessSet::GetAlertnessAttribute())
        .AddUObject(this, &ThisClass::OnAttributeChanged);
    MaximumHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(UShadeAwarenessSet::GetMaxAlertnessAttribute())
        .AddUObject(this, &ThisClass::OnAttributeChanged);
    DeadHandle = InAbilitySystem->RegisterGameplayTagEvent(ShadeGameplayTags::State_Dead)
        .AddUObject(this, &ThisClass::OnDeadTagChanged);
    UpdateState();
    return true;
}

void UShadeAwarenessComponent::SetTargetVisible(bool bVisible)
{
    if (!AbilitySystem.IsValid() || !Settings || bTargetVisible == bVisible) return;
    bTargetVisible = bVisible;
    GetWorld()->GetTimerManager().ClearTimer(DecayTimer);
    StopTrend();
    if (bVisible)
    {
        StartTrend(Settings->AlertnessGainPerSecond);
    }
    else if (Settings->AlertnessDecayDelay <= 0.f)
    {
        StartDecay();
    }
    else
    {
        GetWorld()->GetTimerManager().SetTimer(DecayTimer, this, &ThisClass::StartDecay,
            Settings->AlertnessDecayDelay, false);
    }
}

void UShadeAwarenessComponent::StartTrend(float ChangePerSecond)
{
    auto* ASC = AbilitySystem.Get();
    if (!ASC || !Settings) return;
    StopTrend();
    auto Spec = ASC->MakeOutgoingSpec(UShadeAlertnessTrendEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
    if (!ensure(Spec.IsValid())) return;
    Spec.Data->Period = Settings->AlertnessUpdatePeriod;
    Spec.Data->SetSetByCallerMagnitude(ShadeEnemyTags::Data_Awareness_Change,
        ChangePerSecond * Settings->AlertnessUpdatePeriod);
    TrendEffect = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UShadeAwarenessComponent::StopTrend()
{
    if (auto* ASC = AbilitySystem.Get(); ASC && TrendEffect.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(TrendEffect);
    }
    TrendEffect.Invalidate();
}

void UShadeAwarenessComponent::StartDecay()
{
    if (!bTargetVisible && Settings) StartTrend(-Settings->AlertnessDecayPerSecond);
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
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(DecayTimer);
    auto* ASC = AbilitySystem.Get();
    if (ASC)
    {
        ASC->GetGameplayAttributeValueChangeDelegate(UShadeAwarenessSet::GetAlertnessAttribute()).Remove(AlertnessHandle);
        ASC->GetGameplayAttributeValueChangeDelegate(UShadeAwarenessSet::GetMaxAlertnessAttribute()).Remove(MaximumHandle);
        ASC->RegisterGameplayTagEvent(ShadeGameplayTags::State_Dead).Remove(DeadHandle);
        StopTrend();
        if (StateEffect.IsValid()) ASC->RemoveActiveGameplayEffect(StateEffect);
    }
    StateEffect.Invalidate();
    AlertnessHandle.Reset();
    MaximumHandle.Reset();
    DeadHandle.Reset();
    AbilitySystem.Reset();
    Settings = nullptr;
    bTargetVisible = false;
}

void UShadeAwarenessComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Shutdown();
    Super::EndPlay(EndPlayReason);
}
