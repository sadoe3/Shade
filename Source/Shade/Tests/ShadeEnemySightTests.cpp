#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Components/BoxComponent.h"
#include "AbilitySystemComponent.h"
#include "../AbilitySystem/Data/ShadeInitialAttributesData.h"
#include "../AbilitySystem/Data/ShadeEnemyBehaviorData.h"
#include "../AbilitySystem/ShadeAwarenessSet.h"
#include "../AbilitySystem/ShadeHealthEffects.h"
#include "../AI/ShadeAIController.h"
#include "../AI/ShadeAwarenessComponent.h"
#include "../Character/ShadeEnemyCharacter.h"
#include "../Stealth/ShadeStealthEffects.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeEnemySightTest, "Shade.Enemy.SightAndMemory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeEnemySightTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine context owner"), GEngine)) return false;
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(true)
        .ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ShadeEnemySightTestWorld"),
        nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Isolated sight world"), World)) return false;
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
    if (!HealthProperty || !BehaviorProperty) return false;
    auto* Health = NewObject<UShadeInitialAttributesData>(World);
    Health->InitialHealth = 100.f;
    Health->WalkSpeed = 180.f;
    Health->RunSpeed = 300.f;
    Health->BrakingDecelerationWalking = 1800.f;
    auto* Behavior = NewObject<UShadeEnemyBehaviorData>(World);
    for (auto* Body : { Enemy, Target })
    {
        HealthProperty->SetObjectPropertyValue_InContainer(Body, Health);
        BehaviorProperty->SetObjectPropertyValue_InContainer(Body, Behavior);
        Body->DispatchBeginPlay();
    }
    AI->DispatchBeginPlay();
    AI->Possess(Enemy);
    Enemy->SetActorLocation(FVector::ZeroVector);
    Enemy->SetActorRotation(FRotator::ZeroRotator);
    Target->SetActorLocation(FVector(500.f, 0.f, 0.f));
    TestTrue(TEXT("Unobstructed target in the cone is visible"), AI->CanSeeTarget(Target));
    Target->SetActorLocation(FVector(2000.f, 0.f, 0.f));
    TestFalse(TEXT("Target outside sight radius is hidden"), AI->CanSeeTarget(Target));
    Target->SetActorLocation(FVector(0.f, 500.f, 0.f));
    TestFalse(TEXT("Target outside cone is hidden"), AI->CanSeeTarget(Target));
    Target->SetActorLocation(FVector(-500.f, 0.f, 0.f));
    TestFalse(TEXT("Target behind the Enemy is hidden"), AI->CanSeeTarget(Target));
    Target->SetActorLocation(FVector(500.f, 0.f, 0.f));
    auto* Wall = World->SpawnActor<AActor>();
    auto* Bounds = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Bounds);
    Bounds->SetBoxExtent(FVector(50.f, 200.f, 200.f));
    Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    Bounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Bounds->RegisterComponent();
    Wall->SetActorLocation(FVector(250.f, 0.f, 80.f));
    TestFalse(TEXT("Visibility-blocking wall occludes the target"), AI->CanSeeTarget(Target));
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TestTrue(TEXT("Removing occlusion restores visibility"), AI->CanSeeTarget(Target));
    AI->ObserveTarget(Target);
    TestNull(TEXT("Visible Calm target is not chased"), AI->GetChaseTarget());
    TestFalse(TEXT("Calm observation does not start an Alert search"), AI->HasLastKnownLocation());
    auto* ASC = Enemy->GetAbilitySystemComponent();
    ASC->SetNumericAttributeBase(UShadeAwarenessSet::GetAlertnessAttribute(), 100.f);
    AI->ObserveTarget(Target);
    TestEqual(TEXT("Alert visible target becomes chase target"), AI->GetChaseTarget(), static_cast<APawn*>(Target));
    TestTrue(TEXT("Alert observation stores a search location"), AI->HasLastKnownLocation());
    const FVector Remembered = AI->GetLastKnownLocation();
    auto* TargetASC = Target->GetAbilitySystemComponent();
    auto BushSpec = TargetASC->MakeOutgoingSpec(UShadeInBushEffect::StaticClass(), 1.f, TargetASC->MakeEffectContext());
    if (!TestTrue(TEXT("Bush effect spec"), BushSpec.IsValid())) return false;
    const auto BushHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*BushSpec.Data.Get());
    TestFalse(TEXT("Bush breaks observation even after Alert"), AI->CanSeeTarget(Target));
    AI->ObserveTarget(Target);
    TestNull(TEXT("Hidden target is no longer tracked by Actor"), AI->GetChaseTarget());
    TestEqual(TEXT("Bush does not reset Alertness immediately"), Enemy->GetAwarenessSet()->GetAlertness(), 100.f);
    Target->SetActorLocation(FVector(900.f, 200.f, 0.f));
    AI->ObserveTarget(Target);
    TestEqual(TEXT("Hidden movement cannot update the remembered position"), AI->GetLastKnownLocation(), Remembered);
    AI->CompleteSearch();
    TestFalse(TEXT("Completed search releases location memory"), AI->HasLastKnownLocation());
    TargetASC->RemoveActiveGameplayEffect(BushHandle);
    AI->ObserveTarget(Target);
    TestEqual(TEXT("Leaving Bush permits reacquisition"), AI->GetChaseTarget(), static_cast<APawn*>(Target));
    AI->CompleteSearch();
    TestTrue(TEXT("Completing an old search cannot erase a reacquired target"), AI->HasLastKnownLocation());
    ShadeHealthEffects::ApplyHealthChange(TargetASC, -100.f);
    TestFalse(TEXT("Dead target is excluded"), AI->CanSeeTarget(Target));
    ShadeHealthEffects::ApplyHealthChange(ASC, -100.f);
    TestFalse(TEXT("Dead Enemy stops observation ticks"), AI->IsActorTickEnabled());
    TestNull(TEXT("Dead Enemy clears its chase target"), AI->GetChaseTarget());
    TestFalse(TEXT("Dead Enemy awareness is shut down"), Enemy->GetAwarenessComponent()->IsInitialized());
    AI->Destroy();
    Enemy->Destroy();
    Target->Destroy();
    Wall->Destroy();
    return true;
}

#endif
