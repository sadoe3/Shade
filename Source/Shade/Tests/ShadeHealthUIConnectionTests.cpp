#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "../Player/ShadePlayerController.h"
#include "../Player/ShadePlayerState.h"
#include "../AbilitySystem/ShadeAttributeSet.h"
#include "../UI/ShadeHUD.h"
#include "../UI/ShadeHealthWidget.h"
#include "../UI/ShadeHealthWidgetController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeHealthUIConnectionTest, "Shade.UI.ConnectionPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeHealthUIConnectionTest::RunTest(const FString& Parameters)
{
    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!TestTrue(TEXT("Use the existing isolated verification map"),
        EditorWorld && EditorWorld->GetName().Contains(TEXT("L_InputConnection")))) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]()
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
        auto* PC = Cast<AShadePlayerController>(World->GetFirstPlayerController());
        if (!TestNotNull(TEXT("Configured controller"), PC)) return true;
        auto* HUD = PC->GetHUD<AShadeHUD>();
        if (!TestNotNull(TEXT("GameMode configured the custom HUD"), HUD)) return true;
        auto* WidgetProperty = FindFProperty<FObjectPropertyBase>(HUD->GetClass(), TEXT("HealthWidget"));
        auto* BridgeProperty = FindFProperty<FObjectPropertyBase>(HUD->GetClass(), TEXT("HealthWidgetController"));
        if (!TestNotNull(TEXT("HUD widget reference property"), WidgetProperty) ||
            !TestNotNull(TEXT("HUD bridge reference property"), BridgeProperty)) return true;
        auto* Widget = Cast<UShadeHealthWidget>(WidgetProperty->GetObjectPropertyValue_InContainer(HUD));
        auto* Bridge = Cast<UShadeHealthWidgetController>(BridgeProperty->GetObjectPropertyValue_InContainer(HUD));
        auto* State = PC->GetPlayerState<AShadePlayerState>();
        if (!TestNotNull(TEXT("HUD created widget"), Widget) || !TestNotNull(TEXT("HUD created bridge"), Bridge) ||
            !TestNotNull(TEXT("PlayerState"), State)) return true;
        if (!TestNotNull(TEXT("Widget tree"), Widget->WidgetTree.Get())) return true;
        auto* Bar = Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("HealthBar")));
        auto* Text = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("HealthText")));
        if (!TestNotNull(TEXT("Bar"), Bar) || !TestNotNull(TEXT("Text"), Text)) return true;
        TestTrue(TEXT("Added to viewport"), Widget->IsInViewport());
        TestTrue(TEXT("Owning player remains the local controller"), Widget->GetOwningPlayer() == PC);
        TestTrue(TEXT("HUD is the bridge Outer"), Bridge->GetOuter() == HUD);
        TestEqual(TEXT("Current initial health is displayed"), Text->GetText().ToString(), FString(TEXT("100 / 100")));
        TestEqual(TEXT("Initial bar is full"), Bar->GetPercent(), 1.f);

        auto* ASC = State->GetAbilitySystemComponent();
        if (!TestNotNull(TEXT("PlayerState ASC"), ASC)) return true;
        ASC->SetNumericAttributeBase(UShadeAttributeSet::GetHealthAttribute(), 60.f);
        TestEqual(TEXT("Health delegate reaches view"), Text->GetText().ToString(), FString(TEXT("60 / 100")));
        TestEqual(TEXT("Health updates fraction"), Bar->GetPercent(), 0.6f);
        ASC->SetNumericAttributeBase(UShadeAttributeSet::GetMaxHealthAttribute(), 120.f);
        TestEqual(TEXT("MaxHealth updates fraction"), Bar->GetPercent(), 0.5f);

        APawn* Body = PC->GetPawn();
        if (!TestNotNull(TEXT("Possessed body"), Body)) return true;
        int32 RepossessBroadcasts = 0;
        const FDelegateHandle RepossessHandle = Bridge->OnHealthValuesChanged.AddLambda(
            [&RepossessBroadcasts](float, float) { ++RepossessBroadcasts; });
        PC->UnPossess();
        PC->Possess(Body);
        Bridge->OnHealthValuesChanged.Remove(RepossessHandle);
        TestEqual(TEXT("Repossess with unchanged attributes does not rebroadcast UI values"), RepossessBroadcasts, 0);
        TestTrue(TEXT("Repossess reuses HUD"), PC->GetHUD() == HUD);
        TestTrue(TEXT("Repossess reuses existing widget"), WidgetProperty->GetObjectPropertyValue_InContainer(HUD) == Widget);
        TestTrue(TEXT("Repossess reuses bridge"), BridgeProperty->GetObjectPropertyValue_InContainer(HUD) == Bridge);
        TestEqual(TEXT("Repossess displays current values, not defaults"), Text->GetText().ToString(), FString(TEXT("60 / 120")));

        // Retain old objects to observe HUD replacement cleanup without depending on GC timing.
        TStrongObjectPtr<AShadeHUD> RetainedHUD(HUD);
        TStrongObjectPtr<UShadeHealthWidget> RetainedWidget(Widget);
        TStrongObjectPtr<UShadeHealthWidgetController> RetainedBridge(Bridge);
        TStrongObjectPtr<UAbilitySystemComponent> RetainedASC(ASC);
        PC->ClientSetHUD(HUD->GetClass());
        TestFalse(TEXT("Replacing HUD removes the old screen widget"), Widget->IsInViewport());
        TestNull(TEXT("Old HUD clears widget reference"), WidgetProperty->GetObjectPropertyValue_InContainer(HUD));
        TestNull(TEXT("Old HUD clears bridge reference"), BridgeProperty->GetObjectPropertyValue_InContainer(HUD));
        TestFalse(TEXT("Old HUD removes output subscription"), Bridge->OnHealthValuesChanged.IsBound());

        auto* NewHUD = PC->GetHUD<AShadeHUD>();
        if (!TestNotNull(TEXT("Replacement HUD"), NewHUD)) return true;
        TestTrue(TEXT("Replacement HUD is a new instance"), NewHUD != HUD);
        auto* NewWidget = Cast<UShadeHealthWidget>(WidgetProperty->GetObjectPropertyValue_InContainer(NewHUD));
        auto* NewBridge = Cast<UShadeHealthWidgetController>(BridgeProperty->GetObjectPropertyValue_InContainer(NewHUD));
        if (!TestNotNull(TEXT("Replacement widget"), NewWidget) ||
            !TestNotNull(TEXT("Replacement bridge"), NewBridge)) return true;
        if (!TestNotNull(TEXT("Replacement widget tree"), NewWidget->WidgetTree.Get())) return true;
        auto* NewBar = Cast<UProgressBar>(NewWidget->WidgetTree->FindWidget(TEXT("HealthBar")));
        auto* NewText = Cast<UTextBlock>(NewWidget->WidgetTree->FindWidget(TEXT("HealthText")));
        if (!TestNotNull(TEXT("Replacement bar"), NewBar) || !TestNotNull(TEXT("Replacement text"), NewText)) return true;
        TestTrue(TEXT("Replacement widget is a new instance"), NewWidget != Widget);
        TestTrue(TEXT("Replacement widget is in viewport"), NewWidget->IsInViewport());
        TestTrue(TEXT("Replacement owns the new bridge"), NewBridge->GetOuter() == NewHUD);
        TestTrue(TEXT("Replacement widget keeps owning player"), NewWidget->GetOwningPlayer() == PC);
        TestEqual(TEXT("Replacement starts from current values"), NewText->GetText().ToString(), FString(TEXT("60 / 120")));
        TestEqual(TEXT("Replacement current fraction"), NewBar->GetPercent(), 0.5f);
        ASC->SetNumericAttributeBase(UShadeAttributeSet::GetHealthAttribute(), 30.f);
        TestEqual(TEXT("Replacement receives later changes"), NewText->GetText().ToString(), FString(TEXT("30 / 120")));
        TestEqual(TEXT("Replacement fraction changes"), NewBar->GetPercent(), 0.25f);
        TestEqual(TEXT("Replaced view no longer receives changes"), Text->GetText().ToString(), FString(TEXT("60 / 120")));

        TStrongObjectPtr<AShadeHUD> RetainedNewHUD(NewHUD);
        TStrongObjectPtr<UShadeHealthWidget> RetainedNewWidget(NewWidget);
        TStrongObjectPtr<UShadeHealthWidgetController> RetainedNewBridge(NewBridge);
        PC->Destroy();
        TestFalse(TEXT("Controller destruction removes current screen widget"), NewWidget->IsInViewport());
        TestNull(TEXT("Current HUD clears widget reference"), WidgetProperty->GetObjectPropertyValue_InContainer(NewHUD));
        TestNull(TEXT("Current HUD clears bridge reference"), BridgeProperty->GetObjectPropertyValue_InContainer(NewHUD));
        TestFalse(TEXT("Current HUD removes output subscription"), NewBridge->OnHealthValuesChanged.IsBound());
        ASC->SetNumericAttributeBase(UShadeAttributeSet::GetHealthAttribute(), 15.f);
        TestEqual(TEXT("Detached current view no longer receives changes"), NewText->GetText().ToString(), FString(TEXT("30 / 120")));
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
