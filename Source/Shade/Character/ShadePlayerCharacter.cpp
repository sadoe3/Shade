#include "ShadePlayerCharacter.h"

#include "../Player/ShadePlayerState.h"

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
    GetCharacterMovement()->MaxWalkSpeed = 250.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 350.f;
    CameraBoom->SocketOffset = FVector(0.f, 45.f, 60.f);
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;
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
}

void AShadePlayerCharacter::UnPossessed()
{
    DisconnectAbilitySystem();
    Super::UnPossessed();
}
