#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "../UI/ShadeHealthWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeHealthWidgetTest, "Shade.UI.HealthWidget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeHealthWidgetTest::RunTest(const FString& Parameters)
{
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false)
        .ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ShadeHealthWidgetTestWorld"),
        nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Test world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Class = LoadClass<UShadeHealthWidget>(nullptr, TEXT("/Game/Shade/UI/WBP_Health.WBP_Health_C"));
    if (!TestNotNull(TEXT("Saved Widget Blueprint reloads"), Class)) return false;
    auto* Widget = CreateWidget<UShadeHealthWidget>(World, Class);
    if (!TestNotNull(TEXT("Widget instance"), Widget)) return false;
    auto* Bar = Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("HealthBar")));
    auto* Text = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("HealthText")));
    if (!TestNotNull(TEXT("HealthBar"), Bar) || !TestNotNull(TEXT("HealthText"), Text)) return false;
    Widget->SetHealthValues(75.f, 100.f);
    TestEqual(TEXT("Fraction from current values"), Bar->GetPercent(), 0.75f);
    TestEqual(TEXT("Numeric display"), Text->GetText().ToString(), FString(TEXT("75 / 100")));
    Widget->SetHealthValues(75.f, 150.f);
    TestEqual(TEXT("Maximum change updates fraction"), Bar->GetPercent(), 0.5f);
    Widget->SetHealthValues(0.f, 0.f);
    TestEqual(TEXT("Zero maximum produces an empty bar"), Bar->GetPercent(), 0.f);
    Widget->SetHealthValues(120.f, 100.f);
    TestEqual(TEXT("Visual overflow is clamped"), Bar->GetPercent(), 1.f);
    TestEqual(TEXT("Text does not silently rewrite gameplay values"), Text->GetText().ToString(), FString(TEXT("120 / 100")));
    return true;
}

#endif
