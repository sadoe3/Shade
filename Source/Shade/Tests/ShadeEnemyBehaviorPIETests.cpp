#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "../Character/ShadeEnemyCharacter.h"
#include "../Character/ShadePlayerCharacter.h"
#include "../AI/ShadeAIController.h"
#include "../AI/ShadeAwarenessComponent.h"
#include "../AbilitySystem/ShadeAttributeSet.h"
#include "../AbilitySystem/ShadeAwarenessSet.h"
#include "../AbilitySystem/ShadeEnemyTags.h"
#include "../AbilitySystem/ShadeStealthTags.h"
#include "../AbilitySystem/ShadeGameplayTags.h"
#include "../AbilitySystem/ShadeHealthEffects.h"
#include "../Stealth/ShadeBushVolume.h"

namespace
{
    struct FEnemyPIEScenario
    {
        TWeakObjectPtr<UWorld> World;
        TWeakObjectPtr<AShadeEnemyCharacter> Enemy;
        TWeakObjectPtr<AShadePlayerCharacter> Player;
        TWeakObjectPtr<AShadeAIController> AI;
        TWeakObjectPtr<AShadeBushVolume> Bush;
        FVector Home = FVector::ZeroVector;
        FVector InitialPosition = FVector::ZeroVector;
        FVector Remembered = FVector::ZeroVector;
        FVector DeathPosition = FVector::ZeroVector;
        double WallClockStart = 0.0;
        float PhaseStart = 0.f;
        float SightLostAt = 0.f;
        int32 Phase = 0;
        bool bReachedSearchPoint = false;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeEnemyBehaviorPIETest, "Shade.Enemy.BehaviorPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeEnemyBehaviorPIETest::RunTest(const FString& Parameters)
{
    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!TestTrue(TEXT("Run against the saved stealth prototype map"),
        EditorWorld && EditorWorld->GetName().Contains(TEXT("L_StealthPrototype")))) return false;
    const auto State = MakeShared<FEnemyPIEScenario>();
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (State->WallClockStart == 0.0) State->WallClockStart = FPlatformTime::Seconds();
        if (FPlatformTime::Seconds() - State->WallClockStart > 30.0)
        {
            const auto* Enemy = State->Enemy.Get();
            const auto* AI = State->AI.Get();
            AddError(FString::Printf(TEXT("Enemy PIE scenario timed out in phase %d. Enemy=%s, Home=%s, Alertness=%.2f, Memory=%d"),
                State->Phase, Enemy ? *Enemy->GetActorLocation().ToString() : TEXT("missing"), *State->Home.ToString(),
                Enemy ? Enemy->GetAwarenessSet()->GetAlertness() : -1.f, AI && AI->HasLastKnownLocation()));
            return true;
        }
        if (State->Phase == 0)
        {
            UWorld* World = nullptr;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::PIE && Context.World())
                {
                    World = Context.World();
                    break;
                }
            }
            if (!TestNotNull(TEXT("PIE world"), World)) return true;
            State->World = World;
            auto* PC = World->GetFirstPlayerController();
            auto* Player = PC ? Cast<AShadePlayerCharacter>(PC->GetPawn()) : nullptr;
            if (!TestNotNull(TEXT("Configured playable Character"), Player)) return true;
            State->Player = Player;
            TArray<AShadeEnemyCharacter*> Enemies;
            for (TActorIterator<AShadeEnemyCharacter> It(World); It; ++It) Enemies.Add(*It);
            if (!TestEqual(TEXT("Saved map spawns three configured Enemies"), Enemies.Num(), 3)) return true;
            AShadeEnemyCharacter* Enemy = Enemies[0];
            for (auto* Candidate : Enemies)
                if (Candidate->GetActorLocation().Y < Enemy->GetActorLocation().Y) Enemy = Candidate;
            // Compare the same visible-range placement with/without the saved wall's collision.
            AShadeEnemyCharacter* CenterEnemy = Enemies[0];
            for (auto* Candidate : Enemies)
                if (FMath::Abs(Candidate->GetActorLocation().Y) < FMath::Abs(CenterEnemy->GetActorLocation().Y)) CenterEnemy = Candidate;
            auto* CenterAI = Cast<AShadeAIController>(CenterEnemy->GetController());
            AStaticMeshActor* SightWall = nullptr;
            for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
                if (It->GetActorLabel() == TEXT("Prototype_SightWall")) SightWall = *It;
            if (!TestNotNull(TEXT("Saved wall"), SightWall) || !TestNotNull(TEXT("Wall-facing Enemy AI"), CenterAI)) return true;
            FVector WallTarget = CenterEnemy->GetActorLocation() + CenterEnemy->GetActorForwardVector() * 1400.f;
            WallTarget.Z = Player->GetActorLocation().Z;
            Player->SetActorLocation(WallTarget, false, nullptr, ETeleportType::TeleportPhysics);
            if (!TestFalse(TEXT("Actual saved wall blocks observation"), CenterAI->CanSeeTarget(Player))) return true;
            auto* WallMesh = SightWall->GetStaticMeshComponent();
            const auto SavedCollision = WallMesh->GetCollisionEnabled();
            WallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            const bool bVisibleWithoutWall = CenterAI->CanSeeTarget(Player);
            WallMesh->SetCollisionEnabled(SavedCollision);
            if (!TestTrue(TEXT("Same distance/cone sees Player when only wall collision is removed"), bVisibleWithoutWall)) return true;

