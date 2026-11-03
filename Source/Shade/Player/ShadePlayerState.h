#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "ShadePlayerState.generated.h"

class UAbilitySystemComponent;
class UShadeAttributeSet;

UCLASS()
class SHADE_API AShadePlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AShadePlayerState();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    UShadeAttributeSet* GetAttributeSet() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystem;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UShadeAttributeSet> AttributeSet;
};
