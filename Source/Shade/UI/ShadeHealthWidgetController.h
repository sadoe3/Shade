#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ShadeHealthWidgetController.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

DECLARE_MULTICAST_DELEGATE_TwoParams(FShadeHealthValuesChanged, float /*Health*/, float /*MaxHealth*/);

// UI data bridge, not a Pawn controller. The UI owner must keep this object alive.
UCLASS()
class SHADE_API UShadeHealthWidgetController : public UObject
{
    GENERATED_BODY()

public:
    // Subscribes without broadcasting. Connect the view, then request current values.
    bool BindAbilitySystem(UAbilitySystemComponent* InAbilitySystem);
    void BroadcastCurrentValues();
    void UnbindAbilitySystem();

    // Native UI consumers receive current GAS values, without owning gameplay state.
    FShadeHealthValuesChanged OnHealthValuesChanged;

protected:
    virtual void BeginDestroy() override;

private:
    void HandleAttributeChanged(const FOnAttributeChangeData& Data);

    TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
    FDelegateHandle HealthChangedHandle;
    FDelegateHandle MaxHealthChangedHandle;
};
