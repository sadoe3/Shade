#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShadeHealthWidget.generated.h"

class UProgressBar;
class UTextBlock;

// Presentation only. The UI owner connects the WidgetController in a later step.
UCLASS(Abstract)
class SHADE_API UShadeHealthWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetHealthValues(float Health, float MaxHealth);

protected:
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UProgressBar> HealthBar;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UTextBlock> HealthText;
};
