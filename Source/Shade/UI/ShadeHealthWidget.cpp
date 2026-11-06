#include "ShadeHealthWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UShadeHealthWidget::SetHealthValues(float Health, float MaxHealth)
{
    if (!ensure(HealthBar && HealthText))
    {
        return;
    }

    // Clamp the visual fraction only; gameplay limits belong to the AttributeSet.
    const float Fraction = FMath::IsFinite(Health) && FMath::IsFinite(MaxHealth) && MaxHealth > 0.f
        ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f;
    HealthBar->SetPercent(Fraction);
    HealthText->SetText(FText::Format(NSLOCTEXT("Shade", "HealthValues", "{0} / {1}"),
        FText::AsNumber(Health), FText::AsNumber(MaxHealth)));
}
