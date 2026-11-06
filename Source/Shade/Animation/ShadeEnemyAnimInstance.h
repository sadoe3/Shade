#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ShadeEnemyAnimInstance.generated.h"

UCLASS()
class SHADE_API UShadeEnemyAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Movement")
    float GroundSpeed = 0.f;

protected:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
