#include "ShadePlayerController.h"

#include "../Character/ShadePlayerCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

void AShadePlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    auto* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    if (!Subsystem || !PlayerMappingContext)
    {
        UE_LOG(LogTemp, Error, TEXT("%s requires a local input subsystem and PlayerMappingContext."), *GetName());
        return;
    }

    Subsystem->AddMappingContext(PlayerMappingContext, 0);
    RegisteredInputSubsystem = Subsystem;
    SetInputMode(FInputModeGameOnly());
}

void AShadePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    auto* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInput || !MoveAction || !LookAction || !RunAction ||
        MoveAction->ValueType != EInputActionValueType::Axis2D ||
        LookAction->ValueType != EInputActionValueType::Axis2D ||
        RunAction->ValueType != EInputActionValueType::Boolean)
    {
        UE_LOG(LogTemp, Error, TEXT("%s requires EnhancedInputComponent with Axis2D Move/Look and Boolean Run actions."), *GetName());
        return;
    }

    EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
    EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
    EnhancedInput->BindAction(RunAction, ETriggerEvent::Started, this, &ThisClass::StartRunning);
    EnhancedInput->BindAction(RunAction, ETriggerEvent::Completed, this, &ThisClass::StopRunning);
    EnhancedInput->BindAction(RunAction, ETriggerEvent::Canceled, this, &ThisClass::StopRunning);
}

void AShadePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopRunning();
    if (auto* Subsystem = RegisteredInputSubsystem.Get())
    {
        Subsystem->RemoveMappingContext(PlayerMappingContext);
    }
    RegisteredInputSubsystem.Reset();
    Super::EndPlay(EndPlayReason);
}

void AShadePlayerController::Move(const FInputActionValue& Value)
{
    if (APawn* ControlledPawn = GetPawn())
    {
        const FVector2D Input = Value.Get<FVector2D>();
        const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
        const FRotationMatrix Basis(YawRotation);
        ControlledPawn->AddMovementInput(Basis.GetUnitAxis(EAxis::X), Input.Y);
        ControlledPawn->AddMovementInput(Basis.GetUnitAxis(EAxis::Y), Input.X);
    }
}

void AShadePlayerController::Look(const FInputActionValue& Value)
{
    if (!GetPawn()) return;
    const FVector2D Input = Value.Get<FVector2D>();
    AddYawInput(Input.X);
    AddPitchInput(Input.Y);
}

void AShadePlayerController::StartRunning()
{
    if (auto* ControlledCharacter = Cast<AShadePlayerCharacter>(GetPawn()))
    {
        ControlledCharacter->SetRunning(true);
    }
}

void AShadePlayerController::StopRunning()
{
    if (auto* ControlledCharacter = Cast<AShadePlayerCharacter>(GetPawn()))
    {
        ControlledCharacter->SetRunning(false);
    }
}
