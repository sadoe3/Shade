#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ShadeAwarenessSet.generated.h"

#define SHADE_AWARENESS_ACCESSORS(PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UShadeAwarenessSet, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class SHADE_API UShadeAwarenessSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UShadeAwarenessSet();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
    virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;


    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Alertness, Category="Awareness")
    FGameplayAttributeData Alertness;
    SHADE_AWARENESS_ACCESSORS(Alertness)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxAlertness, Category="Awareness")
    FGameplayAttributeData MaxAlertness;
    SHADE_AWARENESS_ACCESSORS(MaxAlertness)

private:
    void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
    UFUNCTION() void OnRep_Alertness(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_MaxAlertness(const FGameplayAttributeData& OldValue) const;
};

#undef SHADE_AWARENESS_ACCESSORS
