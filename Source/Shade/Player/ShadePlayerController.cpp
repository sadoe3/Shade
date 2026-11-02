#include "Player/ShadePlayerController.h"
#include "Player/ShadePlayerState.h"
#include "UI/ShadeHealthWidget.h"

void AShadePlayerController::BeginPlay()
{
    Super::BeginPlay();
    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = false;
    if (PlayerCameraManager)
    {
        PlayerCameraManager->ViewPitchMin = -65.f;
        PlayerCameraManager->ViewPitchMax = 40.f;
    }
    RefreshHealthHUD();
}
void AShadePlayerController::RefreshHealthHUD()
{
    if (!IsLocalController() || !GetLocalPlayer()) return;
    auto* State = GetPlayerState<AShadePlayerState>();
    if (!State) return;
    if (!HealthWidget)
    {
        HealthWidget = CreateWidget<UShadeHealthWidget>(this, UShadeHealthWidget::StaticClass());
        if (HealthWidget) HealthWidget->AddToViewport();
    }
    if (HealthWidget) HealthWidget->BindAbilitySystem(State->GetAbilitySystemComponent());
}
void AShadePlayerController::ShadeHealth(float Amount)
{
#if !UE_BUILD_SHIPPING
    if (auto* State = GetPlayerState<AShadePlayerState>()) State->ApplyHealthChange(Amount);
#endif
}
void AShadePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HealthWidget) HealthWidget->RemoveFromParent();
    HealthWidget = nullptr;
    Super::EndPlay(EndPlayReason);
}
