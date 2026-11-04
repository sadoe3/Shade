#include "ShadePlayerCharacter.h"

#include "../Player/ShadePlayerState.h"
#include "../AbilitySystem/Data/ShadeInitialAttributesData.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

AShadePlayerCharacter::AShadePlayerCharacter()
{
    // The camera follows control rotation; the body faces its movement direction.
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
    // Movement limits become available when PlayerState is connected.
    GetCharacterMovement()->MaxWalkSpeed = 0.f;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 350.f;
    CameraBoom->SocketOffset = FVector(0.f, 45.f, 60.f);
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;
}

void AShadePlayerCharacter::SetRunning(bool bRunning)
{
    const auto* State = GetPlayerState<AShadePlayerState>();
    if (!IsValid(State))
    {
        return;
    }

    const auto* Settings = State->GetInitialAttributesData();
    if (!IsValid(Settings) || !FMath::IsFinite(Settings->WalkSpeed) || Settings->WalkSpeed < 1.f ||
        !FMath::IsFinite(Settings->RunSpeed) || Settings->RunSpeed < 1.f)
    {
        GetCharacterMovement()->MaxWalkSpeed = 0.f;
        UE_LOG(LogTemp, Error, TEXT("%s: InitialAttributesData must provide finite WalkSpeed and RunSpeed of at least 1."),
            *GetName());
        return;
    }

    if (!FMath::IsFinite(Settings->BrakingDecelerationWalking) || Settings->BrakingDecelerationWalking < 0.f)
    {
        GetCharacterMovement()->MaxWalkSpeed = 0.f;
        UE_LOG(LogTemp, Error, TEXT("%s: BrakingDecelerationWalking must be finite and non-negative."), *GetName());
        return;
    }

    GetCharacterMovement()->BrakingDecelerationWalking = Settings->BrakingDecelerationWalking;
    GetCharacterMovement()->MaxWalkSpeed = bRunning ? Settings->RunSpeed : Settings->WalkSpeed;
}

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
    SetRunning(false);
}

void AShadePlayerCharacter::UnPossessed()
{
    SetRunning(false);
    DisconnectAbilitySystem();
    Super::UnPossessed();
}
