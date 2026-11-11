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
    virtual void Restart() override;
    UFUNCTION(BlueprintPure, Category="GAS")
    bool IsDead() const;

protected:
    void ConnectAbilitySystem(UAbilitySystemComponent* NewAbilitySystem);
    void DisconnectAbilitySystem();
    // Derived Characters can stop AI or presentation after the shared death response.
    virtual void HandleDeath();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // References the GAS owner; the base Character does not create an ASC.
    UPROPERTY(Transient, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystem;

private:
    void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
    FDelegateHandle DeadTagHandle;
};
