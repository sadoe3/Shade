#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "../Player/ShadePlayerState.h"
#include "../AbilitySystem/ShadeAttributeSet.h"
#include "../AbilitySystem/ShadeHealthEffects.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeHealthChangeTest, "Shade.Health.Change",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeHealthChangeTest::RunTest(const FString& Parameters)
{
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false)
        .ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HealthChangeWorld"),
        nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("World"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    auto* StateClass = LoadClass<AShadePlayerState>(nullptr, TEXT("/Game/Shade/Player/BP_ShadePlayerState.BP_ShadePlayerState_C"));
    if (!TestNotNull(TEXT("State class"), StateClass)) return false;
    auto* State = World->SpawnActor<AShadePlayerState>(StateClass);
    auto* Avatar = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("State"), State) || !TestNotNull(TEXT("Avatar"), Avatar)) return false;
    State->InitializeAbilitySystem(Avatar);
    auto* ASC = State->GetAbilitySystemComponent();
    const auto* Attributes = State->GetAttributeSet();
    TestTrue(TEXT("Damage applies"), ShadeHealthEffects::ApplyHealthChange(ASC, -35.f));
    TestEqual(TEXT("Damage leaves65"), Attributes->GetHealth(), 65.f);
    TestTrue(TEXT("Healing applies"), ShadeHealthEffects::ApplyHealthChange(ASC, 15.f));
    TestEqual(TEXT("Healing leaves80"), Attributes->GetHealth(), 80.f);
    ShadeHealthEffects::ApplyHealthChange(ASC, 300.f);
    TestEqual(TEXT("Overheal clamps current"), Attributes->GetHealth(), 100.f);
    TestEqual(TEXT("Overheal clamps base"), Attributes->Health.GetBaseValue(), 100.f);
    ShadeHealthEffects::ApplyHealthChange(ASC, -10.f);
    TestEqual(TEXT("Overheal does not accumulate hidden base health"), Attributes->GetHealth(), 90.f);
    ASC->SetNumericAttributeBase(UShadeAttributeSet::GetMaxHealthAttribute(), 50.f);
    TestEqual(TEXT("Maximum reduction clamps Health"), Attributes->GetHealth(), 50.f);
    ASC->SetNumericAttributeBase(UShadeAttributeSet::GetMaxHealthAttribute(), 100.f);
    TestEqual(TEXT("Maximum increase does not heal"), Attributes->GetHealth(), 50.f);
    ShadeHealthEffects::ApplyHealthChange(ASC, -300.f);
    TestEqual(TEXT("Overdamage clamps current"), Attributes->GetHealth(), 0.f);
    TestEqual(TEXT("Overdamage clamps base"), Attributes->Health.GetBaseValue(), 0.f);
    ASC->SetNumericAttributeBase(UShadeAttributeSet::GetMaxHealthAttribute(), -10.f);
    TestEqual(TEXT("Maximum cannot be zero or negative"), Attributes->GetMaxHealth(), 1.f);
    TestFalse(TEXT("Null target is rejected"), ShadeHealthEffects::ApplyHealthChange(nullptr, 10.f));
    return true;
}
#endif
