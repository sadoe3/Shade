#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "ShadeAIController.generated.h"

class AShadeEnemyCharacter;
class UAbilitySystemComponent;
class UBehaviorTree;

UCLASS()
class SHADE_API AShadeAIController : public AAIController
{
    GENERATED_BODY()

public:
    AShadeAIController();
    virtual void Tick(float DeltaSeconds) override;
    bool CanSeeTarget(const APawn* Target) const;
    void ObserveTarget(APawn* Target);
    APawn* GetChaseTarget() const;
    bool HasLastKnownLocation() const { return bHasLastKnownLocation; }
    FVector GetLastKnownLocation() const { return LastKnownLocation; }
    FVector GetHomeLocation() const { return HomeLocation; }
    void CompleteSearch();

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI")
    TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

private:
    void TryStartBehavior();
    void UpdateBlackboard();
    void ReleaseEnemy();
    void OnDeadTagChanged(const FGameplayTag Tag, int32 Count);
    void StopObserving();
    TWeakObjectPtr<AShadeEnemyCharacter> ObservedEnemy;
    TWeakObjectPtr<UAbilitySystemComponent> ObservedAbilitySystem;
    TWeakObjectPtr<APawn> VisibleTarget;
    FDelegateHandle DeadHandle;
    FVector HomeLocation = FVector::ZeroVector;
    FVector LastKnownLocation = FVector::ZeroVector;
    FVector LastVisibleLocation = FVector::ZeroVector;
    bool bHasLastKnownLocation = false;
    bool bBehaviorStartAttempted = false;
};
