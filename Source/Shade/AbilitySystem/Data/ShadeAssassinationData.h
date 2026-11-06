#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShadeAssassinationData.generated.h"
UCLASS(BlueprintType)
class SHADE_API UShadeAssassinationData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assassination", meta=(ClampMin="1.0", Units="cm"))
    float Range = 220.f;
};
