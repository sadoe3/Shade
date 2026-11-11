#include "ShadeStealthEffects.h"
#include "../AbilitySystem/ShadeGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
UShadeInBushEffect::UShadeInBushEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    FInheritedTagContainer Tags;
    Tags.AddTag(ShadeGameplayTags::State_Stealth_InBush);
    auto* Grants = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("BushTags"));
    GEComponents.Add(Grants);
    Grants->SetAndApplyTargetTagChanges(Tags);
}
