#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "../AbilitySystem/Data/ShadeInitialAttributesData.h"
#include "../AbilitySystem/Data/ShadeEnemyBehaviorData.h"
#include "../AbilitySystem/ShadeAwarenessSet.h"
#include "../AI/ShadeAIController.h"
#include "../AI/ShadeAwarenessComponent.h"
#include "../Character/ShadeEnemyCharacter.h"
#include "../Stealth/ShadeStealthEffects.h"

namespace
{
struct FSightPromotionWorld
{
    UWorld* World = nullptr;
    AShadeEnemyCharacter* Enemy = nullptr;
    AShadeEnemyCharacter* Target = nullptr;
    AShadeAIController* AI = nullptr;
    UBlackboardComponent* Blackboard = nullptr;
    FVector LastSeen = FVector(600.f, 0.f, 0.f);
    bool bSampledSuspicious = false;
    float Elapsed = 0.f;
    uint64 LastFrame = MAX_uint64;
    ~FSightPromotionWorld()
    {
        if (World)
        {
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeSightPromotionTest, "Shade.Enemy.SightPromotionMemory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeSightPromotionTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine"), GEngine)) return false;
    auto State = MakeShared<FSightPromotionWorld>();
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(true)
        .ShouldSimulatePhysics(false).SetTransactional(false);
    State->World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SightPromotionWorld"),
        nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("World"), State->World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(State->World);
    State->World->InitializeActorsForPlay(FURL());
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    State->Enemy = State->World->SpawnActor<AShadeEnemyCharacter>(Spawn);
    State->Target = State->World->SpawnActor<AShadeEnemyCharacter>(Spawn);
    State->AI = State->World->SpawnActor<AShadeAIController>();
    if (!TestNotNull(TEXT("Enemy"), State->Enemy) || !TestNotNull(TEXT("Target"), State->Target) ||
        !TestNotNull(TEXT("AI"), State->AI)) return false;
    auto* HealthProperty = FindFProperty<FObjectPropertyBase>(AShadeEnemyCharacter::StaticClass(), TEXT("InitialAttributesData"));
    auto* BehaviorProperty = FindFProperty<FObjectPropertyBase>(AShadeEnemyCharacter::StaticClass(), TEXT("BehaviorData"));
    if (!TestNotNull(TEXT("Health settings property"), HealthProperty) ||
        !TestNotNull(TEXT("Behavior settings property"), BehaviorProperty)) return false;
    auto* Health = NewObject<UShadeInitialAttributesData>(State->World);
    Health->InitialHealth = 100.f;
    Health->WalkSpeed = 180.f;
    Health->RunSpeed = 300.f;
    Health->BrakingDecelerationWalking = 1800.f;
    auto* Behavior = NewObject<UShadeEnemyBehaviorData>(State->World);
    for (auto* Body : {State->Enemy, State->Target})
    {
        HealthProperty->SetObjectPropertyValue_InContainer(Body, Health);
        BehaviorProperty->SetObjectPropertyValue_InContainer(Body, Behavior);
        Body->DispatchBeginPlay();
        Body->GetCharacterMovement()->GravityScale = 0.f;
    }
    State->AI->DispatchBeginPlay();
    State->AI->Possess(State->Enemy);
    // Sample sight explicitly, while the ASC's real periodic GE keeps advancing between samples.
    State->AI->SetActorTickEnabled(false);
    State->Target->SetActorLocation(FVector(500.f, 0.f, 0.f));
    auto* BlackboardData = NewObject<UBlackboardData>(State->AI);
    BlackboardData->UpdatePersistentKey<UBlackboardKeyType_Object>(TEXT("TargetActor"))->BaseClass = AActor::StaticClass();
    BlackboardData->UpdatePersistentKey<UBlackboardKeyType_Vector>(TEXT("LastKnownLocation"));
    if (!TestTrue(TEXT("Blackboard initialized"), State->AI->UseBlackboard(BlackboardData, State->Blackboard))) return false;
    State->AI->ObserveTarget(State->Target);
    TestFalse(TEXT("Initial Calm observation is not a search"), State->AI->HasLastKnownLocation());

    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (State->LastFrame == GFrameCounter) return false;
        State->LastFrame = GFrameCounter;
        State->World->Tick(LEVELTICK_All, 0.05f);
        State->Elapsed += 0.05f;
        const float Alertness = State->Enemy->GetAwarenessSet()->GetAlertness();
        if (!State->bSampledSuspicious && Alertness >= 90.f && Alertness < 100.f)
        {
            State->Target->SetActorLocation(State->LastSeen);
            State->AI->ObserveTarget(State->Target);
            TestFalse(TEXT("The last Suspicious sample has no Alert search memory"), State->AI->HasLastKnownLocation());
            State->bSampledSuspicious = true;
            return false;
        }
        if (Alertness < 100.f)
        {
            if (State->Elapsed < 3.f) return false;
            AddError(TEXT("Real periodic gain did not reach Alert within the test deadline."));
            return true;
        }
        if (!TestTrue(TEXT("A Suspicious observation preceded periodic Alert promotion"), State->bSampledSuspicious)) return true;
        TestTrue(TEXT("Periodic GE promoted the awareness state"),
            State->Enemy->GetAwarenessComponent()->GetAwarenessState() == EShadeAwarenessState::Alert);
        auto* TargetASC = State->Target->GetAbilitySystemComponent();
        const auto BushSpec = TargetASC->MakeOutgoingSpec(UShadeInBushEffect::StaticClass(), 1.f, TargetASC->MakeEffectContext());
        if (!TestTrue(TEXT("Concealment spec"), BushSpec.IsValid())) return true;
        TargetASC->ApplyGameplayEffectSpecToSelf(*BushSpec.Data.Get());
        State->Target->SetActorLocation(FVector(900.f, 300.f, 0.f));
        TestFalse(TEXT("Target hides before the next sight sample"), State->AI->CanSeeTarget(State->Target));
        State->AI->ObserveTarget(State->Target);
        TestNull(TEXT("Hidden target is not chased by Actor"), State->AI->GetChaseTarget());
        TestTrue(TEXT("Alert promotion retains the last visible position"), State->AI->HasLastKnownLocation());
        TestEqual(TEXT("Memory uses the Suspicious snapshot, not the hidden live location"),
            State->AI->GetLastKnownLocation(), State->LastSeen);
        TestNull(TEXT("Blackboard releases hidden Actor tracking"), State->Blackboard->GetValueAsObject(TEXT("TargetActor")));
        TestEqual(TEXT("Blackboard receives the promoted snapshot"),
            State->Blackboard->GetValueAsVector(TEXT("LastKnownLocation")), State->LastSeen);
        State->Target->SetActorLocation(FVector(1100.f, 400.f, 0.f));
        State->AI->ObserveTarget(State->Target);
        TestEqual(TEXT("Further hidden movement cannot refresh memory"), State->AI->GetLastKnownLocation(), State->LastSeen);
        State->AI->CompleteSearch();
        State->AI->ObserveTarget(State->Target);
        TestFalse(TEXT("A completed search is not revived by an old cached snapshot"), State->AI->HasLastKnownLocation());
        TestFalse(TEXT("Completed search clears the Blackboard point"), State->Blackboard->IsVectorValueSet(TEXT("LastKnownLocation")));
        return true;
    }));
    return true;
}
#endif
