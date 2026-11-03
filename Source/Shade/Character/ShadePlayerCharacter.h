#pragma once

#include "CoreMinimal.h"
#include "ShadeCharacterBase.h"
#include "ShadePlayerCharacter.generated.h"

UCLASS()
class SHADE_API AShadePlayerCharacter : public AShadeCharacterBase
{
    GENERATED_BODY()

public:
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;
    virtual void UnPossessed() override;

private:
    void InitializeAbilitySystem();
};
