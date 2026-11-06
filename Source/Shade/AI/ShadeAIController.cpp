#include "ShadeAIController.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "BrainComponent.h"
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
}

void AShadeAIController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
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
    const bool bVisible = CanSeeTarget(Target);
    Awareness->SetTargetVisible(bVisible);
    VisibleTarget = bVisible ? Target : nullptr;
    if (GetChaseTarget())
    {
        // Freeze this position when observation is lost; never track a hidden Actor's live transform.
        LastKnownLocation = Target->GetActorLocation();
        bHasLastKnownLocation = true;
    }
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
}

void AShadeAIController::OnDeadTagChanged(const FGameplayTag Tag, int32 Count)
{
    if (Count > 0) StopObserving();
}

void AShadeAIController::StopObserving()
{
    SetActorTickEnabled(false);
    VisibleTarget.Reset();
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
    bHasLastKnownLocation = false;
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
