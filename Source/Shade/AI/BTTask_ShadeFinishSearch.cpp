#include "BTTask_ShadeFinishSearch.h"

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "ShadeAIController.h"

UBTTask_ShadeFinishSearch::UBTTask_ShadeFinishSearch()
{
    NodeName = TEXT("Finish search");
}

EBTNodeResult::Type UBTTask_ShadeFinishSearch::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    auto* Controller = Cast<AShadeAIController>(OwnerComp.GetAIOwner());
    if (!Controller) return EBTNodeResult::Failed;
    Controller->CompleteSearch();
    return EBTNodeResult::Succeeded;
}
