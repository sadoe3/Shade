#include "Player/ShadePlayerState.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ShadeHealthSet.h"
#include "AbilitySystem/ShadeHealthEffects.h"
#include "AbilitySystem/ShadeGameplayTags.h"

AShadePlayerState::AShadePlayerState()
{
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    HealthSet = CreateDefaultSubobject<UShadeHealthSet>(TEXT("HealthSet"));
}
UAbilitySystemComponent* AShadePlayerState::GetAbilitySystemComponent() const { return AbilitySystem; }

void AShadePlayerState::InitializeAbilitySystem(AActor* Avatar)
{
    if (!IsValid(Avatar)) return;
    AbilitySystem->InitAbilityActorInfo(this, Avatar);
    if (!HasAuthority()) return;
    if (!HealthChangedHandle.IsValid())
    {
        HealthChangedHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(UShadeHealthSet::GetHealthAttribute())
            .AddUObject(this, &ThisClass::OnHealthChanged);
    }
    if (!bAttributesInitialized)
    {
        FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UShadeInitialHealthEffect::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
        if (ensure(Spec.IsValid()))
        {
            Spec.Data->SetSetByCallerMagnitude(ShadeGameplayTags::Data_Health_Initial, FMath::Max(1.f, InitialHealth));
            AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
            bAttributesInitialized = true;
        }
    }
}

bool AShadePlayerState::ApplyHealthChange(float Amount)
{
    if (!HasAuthority() || !bAttributesInitialized || !FMath::IsFinite(Amount)
        || AbilitySystem->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead)) return false;
    FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UShadeHealthChangeEffect::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
    if (!Spec.IsValid()) return false;
    Spec.Data->SetSetByCallerMagnitude(ShadeGameplayTags::Data_Health_Change, Amount);
    AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    return true;
}

void AShadePlayerState::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    if (Data.NewValue > 0.f || Data.OldValue <= 0.f || AbilitySystem->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead)) return;
    FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UShadeDeadEffect::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
    if (Spec.IsValid())
    {
        AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        AbilitySystem->CancelAllAbilities();
    }
}

void AShadePlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    AbilitySystem->GetGameplayAttributeValueChangeDelegate(UShadeHealthSet::GetHealthAttribute()).Remove(HealthChangedHandle);
    Super::EndPlay(EndPlayReason);
}
