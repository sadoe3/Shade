#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShadeHealthWidget.generated.h"

class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;
struct FOnAttributeChangeData;

UCLASS()
class SHADE_API UShadeHealthWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void BindAbilitySystem(UAbilitySystemComponent* NewAbilitySystem);
    float GetDisplayedHealth() const { return DisplayedHealth; }
    float GetDisplayedMaxHealth() const { return DisplayedMaxHealth; }
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
private:
    void UnbindAbilitySystem();
    void RefreshValues();
    void OnAttributeChanged(const FOnAttributeChangeData& Data);
    TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
    FDelegateHandle HealthHandle;
    FDelegateHandle MaxHealthHandle;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> HealthText;
    float DisplayedHealth = 0.f;
    float DisplayedMaxHealth = 0.f;
};
