#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "ShadeCharacterBase.generated.h"

class UAbilitySystemComponent;

UCLASS(Abstract)
class SHADE_API AShadeCharacterBase : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
protected:
    void ConnectAbilitySystem(UAbilitySystemComponent* NewAbilitySystem);
    void DisconnectAbilitySystem();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UPROPERTY(Transient, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystem;
private:
    FDelegateHandle DeadTagHandle;
    void OnDeadTagChanged(const FGameplayTag Tag, int32 Count);
};
