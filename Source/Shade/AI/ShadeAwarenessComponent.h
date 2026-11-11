#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "ShadeAwarenessComponent.generated.h"

class UAbilitySystemComponent;

struct FOnAttributeChangeData;

UENUM(BlueprintType)
enum class EShadeAwarenessState : uint8
{
    Calm,
    Suspicious,
    Alert
};

// Authority-side observer: turns awareness attributes into one owned state GE.
UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent))
class SHADE_API UShadeAwarenessComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    bool Initialize(UAbilitySystemComponent* InAbilitySystem);

    void Shutdown();
    EShadeAwarenessState GetAwarenessState() const { return CurrentState; }
    bool IsInitialized() const { return AbilitySystem.IsValid(); }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void UpdateState();
    void OnAttributeChanged(const FOnAttributeChangeData& Change);
    void OnDeadTagChanged(const FGameplayTag Tag, int32 Count);

    TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;

    FDelegateHandle AlertnessHandle;
    FDelegateHandle MaximumHandle;
    FDelegateHandle DeadHandle;

    FActiveGameplayEffectHandle StateEffect;

    EShadeAwarenessState CurrentState = EShadeAwarenessState::Calm;

};
