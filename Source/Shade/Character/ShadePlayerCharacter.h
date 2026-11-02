#pragma once
#include "CoreMinimal.h"
#include "Character/ShadeCharacterBase.h"
#include "ShadePlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class SHADE_API AShadePlayerCharacter : public AShadeCharacterBase
{
    GENERATED_BODY()
public:
    AShadePlayerCharacter();
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;
    virtual void UnPossessed() override;
    virtual void PawnClientRestart() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    const UInputAction* GetMoveAction() const { return MoveAction; }
    const UInputAction* GetLookAction() const { return LookAction; }
    const UInputAction* GetRunAction() const { return RunAction; }
    float GetWalkSpeed() const { return WalkSpeed; }
    float GetRunSpeed() const { return RunSpeed; }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<UCameraComponent> FollowCamera;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1")) float WalkSpeed = 250.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1")) float RunSpeed = 550.f;
    UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputMappingContext> PlayerMappingContext;
    UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> MoveAction;
    UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> LookAction;
    UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> RunAction;

private:
    void InitializeAbilitySystem();
    void AddInputMapping();
    void RemoveInputMapping();
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void StartRunning();
    void StopRunning();
    TWeakObjectPtr<class UEnhancedInputLocalPlayerSubsystem> MappedSubsystem;
};
