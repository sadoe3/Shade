#pragma once

#include "CoreMinimal.h"
#include "ShadeCharacterBase.h"
#include "ShadePlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;

UCLASS()
class SHADE_API AShadePlayerCharacter : public AShadeCharacterBase
{
    GENERATED_BODY()

public:
    AShadePlayerCharacter();

    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;
    virtual void UnPossessed() override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

private:
    void InitializeAbilitySystem();
};
