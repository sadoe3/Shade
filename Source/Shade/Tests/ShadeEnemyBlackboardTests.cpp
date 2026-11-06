#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "../AbilitySystem/Data/ShadeInitialAttributesData.h"
#include "../AbilitySystem/Data/ShadeEnemyBehaviorData.h"
#include "../AbilitySystem/ShadeAwarenessSet.h"
#include "../AbilitySystem/ShadeHealthEffects.h"
#include "../AI/ShadeAIController.h"
#include "../AI/BTTask_ShadeFinishSearch.h"
#include "../Character/ShadeEnemyCharacter.h"
#include "../Stealth/ShadeStealthEffects.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeEnemyBlackboardTest, "Shade.Enemy.BlackboardConnection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeEnemyBlackboardTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine context owner"), GEngine)) return false;
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(true)
        .ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ShadeEnemyBlackboardTestWorld"),
        nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Isolated behavior world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    World->InitializeActorsForPlay(FURL());
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy = World->SpawnActor<AShadeEnemyCharacter>(Spawn);
    auto* Target = World->SpawnActor<AShadeEnemyCharacter>(Spawn);
    auto* AI = World->SpawnActor<AShadeAIController>();
    if (!Enemy || !Target || !AI) return false;
    auto* HealthProperty = FindFProperty<FObjectPropertyBase>(AShadeEnemyCharacter::StaticClass(), TEXT("InitialAttributesData"));
    auto* BehaviorProperty = FindFProperty<FObjectPropertyBase>(AShadeEnemyCharacter::StaticClass(), TEXT("BehaviorData"));
    auto* TreeProperty = FindFProperty<FObjectPropertyBase>(AShadeAIController::StaticClass(), TEXT("BehaviorTreeAsset"));
    if (!HealthProperty || !BehaviorProperty || !TreeProperty) return false;
    auto* Health = NewObject<UShadeInitialAttributesData>(World);
    Health->InitialHealth = 100.f;
    Health->WalkSpeed = 180.f;
    Health->RunSpeed = 300.f;
    Health->BrakingDecelerationWalking = 1800.f;
    auto* Behavior = NewObject<UShadeEnemyBehaviorData>(World);
    Behavior->SearchDuration = 2.75f;
    for (auto* Body : { Enemy, Target })
    {
        HealthProperty->SetObjectPropertyValue_InContainer(Body, Health);
        BehaviorProperty->SetObjectPropertyValue_InContainer(Body, Behavior);
    }
    Enemy->SetActorLocation(FVector(100.f, 200.f, 0.f));
    Target->SetActorLocation(FVector(600.f, 200.f, 0.f));
    Target->DispatchBeginPlay();
    auto* Tree = NewObject<UBehaviorTree>(World);
    auto* Data = NewObject<UBlackboardData>(Tree);
    Data->UpdatePersistentKey<UBlackboardKeyType_Object>(TEXT("TargetActor"))->BaseClass = AActor::StaticClass();
    Data->UpdatePersistentKey<UBlackboardKeyType_Vector>(TEXT("LastKnownLocation"));
    Data->UpdatePersistentKey<UBlackboardKeyType_Vector>(TEXT("HomeLocation"));
    Data->UpdatePersistentKey<UBlackboardKeyType_Float>(TEXT("SearchDuration"));
    Tree->BlackboardAsset = Data;
    auto* Root = NewObject<UBTComposite_Selector>(Tree);
    auto* Wait = NewObject<UBTTask_Wait>(Root);
    Wait->WaitTime = FValueOrBBKey_Float(60.f);
    FBTCompositeChild Child;
    Child.ChildTask = Wait;
    Root->Children.Add(Child);
    Tree->RootNode = Root;
    TreeProperty->SetObjectPropertyValue_InContainer(AI, Tree);
    AI->DispatchBeginPlay();
    AI->Possess(Enemy); // Real spawn ordering can possess before the body's BeginPlay.
    TestNull(TEXT("Behavior waits while Enemy GAS is not initialized"), AI->GetBrainComponent());
    Enemy->DispatchBeginPlay();
    AI->Tick(0.1f);
    auto* BB = AI->GetBlackboardComponent();
    auto* BT = Cast<UBehaviorTreeComponent>(AI->GetBrainComponent());
    if (!TestNotNull(TEXT("Blackboard initialized after Enemy readiness"), BB) ||
        !TestNotNull(TEXT("BehaviorTree component created"), BT)) return false;
    TestTrue(TEXT("Configured tree is running"), BT->IsRunning());
    TestEqual(TEXT("Running tree matches configured asset"), BT->GetRootTree(), Tree);
    TestEqual(TEXT("Home position captured before behavior starts"), BB->GetValueAsVector(TEXT("HomeLocation")), FVector(100.f, 200.f, 0.f));
    TestEqual(TEXT("Search duration is read from data"), BB->GetValueAsFloat(TEXT("SearchDuration")), 2.75f);
    auto* ASC = Enemy->GetAbilitySystemComponent();
    ASC->SetNumericAttributeBase(UShadeAwarenessSet::GetAlertnessAttribute(), 100.f);
    AI->ObserveTarget(Target);
    TestEqual(TEXT("BB TargetActor projects Alert visible target"), BB->GetValueAsObject(TEXT("TargetActor")), static_cast<UObject*>(Target));
    TestEqual(TEXT("Chase uses authored RunSpeed"), Enemy->GetCharacterMovement()->MaxWalkSpeed, 300.f);
    const FVector Remembered = BB->GetValueAsVector(TEXT("LastKnownLocation"));
    auto* TargetASC = Target->GetAbilitySystemComponent();
    auto BushSpec = TargetASC->MakeOutgoingSpec(UShadeInBushEffect::StaticClass(), 1.f, TargetASC->MakeEffectContext());
    const auto BushHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*BushSpec.Data.Get());
    AI->ObserveTarget(Target);
    TestNull(TEXT("Concealment clears Actor tracking in Blackboard"), BB->GetValueAsObject(TEXT("TargetActor")));
    Target->SetActorLocation(FVector(900.f, 200.f, 0.f));
    AI->ObserveTarget(Target);
    TestEqual(TEXT("BB search point stays at last observation"), BB->GetValueAsVector(TEXT("LastKnownLocation")), Remembered);
    TestEqual(TEXT("Search uses authored WalkSpeed"), Enemy->GetCharacterMovement()->MaxWalkSpeed, 180.f);
    auto* FinishTask = NewObject<UBTTask_ShadeFinishSearch>(Tree);
    TestTrue(TEXT("Finish task succeeds"), FinishTask->ExecuteTask(*BT, nullptr) == EBTNodeResult::Succeeded);
    TestFalse(TEXT("Finish task clears the search key"), BB->IsVectorValueSet(TEXT("LastKnownLocation")));
    TargetASC->RemoveActiveGameplayEffect(BushHandle);
    AI->ObserveTarget(Target);
    FinishTask->ExecuteTask(*BT, nullptr);
    TestTrue(TEXT("Finishing an old search preserves a reacquired target"), BB->IsVectorValueSet(TEXT("LastKnownLocation")));
    ShadeHealthEffects::ApplyHealthChange(ASC, -100.f);
    TestFalse(TEXT("Dead Enemy stops its behavior tree"), BT->IsRunning());
    TestNull(TEXT("Dead Enemy clears target key"), BB->GetValueAsObject(TEXT("TargetActor")));
    TestFalse(TEXT("Dead Enemy clears search key"), BB->IsVectorValueSet(TEXT("LastKnownLocation")));
    AI->Destroy();
    Enemy->Destroy();
    Target->Destroy();
    return true;
}

#endif
