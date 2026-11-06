#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "ShadeAwarenessComponent.generated.h"

class UAbilitySystemComponent;
class UShadeEnemyBehaviorData;
struct FOnAttributeChangeData;

UENUM(BlueprintType)
enum class EShadeAwarenessState : uint8
{
    Calm,
    Suspicious,
    Alert
};

// Translates observation into GAS effects; AI behavior consumes the resulting state.
UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent))
class SHADE_API UShadeAwarenessComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    bool Initialize(UAbilitySystemComponent* InAbilitySystem, UShadeEnemyBehaviorData* InSettings);
    void SetTargetVisible(bool bVisible);
    void Shutdown();
    EShadeAwarenessState GetAwarenessState() const { return CurrentState; }
    bool IsInitialized() const { return AbilitySystem.IsValid(); }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void StartTrend(float ChangePerSecond);
    void StopTrend();
    void StartDecay();
    void UpdateState();
    void OnAttributeChanged(const FOnAttributeChangeData& Change);
    void OnDeadTagChanged(const FGameplayTag Tag, int32 Count);

    TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
    UPROPERTY(Transient)
    TObjectPtr<UShadeEnemyBehaviorData> Settings;
    FDelegateHandle AlertnessHandle;
    FDelegateHandle MaximumHandle;
    FDelegateHandle DeadHandle;
    FActiveGameplayEffectHandle TrendEffect;
    FActiveGameplayEffectHandle StateEffect;
    FTimerHandle DecayTimer;
    EShadeAwarenessState CurrentState = EShadeAwarenessState::Calm;
    bool bTargetVisible = false;
};
