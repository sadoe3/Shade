#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShadePlayerController.generated.h"

class UShadeHealthWidget;

UCLASS()
class SHADE_API AShadePlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    void RefreshHealthHUD();
    UShadeHealthWidget* GetHealthWidget() const { return HealthWidget; }
    // Development console only; this is not a combat ability or a respawn policy.
    UFUNCTION(Exec) void ShadeHealth(float Amount);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    UPROPERTY(Transient) TObjectPtr<UShadeHealthWidget> HealthWidget;
};
