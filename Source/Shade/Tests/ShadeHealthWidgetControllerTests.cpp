#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "../AbilitySystem/ShadeAttributeSet.h"
#include "../Player/ShadePlayerState.h"
#include "../UI/ShadeHealthWidgetController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeHealthWidgetControllerTest,
    "Shade.UI.HealthWidgetController",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeHealthWidgetControllerTest::RunTest(const FString& Parameters)
{
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false)
        .ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ShadeHealthUITestWorld"),
        nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());

    UClass* StateClass = LoadClass<AShadePlayerState>(nullptr,
        TEXT("/Game/Shade/Player/BP_ShadePlayerState.BP_ShadePlayerState_C"));
    if (!TestNotNull(TEXT("Configured PlayerState"), StateClass)) return false;
    auto* State = World->SpawnActor<AShadePlayerState>(StateClass);
    auto* OtherState = World->SpawnActor<AShadePlayerState>(StateClass);
    auto* Avatar = World->SpawnActor<AActor>();
    if (!State || !OtherState || !Avatar) return false;
    State->InitializeAbilitySystem(Avatar);
    OtherState->InitializeAbilitySystem(Avatar);
    auto* ASC = State->GetAbilitySystemComponent();
    auto* OtherASC = OtherState->GetAbilitySystemComponent();
    const auto Health = UShadeAttributeSet::GetHealthAttribute();
    const auto MaxHealth = UShadeAttributeSet::GetMaxHealthAttribute();

    // Arrange pre-existing gameplay values; this test does not implement damage/healing.
    ASC->SetNumericAttributeBase(Health, 70.f);
    OtherASC->SetNumericAttributeBase(Health, 33.f);
    auto* Bridge = NewObject<UShadeHealthWidgetController>(State);
    int32 Calls = 0;
    float LastHealth = -1.f;
    float LastMaxHealth = -1.f;
    const auto Listener = Bridge->OnHealthValuesChanged.AddLambda([&](float Current, float Maximum)
    {
        ++Calls;
        LastHealth = Current;
        LastMaxHealth = Maximum;
    });
    ON_SCOPE_EXIT
    {
        Bridge->UnbindAbilitySystem();
        Bridge->OnHealthValuesChanged.Remove(Listener);
    };

    TestFalse(TEXT("Null source cannot bind"), Bridge->BindAbilitySystem(nullptr));
    Bridge->BroadcastCurrentValues();
    TestEqual(TEXT("No fabricated values without a source"), Calls, 0);
    TestTrue(TEXT("Bind ready source"), Bridge->BindAbilitySystem(ASC));
    TestEqual(TEXT("Subscription does not replay past changes"), Calls, 0);
    Bridge->BroadcastCurrentValues();
    TestEqual(TEXT("Initial snapshot notification"), Calls, 1);
    TestEqual(TEXT("Late UI reads current health, not initial data"), LastHealth, 70.f);
    TestEqual(TEXT("Current MaxHealth"), LastMaxHealth, 100.f);

    TestTrue(TEXT("Same source can be reused"), Bridge->BindAbilitySystem(ASC));
    ASC->SetNumericAttributeBase(Health, 50.f);
    TestEqual(TEXT("Repeated binding does not duplicate notifications"), Calls, 2);
    TestEqual(TEXT("Health notification value"), LastHealth, 50.f);
    ASC->SetNumericAttributeBase(MaxHealth, 200.f);
    TestEqual(TEXT("MaxHealth also notifies"), Calls, 3);
    TestEqual(TEXT("MaxHealth notification value"), LastMaxHealth, 200.f);

    TestTrue(TEXT("Switch source"), Bridge->BindAbilitySystem(OtherASC));
    ASC->SetNumericAttributeBase(Health, 40.f);
    ASC->SetNumericAttributeBase(MaxHealth, 250.f);
    TestEqual(TEXT("Old source subscriptions are removed"), Calls, 3);
    Bridge->BroadcastCurrentValues();
    TestEqual(TEXT("Replacement source current health"), LastHealth, 33.f);
    TestEqual(TEXT("Replacement source current maximum"), LastMaxHealth, 100.f);
    OtherASC->SetNumericAttributeBase(Health, 20.f);
    TestEqual(TEXT("Replacement source sends one notification"), Calls, 5);

    Bridge->UnbindAbilitySystem();
    Bridge->UnbindAbilitySystem();
    OtherASC->SetNumericAttributeBase(Health, 10.f);
    OtherASC->SetNumericAttributeBase(MaxHealth, 150.f);
    Bridge->BroadcastCurrentValues();
    TestEqual(TEXT("Unbound bridge stops both notifications"), Calls, 5);
    TestFalse(TEXT("Health subscription removed"), OtherASC->GetGameplayAttributeValueChangeDelegate(Health).IsBoundToObject(Bridge));
    TestFalse(TEXT("MaxHealth subscription removed"), OtherASC->GetGameplayAttributeValueChangeDelegate(MaxHealth).IsBoundToObject(Bridge));
    TestTrue(TEXT("Reopen with same source"), Bridge->BindAbilitySystem(OtherASC));
    Bridge->BroadcastCurrentValues();
    TestEqual(TEXT("Reopened UI reads latest health"), LastHealth, 10.f);
    TestEqual(TEXT("Reopened UI reads latest maximum"), LastMaxHealth, 150.f);
    return true;
}

#endif
