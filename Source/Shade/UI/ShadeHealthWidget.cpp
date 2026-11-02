#include "UI/ShadeHealthWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ShadeHealthSet.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UShadeHealthWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;
    auto* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HealthPanel"));
    Panel->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.03f, 0.85f));
    Panel->SetPadding(FMargin(14.f));
    auto* PanelSlot = Root->AddChildToCanvas(Panel);
    PanelSlot->SetPosition(FVector2D(28.f, 28.f));
    PanelSlot->SetSize(FVector2D(260.f, 82.f));
    auto* Box = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Box);
    HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
    Box->AddChildToVerticalBox(HealthText);
    HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
    HealthBar->SetFillColorAndOpacity(FLinearColor(0.12f, 0.75f, 0.48f));
    auto* BarSlot = Box->AddChildToVerticalBox(HealthBar);
    BarSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
    BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    SetVisibility(ESlateVisibility::HitTestInvisible);
    RefreshValues();
}
void UShadeHealthWidget::BindAbilitySystem(UAbilitySystemComponent* NewAbilitySystem)
{
    if (AbilitySystem.Get() != NewAbilitySystem)
    {
        UnbindAbilitySystem();
        AbilitySystem = NewAbilitySystem;
        if (NewAbilitySystem)
        {
            HealthHandle = NewAbilitySystem->GetGameplayAttributeValueChangeDelegate(UShadeHealthSet::GetHealthAttribute())
                .AddUObject(this, &ThisClass::OnAttributeChanged);
            MaxHealthHandle = NewAbilitySystem->GetGameplayAttributeValueChangeDelegate(UShadeHealthSet::GetMaxHealthAttribute())
                .AddUObject(this, &ThisClass::OnAttributeChanged);
        }
    }
    RefreshValues();
}
void UShadeHealthWidget::UnbindAbilitySystem()
{
    if (AbilitySystem.IsValid())
    {
        AbilitySystem->GetGameplayAttributeValueChangeDelegate(UShadeHealthSet::GetHealthAttribute()).Remove(HealthHandle);
        AbilitySystem->GetGameplayAttributeValueChangeDelegate(UShadeHealthSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);
    }
    AbilitySystem.Reset();
    HealthHandle.Reset();
    MaxHealthHandle.Reset();
}
void UShadeHealthWidget::OnAttributeChanged(const FOnAttributeChangeData& Data) { RefreshValues(); }
void UShadeHealthWidget::RefreshValues()
{
    const auto* ASC = AbilitySystem.Get();
    DisplayedHealth = ASC ? ASC->GetNumericAttribute(UShadeHealthSet::GetHealthAttribute()) : 0.f;
    DisplayedMaxHealth = ASC ? ASC->GetNumericAttribute(UShadeHealthSet::GetMaxHealthAttribute()) : 0.f;
    if (HealthBar) HealthBar->SetPercent(DisplayedMaxHealth > 0.f ? FMath::Clamp(DisplayedHealth / DisplayedMaxHealth, 0.f, 1.f) : 0.f);
    if (HealthText) HealthText->SetText(FText::Format(NSLOCTEXT("Shade", "HealthDisplay", "Health  {0} / {1}"), FText::AsNumber(FMath::RoundToInt(DisplayedHealth)), FText::AsNumber(FMath::RoundToInt(DisplayedMaxHealth))));
}
void UShadeHealthWidget::NativeDestruct()
{
    UnbindAbilitySystem();
    Super::NativeDestruct();
}
