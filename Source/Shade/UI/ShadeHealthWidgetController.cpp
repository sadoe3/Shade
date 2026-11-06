#include "ShadeHealthWidgetController.h"

#include "AbilitySystemComponent.h"
#include "../AbilitySystem/ShadeAttributeSet.h"

bool UShadeHealthWidgetController::BindAbilitySystem(UAbilitySystemComponent* InAbilitySystem)
{
    if (IsValid(InAbilitySystem) && AbilitySystem.Get() == InAbilitySystem)
    {
        return true;
    }

    UnbindAbilitySystem();
    if (!IsValid(InAbilitySystem) || !InAbilitySystem->GetSet<UShadeAttributeSet>())
    {
        return false;
    }

    AbilitySystem = InAbilitySystem;
    HealthChangedHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(
        UShadeAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleAttributeChanged);
    MaxHealthChangedHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(
        UShadeAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleAttributeChanged);
    return true;
}

void UShadeHealthWidgetController::BroadcastCurrentValues()
{
    const auto* ASC = AbilitySystem.Get();
    const auto* Attributes = ASC ? ASC->GetSet<UShadeAttributeSet>() : nullptr;
    if (Attributes)
    {
        OnHealthValuesChanged.Broadcast(Attributes->GetHealth(), Attributes->GetMaxHealth());
    }
}

void UShadeHealthWidgetController::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
    BroadcastCurrentValues();
}

void UShadeHealthWidgetController::UnbindAbilitySystem()
{
    if (auto* ASC = AbilitySystem.Get())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(UShadeAttributeSet::GetHealthAttribute())
            .Remove(HealthChangedHandle);
        ASC->GetGameplayAttributeValueChangeDelegate(UShadeAttributeSet::GetMaxHealthAttribute())
            .Remove(MaxHealthChangedHandle);
    }
    HealthChangedHandle.Reset();
    MaxHealthChangedHandle.Reset();
    AbilitySystem.Reset();
}

void UShadeHealthWidgetController::BeginDestroy()
{
    UnbindAbilitySystem();
    Super::BeginDestroy();
}
