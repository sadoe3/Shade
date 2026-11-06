#include "ShadeHUD.h"

#include "ShadeHealthWidget.h"
#include "ShadeHealthWidgetController.h"
#include "../Player/ShadePlayerState.h"
#include "GameFramework/PlayerController.h"

void AShadeHUD::BeginPlay()
{
    Super::BeginPlay();
    InitializeHealthUI();
}

void AShadeHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ReleaseHealthUI();
    Super::EndPlay(EndPlayReason);
}

void AShadeHUD::InitializeHealthUI()
{
    APlayerController* PC = GetOwningPlayerController();
    if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer()) return;

    auto* State = PC->GetPlayerState<AShadePlayerState>();
    if (!State) return;
    if (!HealthWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("%s requires HealthWidgetClass."), *GetName());
        return;
    }

    HealthWidget = CreateWidget<UShadeHealthWidget>(PC, HealthWidgetClass);
    if (!ensure(HealthWidget)) return;

    HealthWidgetController = NewObject<UShadeHealthWidgetController>(this);
    HealthDisplayHandle = HealthWidgetController->OnHealthValuesChanged.AddUObject(
        HealthWidget.Get(), &UShadeHealthWidget::SetHealthValues);

    if (!HealthWidgetController->BindAbilitySystem(State->GetAbilitySystemComponent()))
    {
        ReleaseHealthUI();
        return;
    }

    // Subscribe once, then display current values. Later changes arrive through GAS delegates.
    HealthWidgetController->BroadcastCurrentValues();
    HealthWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
    HealthWidget->SetPositionInViewport(FVector2D(24.f, 24.f), false);
    HealthWidget->SetDesiredSizeInViewport(FVector2D(260.f, 52.f));
    if (!HealthWidget->AddToPlayerScreen())
    {
        UE_LOG(LogTemp, Error, TEXT("%s could not add the Health widget to the local player screen."), *GetName());
        ReleaseHealthUI();
    }
}

void AShadeHUD::ReleaseHealthUI()
{
    if (HealthWidgetController)
    {
        HealthWidgetController->UnbindAbilitySystem();
        HealthWidgetController->OnHealthValuesChanged.Remove(HealthDisplayHandle);
    }
    HealthDisplayHandle.Reset();
    if (HealthWidget)
    {
        HealthWidget->RemoveFromParent();
    }
    HealthWidget = nullptr;
    HealthWidgetController = nullptr;
}
