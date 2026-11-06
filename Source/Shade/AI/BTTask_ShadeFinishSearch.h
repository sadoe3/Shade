#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ShadeFinishSearch.generated.h"

UCLASS()
class SHADE_API UBTTask_ShadeFinishSearch : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_ShadeFinishSearch();
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
