#include "ShadeEnemyCharacter.h"
#include "../AI/ShadeAwarenessComponent.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "../AbilitySystem/Data/ShadeInitialAttributesData.h"
#include "../AbilitySystem/ShadeAttributeSet.h"
#include "../AbilitySystem/ShadeAwarenessSet.h"
#include "../AbilitySystem/ShadeAwarenessEffects.h"
#include "../AbilitySystem/Data/ShadeEnemyBehaviorData.h"
#include "../AbilitySystem/ShadeGameplayTags.h"
#include "../AbilitySystem/ShadeHealthEffects.h"

AShadeEnemyCharacter::AShadeEnemyCharacter()
{
    OwnedAbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    OwnedAbilitySystem->SetIsReplicated(true);
    OwnedAbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
    AttributeSet = CreateDefaultSubobject<UShadeAttributeSet>(TEXT("AttributeSet"));
    AwarenessSet = CreateDefaultSubobject<UShadeAwarenessSet>(TEXT("AwarenessSet"));

    AwarenessComponent = CreateDefaultSubobject<UShadeAwarenessComponent>(TEXT("AwarenessComponent"));

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
    GetCharacterMovement()->MaxWalkSpeed = 0.f;
}

UAbilitySystemComponent* AShadeEnemyCharacter::GetAbilitySystemComponent() const
{
    return OwnedAbilitySystem;
}

UShadeAttributeSet* AShadeEnemyCharacter::GetAttributeSet() const
{
    return AttributeSet;
}

const UShadeInitialAttributesData* AShadeEnemyCharacter::GetInitialAttributesData() const
{
    return InitialAttributesData;
}

void AShadeEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();
    InitializeAbilitySystem();
}

void AShadeEnemyCharacter::InitializeAbilitySystem()
{
    OwnedAbilitySystem->InitAbilityActorInfo(this, this);
    if (!HasAuthority() || bAttributesInitialized)
    {
        ConnectAbilitySystem(OwnedAbilitySystem);
        if (HasAuthority() && bAttributesInitialized) InitializeAwareness();
        return;
    }

    if (!IsValid(InitialAttributesData))
    {
        UE_LOG(LogTemp, Error, TEXT("%s: Enemy InitialAttributesData must be assigned."), *GetName());
        return;
    }

    const float InitialHealth = InitialAttributesData->InitialHealth;
    if (!FMath::IsFinite(InitialHealth) || InitialHealth < 1.f)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: Enemy InitialHealth must be finite and at least 1."), *GetName());
        return;
    }
    if (!FMath::IsFinite(InitialAttributesData->WalkSpeed) || InitialAttributesData->WalkSpeed < 1.f ||
        !FMath::IsFinite(InitialAttributesData->RunSpeed) || InitialAttributesData->RunSpeed < 1.f ||
        !FMath::IsFinite(InitialAttributesData->BrakingDecelerationWalking) ||
        InitialAttributesData->BrakingDecelerationWalking < 0.f)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: Enemy movement settings must be finite, speeds at least 1, and braking non-negative."), *GetName());
        return;
    }

    FGameplayEffectSpecHandle Spec = OwnedAbilitySystem->MakeOutgoingSpec(
        UShadeInitialHealthEffect::StaticClass(), 1.f, OwnedAbilitySystem->MakeEffectContext());
    if (!ensure(Spec.IsValid()))
    {
        return;
    }
    Spec.Data->SetSetByCallerMagnitude(ShadeGameplayTags::Data_Health_Initial, InitialHealth);
    bAttributesInitialized = OwnedAbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();
    if (bAttributesInitialized)
    {
        GetCharacterMovement()->MaxWalkSpeed = InitialAttributesData->WalkSpeed;
        GetCharacterMovement()->BrakingDecelerationWalking = InitialAttributesData->BrakingDecelerationWalking;
        // Shared life-state observers see initialized Health, not the zero placeholder.
        ConnectAbilitySystem(OwnedAbilitySystem);
        if (HasAuthority() && bAttributesInitialized) InitializeAwareness();
    }
}

void AShadeEnemyCharacter::InitializeAwareness()
{
    if (bAwarenessInitialized)
    {
        AwarenessComponent->Initialize(OwnedAbilitySystem);
        return;
    }
    if (!IsValid(BehaviorData))
    {
        UE_LOG(LogTemp, Error, TEXT("%s: Enemy BehaviorData must be assigned."), *GetName());
        return;
    }
    if (!FMath::IsFinite(BehaviorData->MaxAlertness) || BehaviorData->MaxAlertness < 1.f)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: Enemy MaxAlertness must be finite and at least 1."), *GetName());
        return;
    }
    const auto Spec = OwnedAbilitySystem->MakeOutgoingSpec(
        UShadeInitialAwarenessEffect::StaticClass(), 1.f, OwnedAbilitySystem->MakeEffectContext());
    if (!ensure(Spec.IsValid())) return;
    Spec.Data->SetSetByCallerMagnitude(ShadeGameplayTags::Data_Awareness_Maximum, BehaviorData->MaxAlertness);
    bAwarenessInitialized = OwnedAbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();
    if (bAwarenessInitialized) AwarenessComponent->Initialize(OwnedAbilitySystem);
}
