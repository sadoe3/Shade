#pragma once

#include "CoreMinimal.h"
#include "ShadeCharacterBase.h"
#include "ShadeEnemyCharacter.generated.h"

class UAbilitySystemComponent;
class UShadeAttributeSet;
class UShadeInitialAttributesData;
class UShadeAwarenessSet;
class UShadeAwarenessComponent;
class UShadeEnemyBehaviorData;

UCLASS()
class SHADE_API AShadeEnemyCharacter : public AShadeCharacterBase
{
    GENERATED_BODY()

public:
    AShadeEnemyCharacter();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    UShadeAttributeSet* GetAttributeSet() const;
    const UShadeInitialAttributesData* GetInitialAttributesData() const;
    UShadeAwarenessSet* GetAwarenessSet() const { return AwarenessSet; }
    UShadeAwarenessComponent* GetAwarenessComponent() const { return AwarenessComponent; }
    const UShadeEnemyBehaviorData* GetBehaviorData() const { return BehaviorData; }

    // Establishes this Enemy as both GAS Owner and Avatar. Initial data is applied once.
    void InitializeAbilitySystem();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UAbilitySystemComponent> OwnedAbilitySystem;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UShadeAttributeSet> AttributeSet;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UShadeInitialAttributesData> InitialAttributesData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UShadeAwarenessSet> AwarenessSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI")
    TObjectPtr<UShadeAwarenessComponent> AwarenessComponent;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI")
    TObjectPtr<UShadeEnemyBehaviorData> BehaviorData;

private:
    bool bAttributesInitialized = false;
};
