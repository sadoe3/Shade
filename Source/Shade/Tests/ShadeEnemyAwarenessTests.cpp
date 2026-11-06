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
#include "../AbilitySystem/ShadeEnemyTags.h"
#include "../AbilitySystem/ShadeGameplayTags.h"
#include "../AbilitySystem/ShadeHealthEffects.h"
#include "../AI/ShadeAwarenessComponent.h"
#include "../Character/ShadeEnemyCharacter.h"

namespace
{
    struct FAwarenessTestWorld
    {
        UWorld* World = nullptr;
        AShadeEnemyCharacter* First = nullptr;
        AShadeEnemyCharacter* Second = nullptr;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeEnemyAwarenessTest, "Shade.Enemy.Awareness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeEnemyAwarenessTest::RunTest(const FString& Parameters)
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
    TestEqual(TEXT("Initial Alertness is zero"), State->First->GetAwarenessSet()->GetAlertness(), 0.f);
    TestTrue(TEXT("Initial Calm tag is granted through GE"), ASC->HasMatchingGameplayTag(ShadeEnemyTags::State_Awareness_Calm));
    TestEqual(TEXT("Only a state effect is initially active"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 1);
    State->First->GetAwarenessComponent()->SetTargetVisible(true);

    // One world tick per engine frame exercises the real periodic GE and decay timer.
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (State->LastFrame == GFrameCounter) return false;
        State->LastFrame = GFrameCounter;
        State->World->Tick(LEVELTICK_All, 0.05f);
        State->Elapsed += 0.05f;
        auto* Enemy = State->First;
        auto* Awareness = Enemy->GetAwarenessComponent();
        auto* Attributes = Enemy->GetAwarenessSet();
        auto* ASC = Enemy->GetAbilitySystemComponent();
        switch (State->Phase)
        {
        case 0:
            Awareness->SetTargetVisible(true); // Repeated observation must not stack effects.
            if (State->Elapsed < 0.3f) return false;
            TestTrue(TEXT("Continuous sight first becomes Suspicious"), Attributes->GetAlertness() > 0.f && Attributes->GetAlertness() < 100.f);
            TestTrue(TEXT("Suspicious tag is active"), ASC->HasMatchingGameplayTag(ShadeEnemyTags::State_Awareness_Suspicious));
            TestFalse(TEXT("Calm was removed"), ASC->HasMatchingGameplayTag(ShadeEnemyTags::State_Awareness_Calm));
            TestEqual(TEXT("Repeated sight has one trend and one state effect"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 2);
            ++State->Phase;
            return false;
        case 1:
            if (State->Elapsed < 1.3f) return false;
            TestEqual(TEXT("Continuous sight reaches capped Alertness"), Attributes->GetAlertness(), 100.f);
            TestTrue(TEXT("Alert tag is active"), ASC->HasMatchingGameplayTag(ShadeEnemyTags::State_Awareness_Alert));
            TestEqual(TEXT("Another Enemy remains Calm independently"), State->Second->GetAwarenessSet()->GetAlertness(), 0.f);
            ASC->SetNumericAttributeBase(UShadeAwarenessSet::GetMaxAlertnessAttribute(), 50.f);
            TestEqual(TEXT("Reducing the maximum also bounds current Alertness"), Attributes->GetAlertness(), 50.f);
            TestTrue(TEXT("Alert uses the changed maximum"), ASC->HasMatchingGameplayTag(ShadeEnemyTags::State_Awareness_Alert));
            ASC->SetNumericAttributeBase(UShadeAwarenessSet::GetMaxAlertnessAttribute(), 100.f);
            TestTrue(TEXT("Raising maximum recalculates state"), ASC->HasMatchingGameplayTag(ShadeEnemyTags::State_Awareness_Suspicious));
            ASC->SetNumericAttributeBase(UShadeAwarenessSet::GetAlertnessAttribute(), 100.f);
            Awareness->SetTargetVisible(false);
            State->Elapsed = 0.f;
            ++State->Phase;
            return false;
        case 2:
            Awareness->SetTargetVisible(false); // Does not restart the delay.
            if (State->Elapsed < 1.f) return false;
            TestEqual(TEXT("Sight loss preserves Alertness during decay delay"), Attributes->GetAlertness(), 100.f);
            ++State->Phase;
            return false;
        case 3:
            if (State->Elapsed < 2.5f) return false;
            TestTrue(TEXT("Decay starts after delay even under repeated lost sight"), Attributes->GetAlertness() > 0.f && Attributes->GetAlertness() < 100.f);
            TestTrue(TEXT("Decay transitions Alert to Suspicious"), ASC->HasMatchingGameplayTag(ShadeEnemyTags::State_Awareness_Suspicious));
            Awareness->SetTargetVisible(true);
            State->Elapsed = 0.f;
            ++State->Phase;
            return false;
        case 4:
            if (State->Elapsed < 0.5f) return false;
            TestEqual(TEXT("Reacquisition replaces decay with gain"), Attributes->GetAlertness(), 100.f);
            TestEqual(TEXT("Gain and decay never coexist"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 2);
            Awareness->SetTargetVisible(false);
            State->Elapsed = 0.f;
            ++State->Phase;
            return false;
        case 5:
            if (State->Elapsed < 6.5f) return false;
            TestEqual(TEXT("Long loss decays to zero without underflow"), Attributes->GetAlertness(), 0.f);
            TestEqual(TEXT("Base Alertness is also bounded"), Attributes->Alertness.GetBaseValue(), 0.f);
            TestTrue(TEXT("Calm returns"), ASC->HasMatchingGameplayTag(ShadeEnemyTags::State_Awareness_Calm));
            Awareness->SetTargetVisible(true);
            State->Elapsed = 0.f;
            ++State->Phase;
            return false;
        case 6:
            if (State->Elapsed < 0.3f) return false;
            TestTrue(TEXT("Sighting works again after Calm"), Attributes->GetAlertness() > 0.f);
            ShadeHealthEffects::ApplyHealthChange(ASC, -100.f);
            TestTrue(TEXT("Death was applied"), ASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead));
            TestFalse(TEXT("Death shuts awareness down"), Awareness->IsInitialized());
            TestFalse(TEXT("Death removes the Awareness stage effect"), ASC->HasMatchingGameplayTag(ShadeEnemyTags::State_Awareness_Suspicious));
            TestEqual(TEXT("Only shared Dead effect remains"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 1);
            return true;
        }
        return true;
    }));
    return true;
}

#endif
