#include "ShadeHealthEffects.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#include "ShadeAttributeSet.h"
#include "../Player/ShadePlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "String/LexFromString.h"

namespace
{
FAutoConsoleCommandWithWorldAndArgs HealthCommand(
    TEXT("Shade.Health"), TEXT("Shade.Health <signed delta>: change local Player Health through GAS; negative damages, positive heals."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        float Delta = 0.f;
        if (Args.Num() != 1 || !LexTryParseString(Delta, *Args[0]) || !FMath::IsFinite(Delta))
        {
            UE_LOG(LogTemp, Display, TEXT("Usage: Shade.Health <signed delta>, for example Shade.Health -25"));
            return;
        }
        auto* PC = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
        auto* State = IsValid(PC) ? PC->GetPlayerState<AShadePlayerState>() : nullptr;
        if (!IsValid(State) || !ShadeHealthEffects::ApplyHealthChange(State->GetAbilitySystemComponent(), Delta))
        {
            UE_LOG(LogTemp, Display, TEXT("Health change rejected: a live authoritative PlayerState is required."));
            return;
        }
        const auto* Attributes = State->GetAttributeSet();
        UE_LOG(LogTemp, Display, TEXT("Health: %.0f / %.0f"), Attributes->GetHealth(), Attributes->GetMaxHealth());
    }), ECVF_Cheat);
}
#endif
