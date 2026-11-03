#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ShadeAttributeSet.generated.h"

#define SHADE_ATTRIBUTE_ACCESSORS(PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UShadeAttributeSet, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class SHADE_API UShadeAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UShadeAttributeSet();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Health")
    FGameplayAttributeData Health;
    SHADE_ATTRIBUTE_ACCESSORS(Health)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Health")
    FGameplayAttributeData MaxHealth;
    SHADE_ATTRIBUTE_ACCESSORS(MaxHealth)

private:
    UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue) const;
};

#undef SHADE_ATTRIBUTE_ACCESSORS
