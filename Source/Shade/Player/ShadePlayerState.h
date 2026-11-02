#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "ShadePlayerState.generated.h"

class UAbilitySystemComponent;
class UShadeHealthSet;
struct FOnAttributeChangeData;

UCLASS()
class SHADE_API AShadePlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    AShadePlayerState();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    UShadeHealthSet* GetHealthSet() const { return HealthSet; }
    void InitializeAbilitySystem(AActor* Avatar);
    bool ApplyHealthChange(float Amount);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystem;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UShadeHealthSet> HealthSet;
    UPROPERTY(EditDefaultsOnly, Category="GAS", meta=(ClampMin="1"))
    float InitialHealth = 100.f;

private:
    bool bAttributesInitialized = false;
    FDelegateHandle HealthChangedHandle;
    void OnHealthChanged(const FOnAttributeChangeData& Data);
};
