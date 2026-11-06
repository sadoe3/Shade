#include "ShadeAIController.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "BrainComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "../AbilitySystem/Data/ShadeInitialAttributesData.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "ShadeAwarenessComponent.h"
#include "../Character/ShadeEnemyCharacter.h"
#include "../AbilitySystem/Data/ShadeEnemyBehaviorData.h"
#include "../AbilitySystem/ShadeGameplayTags.h"
#include "../AbilitySystem/ShadeStealthTags.h"

AShadeAIController::AShadeAIController()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void AShadeAIController::OnPossess(APawn* InPawn)
{
    ReleaseEnemy();
    Super::OnPossess(InPawn);
    auto* Enemy = Cast<AShadeEnemyCharacter>(InPawn);
    const auto* Data = Enemy ? Enemy->GetBehaviorData() : nullptr;
    if (!Data || !FMath::IsFinite(Data->SightRadius) || Data->SightRadius < 1.f ||
        !FMath::IsFinite(Data->SightHalfAngleDegrees) || Data->SightHalfAngleDegrees <= 0.f || Data->SightHalfAngleDegrees >= 180.f ||
        !FMath::IsFinite(Data->SightUpdateInterval) || Data->SightUpdateInterval <= 0.f)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: Enemy AI requires valid sight settings."), *GetName());
        return;
    }
    ObservedEnemy = Enemy;
    ObservedAbilitySystem = Enemy->GetAbilitySystemComponent();
    HomeLocation = Enemy->GetActorLocation();
    LastKnownLocation = HomeLocation;
    bHasLastKnownLocation = false;
    if (auto* ASC = ObservedAbilitySystem.Get())
    {
        DeadHandle = ASC->RegisterGameplayTagEvent(ShadeGameplayTags::State_Dead)
            .AddUObject(this, &ThisClass::OnDeadTagChanged);
    }
    SetActorTickInterval(Data->SightUpdateInterval);
    SetActorTickEnabled(!Enemy->IsDead());
    TryStartBehavior();
}

void AShadeAIController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    TryStartBehavior();
    // Target selection is intentionally the single local Player for this prototype.
    ObserveTarget(UGameplayStatics::GetPlayerPawn(this, 0));
}

bool AShadeAIController::CanSeeTarget(const APawn* Target) const
{
    const auto* Enemy = ObservedEnemy.Get();
    const auto* Data = Enemy ? Enemy->GetBehaviorData() : nullptr;
    if (!IsValid(Target) || !Enemy || !Data || Target == Enemy || Enemy->IsDead()) return false;
    const auto* TargetInterface = Cast<IAbilitySystemInterface>(Target);
    const auto* TargetASC = TargetInterface ? TargetInterface->GetAbilitySystemComponent() : nullptr;
    if (TargetASC && (TargetASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead) ||
        TargetASC->HasMatchingGameplayTag(ShadeStealthTags::State_Stealth_InBush))) return false;

    const FVector Origin = Enemy->GetPawnViewLocation();
    const FVector TargetLocation = Target->GetPawnViewLocation();
    const FVector ToTarget = TargetLocation - Origin;
    if (ToTarget.SizeSquared() > FMath::Square(Data->SightRadius)) return false;
    if (!ToTarget.IsNearlyZero() && FVector::DotProduct(Enemy->GetActorForwardVector(), ToTarget.GetSafeNormal()) <
        FMath::Cos(FMath::DegreesToRadians(Data->SightHalfAngleDegrees))) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ShadeEnemySight), false, Enemy);
    Query.AddIgnoredActor(Target);
    return !GetWorld()->LineTraceTestByChannel(Origin, TargetLocation, ECC_Visibility, Query);
}

void AShadeAIController::ObserveTarget(APawn* Target)
{
    auto* Enemy = ObservedEnemy.Get();
    auto* Awareness = Enemy ? Enemy->GetAwarenessComponent() : nullptr;
    if (!Enemy || Enemy->IsDead() || !Awareness || !Awareness->IsInitialized()) return;
    // A periodic GE can reach Alert between sight samples. Promote the previous visible
    // snapshot before clearing its Actor reference; the hidden Actor may have already moved.
    if (GetChaseTarget())
    {
        LastKnownLocation = LastVisibleLocation;
        bHasLastKnownLocation = true;
    }
    const bool bVisible = CanSeeTarget(Target);
    Awareness->SetTargetVisible(bVisible);
    VisibleTarget = bVisible ? Target : nullptr;
    if (bVisible)
    {
        LastVisibleLocation = Target->GetActorLocation();
        if (GetChaseTarget())
        {
            LastKnownLocation = LastVisibleLocation;
            bHasLastKnownLocation = true;
        }
    }
    UpdateBlackboard();
}

