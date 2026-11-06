#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ShadeHUD.generated.h"

class UShadeHealthWidget;
class UShadeHealthWidgetController;

// Owns the local Health UI lifecycle; gameplay data remains in PlayerState's ASC.
UCLASS()
class SHADE_API AShadeHUD : public AHUD
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UShadeHealthWidget> HealthWidgetClass;

private:
    // Called once by this HUD instance in BeginPlay.
    void InitializeHealthUI();
    void ReleaseHealthUI();

    UPROPERTY(Transient)
    TObjectPtr<UShadeHealthWidget> HealthWidget;

    UPROPERTY(Transient)
    TObjectPtr<UShadeHealthWidgetController> HealthWidgetController;

    FDelegateHandle HealthDisplayHandle;
};
