#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "ShadeBushVolume.generated.h"
class UBoxComponent;
class UAbilitySystemComponent;
class AShadePlayerCharacter;

// Each bush owns only its own effect handle. Overlapping bushes remain independent.
UCLASS()
class SHADE_API AShadeBushVolume : public AActor
{
    GENERATED_BODY()
public:
    AShadeBushVolume();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UBoxComponent> Bounds;
private:
    UFUNCTION() void Enter(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent,
        int32 BodyIndex, bool bSweep, const FHitResult& Hit);
    UFUNCTION() void Exit(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex);
    void RemoveBushEffect();
    // Standalone: one player per bush. Each bush retains only its own GE grant.
    TWeakObjectPtr<AShadePlayerCharacter> Occupant;
    TWeakObjectPtr<UAbilitySystemComponent> OccupantASC;
    FActiveGameplayEffectHandle BushEffectHandle;
};
