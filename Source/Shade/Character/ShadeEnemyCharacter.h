#pragma once

#include "CoreMinimal.h"
#include "ShadeCharacterBase.h"
#include "ShadeEnemyCharacter.generated.h"

class UAbilitySystemComponent;
class UShadeAttributeSet;
class UShadeAwarenessSet;
class UShadeInitialAttributesData;

UCLASS()
class SHADE_API AShadeEnemyCharacter : public AShadeCharacterBase
{
    GENERATED_BODY()

public:
    AShadeEnemyCharacter();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    UShadeAttributeSet* GetAttributeSet() const;
    UShadeAwarenessSet* GetAwarenessSet() const { return AwarenessSet; }
    const UShadeInitialAttributesData* GetInitialAttributesData() const;

    // Establishes this Enemy as both GAS Owner and Avatar. Initial data is applied once.
    void InitializeAbilitySystem();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UAbilitySystemComponent> OwnedAbilitySystem;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UShadeAttributeSet> AttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UShadeAwarenessSet> AwarenessSet;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UShadeInitialAttributesData> InitialAttributesData;

private:
    bool bAttributesInitialized = false;
};
