#include "Character/ShadePlayerCharacter.h"
#include "Player/ShadePlayerState.h"
#include "Player/ShadePlayerController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ShadeGameplayTags.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

AShadePlayerCharacter::AShadePlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = false;
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 350.f;
    CameraBoom->SocketOffset = FVector(0.f, 45.f, 60.f);
    CameraBoom->bUsePawnControlRotation = true;
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    // Native defaults make the prototype runnable; Blueprint may select authored assets later.
    MoveAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveAction"));
    MoveAction->ValueType = EInputActionValueType::Axis2D;
    LookAction = CreateDefaultSubobject<UInputAction>(TEXT("LookAction"));
    LookAction->ValueType = EInputActionValueType::Axis2D;
    RunAction = CreateDefaultSubobject<UInputAction>(TEXT("RunAction"));
    RunAction->ValueType = EInputActionValueType::Boolean;
    PlayerMappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("PlayerMappingContext"));
    auto* Negate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("NegativeAxis"));
    auto* Swizzle = CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("ForwardAxis"));
    Swizzle->Order = EInputAxisSwizzle::YXZ;
    PlayerMappingContext->MapKey(MoveAction, EKeys::D);
    PlayerMappingContext->MapKey(MoveAction, EKeys::A).Modifiers.Add(Negate);
    PlayerMappingContext->MapKey(MoveAction, EKeys::W).Modifiers.Add(Swizzle);
    auto& Back = PlayerMappingContext->MapKey(MoveAction, EKeys::S);
    Back.Modifiers.Add(Negate);
    Back.Modifiers.Add(Swizzle);
    PlayerMappingContext->MapKey(LookAction, EKeys::Mouse2D);
    PlayerMappingContext->MapKey(RunAction, EKeys::LeftShift);
}

void AShadePlayerCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitializeAbilitySystem();
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}
void AShadePlayerCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    InitializeAbilitySystem();
}
void AShadePlayerCharacter::InitializeAbilitySystem()
{
    if (AShadePlayerState* State = GetPlayerState<AShadePlayerState>())
    {
        State->InitializeAbilitySystem(this);
        ConnectAbilitySystem(State->GetAbilitySystemComponent());
        if (auto* PC = Cast<AShadePlayerController>(GetController())) PC->RefreshHealthHUD();
    }
}
void AShadePlayerCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();
    AddInputMapping();
}
void AShadePlayerCharacter::AddInputMapping()
{
    auto* PC = Cast<APlayerController>(GetController());
    if (!PC || !PC->IsLocalController()) return;
    auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
    if (!Subsystem || !PlayerMappingContext) return;
    RemoveInputMapping();
    Subsystem->AddMappingContext(PlayerMappingContext, 0);
    MappedSubsystem = Subsystem;
}
void AShadePlayerCharacter::RemoveInputMapping()
{
    if (MappedSubsystem.IsValid()) MappedSubsystem->RemoveMappingContext(PlayerMappingContext);
    MappedSubsystem.Reset();
}
void AShadePlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    auto* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
    Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
    Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
    Input->BindAction(RunAction, ETriggerEvent::Started, this, &ThisClass::StartRunning);
    Input->BindAction(RunAction, ETriggerEvent::Completed, this, &ThisClass::StopRunning);
    Input->BindAction(RunAction, ETriggerEvent::Canceled, this, &ThisClass::StopRunning);
    AddInputMapping();
}
void AShadePlayerCharacter::Move(const FInputActionValue& Value)
{
    if (!Controller || !AbilitySystem || AbilitySystem->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead)) return;
    const FVector2D Axis = Value.Get<FVector2D>();
    const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}
void AShadePlayerCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    AddControllerYawInput(Axis.X);
    AddControllerPitchInput(-Axis.Y);
}
void AShadePlayerCharacter::StartRunning()
{
    if (AbilitySystem && !AbilitySystem->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead))
        GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
}
void AShadePlayerCharacter::StopRunning() { GetCharacterMovement()->MaxWalkSpeed = WalkSpeed; }
void AShadePlayerCharacter::UnPossessed()
{
    RemoveInputMapping();
    StopRunning();
    if (AbilitySystem && AbilitySystem->GetAvatarActor() == this) AbilitySystem->ClearActorInfo();
    DisconnectAbilitySystem();
    Super::UnPossessed();
}
void AShadePlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveInputMapping();
    if (AbilitySystem && AbilitySystem->GetAvatarActor() == this) AbilitySystem->ClearActorInfo();
    Super::EndPlay(EndPlayReason);
}