            for (auto* Other : Enemies)
            {
                if (Other == Enemy) continue;
                if (Other->GetController()) Other->GetController()->Destroy();
                Other->Destroy(); // Isolate one NPC in this disposable PIE world.
            }
            auto* AI = Cast<AShadeAIController>(Enemy->GetController());
            auto* BT = AI ? Cast<UBehaviorTreeComponent>(AI->GetBrainComponent()) : nullptr;
            auto* BB = AI ? AI->GetBlackboardComponent() : nullptr;
            if (!TestNotNull(TEXT("Blueprint configured the AIController"), AI) ||
                !TestNotNull(TEXT("Blueprint tree created its BrainComponent"), BT) ||
                !TestNotNull(TEXT("Blueprint tree initialized Blackboard"), BB)) return true;
            if (!TestTrue(TEXT("Saved Behavior Tree starts in actual PIE lifecycle"), BT->IsRunning()) ||
                !TestTrue(TEXT("Home key was initialized before behavior"), BB->IsVectorValueSet(TEXT("HomeLocation")))) return true;
            State->Enemy = Enemy;
            State->AI = AI;
            State->Home = AI->GetHomeLocation();
            State->InitialPosition = Enemy->GetActorLocation();
            Player->GetCharacterMovement()->StopMovementImmediately();
            Player->GetCharacterMovement()->DisableMovement();
            FVector Goal = Enemy->GetActorLocation() + Enemy->GetActorForwardVector() * 600.f;
            Goal.Z = Player->GetActorLocation().Z;
            Player->SetActorLocation(Goal, false, nullptr, ETeleportType::TeleportPhysics);
            auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Enemy->GetActorLocation(), Goal);
            if (!TestTrue(TEXT("Saved navigation offers a complete chase path"), Path && Path->IsValid() && !Path->IsPartial()) ||
                !TestTrue(TEXT("Fixture starts with a visible Player"), AI->CanSeeTarget(Player))) return true;
            State->PhaseStart = World->GetTimeSeconds();
            State->Phase = 1;
            return false;
        }
        auto* World = State->World.Get();
        auto* Enemy = State->Enemy.Get();
        auto* Player = State->Player.Get();
        auto* AI = State->AI.Get();
        if (!World || !Enemy || !Player || !AI)
        {
            AddError(TEXT("PIE gameplay objects disappeared before the scenario completed."));
            return true;
        }
        auto* BB = AI->GetBlackboardComponent();
        auto* ASC = Enemy->GetAbilitySystemComponent();
        auto* PlayerASC = Player->GetAbilitySystemComponent();
        const float Now = World->GetTimeSeconds();
        const float Elapsed = Now - State->PhaseStart;
        switch (State->Phase)
        {
        case 1:
            if (Elapsed < 1.6f) return false;
            if (!TestTrue(TEXT("Actual observation raises Alert through periodic GAS"), ASC->HasMatchingGameplayTag(ShadeEnemyTags::State_Awareness_Alert)) ||
                !TestEqual(TEXT("BT has the actual Player target"), BB->GetValueAsObject(TEXT("TargetActor")), static_cast<UObject*>(Player)) ||
                !TestTrue(TEXT("BT navigation actually moves the Enemy"), FVector::Dist2D(Enemy->GetActorLocation(), State->InitialPosition) > 35.f) ||
                !TestTrue(TEXT("Player is visible immediately before Bush is added"), AI->CanSeeTarget(Player))) return true;
            State->Remembered = BB->GetValueAsVector(TEXT("LastKnownLocation"));
            State->Bush = World->SpawnActor<AShadeBushVolume>(Player->GetActorLocation(), FRotator::ZeroRotator);
            if (!TestTrue(TEXT("Runtime Bush spawned"), State->Bush.IsValid())) return true;
            if (auto* Bounds = State->Bush->FindComponentByClass<UBoxComponent>()) Bounds->UpdateOverlaps();
            Player->GetCapsuleComponent()->UpdateOverlaps();
            State->SightLostAt = Now;
            State->PhaseStart = Now;
            State->Phase = 2;
            return false;
        case 2:
            if (Elapsed < 0.35f) return false;
            if (!TestTrue(TEXT("Actual Bush overlap conceals the Player"), PlayerASC->HasMatchingGameplayTag(ShadeStealthTags::State_Stealth_InBush)) ||
                !TestNull(TEXT("Sight loss clears live Actor tracking"), BB->GetValueAsObject(TEXT("TargetActor"))) ||
                !TestEqual(TEXT("Bush does not instantly erase Alertness"), Enemy->GetAwarenessSet()->GetAlertness(), 100.f)) return true;
            Player->SetActorLocation(Player->GetActorLocation() + FVector(0.f, 120.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
            State->PhaseStart = Now;
            State->Phase = 3;
            return false;
        case 3:
            if (Elapsed < 0.35f) return false;
            if (!TestTrue(TEXT("Moved Player remains inside Bush"), PlayerASC->HasMatchingGameplayTag(ShadeStealthTags::State_Stealth_InBush)) ||
                !TestTrue(TEXT("Search point cannot follow hidden movement"), BB->GetValueAsVector(TEXT("LastKnownLocation")).Equals(State->Remembered, 1.f))) return true;
            State->Phase = 4;
            return false;
        case 4:
            State->bReachedSearchPoint |= FVector::Dist2D(Enemy->GetActorLocation(), State->Remembered) < 160.f;
            if (BB->IsVectorValueSet(TEXT("LastKnownLocation")) ||
                FVector::Dist2D(Enemy->GetActorLocation(), State->Home) > 140.f ||
                Enemy->GetVelocity().Size2D() > 10.f || Now - State->SightLostAt < 6.3f) return false;
            if (!TestTrue(TEXT("Enemy actually investigated the remembered location"), State->bReachedSearchPoint) ||
                !TestTrue(TEXT("Search finishes and navigation returns home"), FVector::Dist2D(Enemy->GetActorLocation(), State->Home) <= 140.f) ||
                !TestEqual(TEXT("Sustained concealment decays Alertness to zero"), Enemy->GetAwarenessSet()->GetAlertness(), 0.f) ||
                !TestTrue(TEXT("Enemy returns to Calm"), ASC->HasMatchingGameplayTag(ShadeEnemyTags::State_Awareness_Calm))) return true;
            State->DeathPosition = Enemy->GetActorLocation();
            if (!TestTrue(TEXT("Death effect is applied through Health GE"),
                ShadeHealthEffects::ApplyHealthChange(ASC, -Enemy->GetAttributeSet()->GetHealth()))) return true;
            State->PhaseStart = Now;
            State->Phase = 5;
            return false;
        case 5:
            if (Elapsed < 0.35f) return false;
            TestTrue(TEXT("Enemy has the persistent Dead state"), ASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead));
            TestFalse(TEXT("Dead Enemy stops its Behavior Tree"), AI->GetBrainComponent()->IsRunning());
            TestFalse(TEXT("Dead Enemy stops observation ticks"), AI->IsActorTickEnabled());
            TestFalse(TEXT("Death shuts awareness timers and effects down"), Enemy->GetAwarenessComponent()->IsInitialized());
            TestTrue(TEXT("Dead Enemy remains stationary"), Enemy->GetActorLocation().Equals(State->DeathPosition, 2.f) && Enemy->GetVelocity().IsNearlyZero());
            TestEqual(TEXT("Only the Dead effect remains active"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 1);
            return true;
        }
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([State]()
    {
        if (State->Bush.IsValid()) State->Bush->Destroy();
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
