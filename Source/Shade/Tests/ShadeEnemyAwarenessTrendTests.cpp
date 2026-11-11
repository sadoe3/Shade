#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "AbilitySystemComponent.h"
#include "../AbilitySystem/Data/ShadeInitialAttributesData.h"
#include "../AbilitySystem/Data/ShadeEnemyBehaviorData.h"
#include "../AbilitySystem/ShadeAttributeSet.h"
#include "../AbilitySystem/ShadeAwarenessSet.h"
#include "../AbilitySystem/ShadeAwarenessEffects.h"
#include "../AbilitySystem/ShadeGameplayTags.h"
#include "../AbilitySystem/ShadeHealthEffects.h"

#include "../Character/ShadeEnemyCharacter.h"

namespace
{
    struct FAwarenessTestWorld
    {
        UWorld* World = nullptr;
        AShadeEnemyCharacter* First = nullptr;
        AShadeEnemyCharacter* Second = nullptr;
        FActiveGameplayEffectHandle TrendHandle;
        float StoppedValue = 0.f;
        int32 Phase = 0;
        float Elapsed = 0.f;
        uint64 LastFrame = MAX_uint64;
        ~FAwarenessTestWorld()
        {
            if (World)
            {
                World->DestroyWorld(false);
                GEngine->DestroyWorldContext(World);
            }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeEnemyAwarenessTrendTest, "Shade.Enemy.AwarenessTrend",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeEnemyAwarenessTrendTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine context owner"), GEngine)) return false;
    auto State = MakeShared<FAwarenessTestWorld>();
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false)
        .ShouldSimulatePhysics(false).SetTransactional(false);
    State->World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ShadeAwarenessTestWorld"),
        nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Isolated awareness world"), State->World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(State->World);
    State->World->InitializeActorsForPlay(FURL());
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    State->First = State->World->SpawnActor<AShadeEnemyCharacter>(Spawn);
    State->Second = State->World->SpawnActor<AShadeEnemyCharacter>(Spawn);
    if (!TestNotNull(TEXT("First Enemy"), State->First) || !TestNotNull(TEXT("Second Enemy"), State->Second)) return false;
    auto* HealthProperty = FindFProperty<FObjectPropertyBase>(AShadeEnemyCharacter::StaticClass(), TEXT("InitialAttributesData"));
    auto* BehaviorProperty = FindFProperty<FObjectPropertyBase>(AShadeEnemyCharacter::StaticClass(), TEXT("BehaviorData"));
    if (!HealthProperty || !BehaviorProperty) return false;
    auto* Health = NewObject<UShadeInitialAttributesData>(State->World);
    Health->InitialHealth = 100.f;
    Health->WalkSpeed = 180.f;
    Health->RunSpeed = 300.f;
    Health->BrakingDecelerationWalking = 1800.f;
    auto* Behavior = NewObject<UShadeEnemyBehaviorData>(State->World);
    for (auto* Enemy : { State->First, State->Second })
    {
        HealthProperty->SetObjectPropertyValue_InContainer(Enemy, Health);
        BehaviorProperty->SetObjectPropertyValue_InContainer(Enemy, Behavior);
        Enemy->DispatchBeginPlay();
    }
    auto* ASC = State->First->GetAbilitySystemComponent();
    auto Spec = ASC->MakeOutgoingSpec(UShadeAlertnessTrendEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
    if (!TestTrue(TEXT("Trend spec exists"), Spec.IsValid())) return false;
    Spec.Data->SetSetByCallerMagnitude(ShadeGameplayTags::Data_Awareness_Change, 25.f);
    State->TrendHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    TestTrue(TEXT("Periodic effect has an active handle"), State->TrendHandle.IsValid());
    TestEqual(TEXT("Application does not immediately execute"), State->First->GetAwarenessSet()->GetAlertness(), 0.f);

    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (State->LastFrame == GFrameCounter) return false;
        State->LastFrame = GFrameCounter;
        State->World->Tick(LEVELTICK_All, 0.05f);
        State->Elapsed += 0.05f;
        auto* ASC = State->First->GetAbilitySystemComponent();
        auto* Attributes = State->First->GetAwarenessSet();
        switch (State->Phase)
        {
        case 0:
            if (State->Elapsed < 0.7f) return false;
            TestEqual(TEXT("Positive periodic delta reaches upper bound"), Attributes->GetAlertness(), 100.f);
            TestEqual(TEXT("Other Enemy does not change"), State->Second->GetAwarenessSet()->GetAlertness(), 0.f);
            ASC->RemoveActiveGameplayEffect(State->TrendHandle);
            ASC->SetNumericAttributeBase(UShadeAwarenessSet::GetAlertnessAttribute(), 50.f);
            State->Elapsed = 0.f;
            ++State->Phase;
            return false;
        case 1:
            if (State->Elapsed < 0.3f) return false;
            TestEqual(TEXT("Removed gain effect stops future executions"), Attributes->GetAlertness(), 50.f);
            {
                auto Spec = ASC->MakeOutgoingSpec(UShadeAlertnessTrendEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
                if (!TestTrue(TEXT("Decay spec exists"), Spec.IsValid())) return true;
                Spec.Data->SetSetByCallerMagnitude(ShadeGameplayTags::Data_Awareness_Change, -25.f);
                State->TrendHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
            }
            State->Elapsed = 0.f;
            ++State->Phase;
            return false;
        case 2:
            if (State->Elapsed < 0.5f) return false;
            TestEqual(TEXT("Negative periodic delta reaches zero"), Attributes->GetAlertness(), 0.f);
            TestEqual(TEXT("Base value also remains bounded"), Attributes->Alertness.GetBaseValue(), 0.f);
            ASC->RemoveActiveGameplayEffect(State->TrendHandle);
            State->StoppedValue = Attributes->GetAlertness();
            State->Elapsed = 0.f;
            ++State->Phase;
            return false;
        case 3:
            if (State->Elapsed < 0.3f) return false;
            TestEqual(TEXT("Removal does not undo previous periodic changes"), Attributes->GetAlertness(), State->StoppedValue);
            TestEqual(TEXT("No trend effect remains"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 0);
            return true;
        }
        return true;
    }));
    return true;
}

#endif
