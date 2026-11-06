#include "ShadeStealthEffects.h"
#include "../AbilitySystem/ShadeStealthTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
UShadeInBushEffect::UShadeInBushEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    FInheritedTagContainer Tags;
    Tags.AddTag(ShadeStealthTags::State_Stealth_InBush);
    auto* Grants = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("BushTags"));
    GEComponents.Add(Grants);
    Grants->SetAndApplyTargetTagChanges(Tags);
}
