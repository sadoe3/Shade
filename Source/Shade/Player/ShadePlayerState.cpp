#include "ShadePlayerState.h"
#include "../AbilitySystem/ShadeAssassinateAbility.h"
#include "../AbilitySystem/Data/ShadeAssassinationData.h"

#include "AbilitySystemComponent.h"
#include "../AbilitySystem/ShadeAttributeSet.h"
#include "../AbilitySystem/ShadeGameplayTags.h"
#include "../AbilitySystem/ShadeHealthEffects.h"
#include "../AbilitySystem/Data/ShadeInitialAttributesData.h"

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

const UShadeInitialAttributesData* AShadePlayerState::GetInitialAttributesData() const
{
    return InitialAttributesData;
}

void AShadePlayerState::InitializeAbilitySystem(AActor* Avatar)
{
    if (!IsValid(Avatar))
    {
        return;
    }

    AbilitySystem->InitAbilityActorInfo(this, Avatar);

    if (HasAuthority() && AssassinationData &&
        !AbilitySystem->FindAbilitySpecFromClass(UShadeAssassinateAbility::StaticClass()))
    {
        AbilitySystem->GiveAbility(FGameplayAbilitySpec(UShadeAssassinateAbility::StaticClass(), 1, INDEX_NONE, AssassinationData));
    }

    if (!HasAuthority() || bAttributesInitialized)
    {
        return;
    }

    if (!IsValid(InitialAttributesData))
    {
        UE_LOG(LogTemp, Error, TEXT("%s: InitialAttributesData must be assigned."), *GetName());
        return;
    }

    const float InitialHealth = InitialAttributesData->InitialHealth;
    if (!FMath::IsFinite(InitialHealth) || InitialHealth < 1.f)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: InitialHealth must be finite and at least 1."),
            *GetNameSafe(InitialAttributesData));
        return;
    }

    FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(
        UShadeInitialHealthEffect::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
    if (!ensure(Spec.IsValid()))
    {
        return;
    }

    Spec.Data->SetSetByCallerMagnitude(ShadeGameplayTags::Data_Health_Initial, InitialHealth);
    const FActiveGameplayEffectHandle Result = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    // Instant effects do not retain a valid active handle after successful execution.
    bAttributesInitialized = Result.WasSuccessfullyApplied();
}
