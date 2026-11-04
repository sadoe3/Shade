#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "ShadePlayerState.generated.h"

class UAbilitySystemComponent;
class UShadeAttributeSet;
class UShadeInitialAttributesData;

UCLASS()
class SHADE_API AShadePlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AShadePlayerState();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    UShadeAttributeSet* GetAttributeSet() const;
    const UShadeInitialAttributesData* GetInitialAttributesData() const;

    // Refreshes the Avatar; initial attributes are applied once on authority.
    void InitializeAbilitySystem(AActor* Avatar);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystem;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UShadeAttributeSet> AttributeSet;

    UPROPERTY(EditDefaultsOnly, Category="GAS")
    TObjectPtr<UShadeInitialAttributesData> InitialAttributesData;

private:
    bool bAttributesInitialized = false;
};