APawn* AShadeAIController::GetChaseTarget() const
{
    const auto* Enemy = ObservedEnemy.Get();
    const auto* Awareness = Enemy ? Enemy->GetAwarenessComponent() : nullptr;
    return Enemy && !Enemy->IsDead() && Awareness && Awareness->IsInitialized() &&
        Awareness->GetAwarenessState() == EShadeAwarenessState::Alert ? VisibleTarget.Get() : nullptr;
}

void AShadeAIController::CompleteSearch()
{
    if (GetChaseTarget()) return;
    bHasLastKnownLocation = false;
    UpdateBlackboard();
}

void AShadeAIController::OnDeadTagChanged(const FGameplayTag Tag, int32 Count)
{
    if (Count > 0) StopObserving();
}

void AShadeAIController::StopObserving()
{
    SetActorTickEnabled(false);
    VisibleTarget.Reset();
    bHasLastKnownLocation = false;
    UpdateBlackboard();
    StopMovement();
    if (GetBrainComponent()) GetBrainComponent()->StopLogic(TEXT("Enemy observation ended"));
    if (auto* Enemy = ObservedEnemy.Get()) Enemy->GetAwarenessComponent()->SetTargetVisible(false);
}

void AShadeAIController::ReleaseEnemy()
{
    StopObserving();
    if (auto* ASC = ObservedAbilitySystem.Get())
    {
        ASC->RegisterGameplayTagEvent(ShadeGameplayTags::State_Dead).Remove(DeadHandle);
    }
    DeadHandle.Reset();
    ObservedAbilitySystem.Reset();
    ObservedEnemy.Reset();
    bBehaviorStartAttempted = false;
}

void AShadeAIController::OnUnPossess()
{
    ReleaseEnemy();
    Super::OnUnPossess();
}

void AShadeAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ReleaseEnemy();
    Super::EndPlay(EndPlayReason);
}

void AShadeAIController::TryStartBehavior()
{
    auto* Enemy = ObservedEnemy.Get();
    // OnPossess can precede the Enemy's BeginPlay. Tick retries only until GAS is ready.
    if (bBehaviorStartAttempted || !BehaviorTreeAsset || !Enemy || Enemy->IsDead() ||
        !Enemy->GetAwarenessComponent()->IsInitialized()) return;
    bBehaviorStartAttempted = true;
    const auto* Data = Enemy->GetBehaviorData();
    if (!BehaviorTreeAsset->BlackboardAsset || !BehaviorTreeAsset->RootNode ||
        !FMath::IsFinite(Data->SearchDuration) || Data->SearchDuration < 0.f)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: Enemy behavior requires a tree root, Blackboard, and valid search duration."), *GetName());
        return;
    }
    UBlackboardComponent* BlackboardComponent = nullptr;
    if (!UseBlackboard(BehaviorTreeAsset->BlackboardAsset, BlackboardComponent) || !BlackboardComponent) return;
    const bool bValidKeys =
        BlackboardComponent->GetKeyType(BlackboardComponent->GetKeyID(TEXT("TargetActor"))) == UBlackboardKeyType_Object::StaticClass() &&
        BlackboardComponent->GetKeyType(BlackboardComponent->GetKeyID(TEXT("LastKnownLocation"))) == UBlackboardKeyType_Vector::StaticClass() &&
        BlackboardComponent->GetKeyType(BlackboardComponent->GetKeyID(TEXT("HomeLocation"))) == UBlackboardKeyType_Vector::StaticClass() &&
        BlackboardComponent->GetKeyType(BlackboardComponent->GetKeyID(TEXT("SearchDuration"))) == UBlackboardKeyType_Float::StaticClass();
    if (!bValidKeys)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: Enemy Blackboard must define TargetActor, LastKnownLocation, HomeLocation, and SearchDuration."), *GetName());
        return;
    }
    BlackboardComponent->SetValueAsVector(TEXT("HomeLocation"), HomeLocation);
    BlackboardComponent->SetValueAsFloat(TEXT("SearchDuration"), Data->SearchDuration);
    UpdateBlackboard();
    if (!RunBehaviorTree(BehaviorTreeAsset))
    {
        UE_LOG(LogTemp, Error, TEXT("%s: Enemy behavior tree could not start."), *GetName());
    }
}

void AShadeAIController::UpdateBlackboard()
{
    if (auto* BlackboardComponent = GetBlackboardComponent())
    {
        BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), GetChaseTarget());
        if (bHasLastKnownLocation) BlackboardComponent->SetValueAsVector(TEXT("LastKnownLocation"), LastKnownLocation);
        else BlackboardComponent->ClearValue(TEXT("LastKnownLocation"));
    }
    if (auto* Enemy = ObservedEnemy.Get(); Enemy && !Enemy->IsDead())
    {
        if (const auto* MovementData = Enemy->GetInitialAttributesData())
        {
            Enemy->GetCharacterMovement()->MaxWalkSpeed = GetChaseTarget() ? MovementData->RunSpeed : MovementData->WalkSpeed;
        }
    }
}
