#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "../Player/ShadePlayerState.h"
#include "../Character/ShadePlayerCharacter.h"
#include "../Stealth/ShadeBushVolume.h"
#include "../AbilitySystem/ShadeGameplayTags.h"

namespace
{
struct FScopedBushTestWorld
{
    UWorld* World = nullptr;
    ~FScopedBushTestWorld()
    {
        if (World)
        {
            World->EndPlay(EEndPlayReason::Quit);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeBushTest, "Shade.Stealth.BushOverlap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeBushTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("Engine"), GEngine)) return false;
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false)
        .ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ShadeBushTest"),
        nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("World"), World)) return false;
    auto Lifetime = MakeShared<FScopedBushTestWorld>();
    Lifetime->World = World;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    // The isolated world has no GameMode; explicitly enter play so overlap notifications are enabled.
    World->SetBegunPlay(true);
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* PC = World->SpawnActor<APlayerController>();
    auto* StateClass = LoadClass<AShadePlayerState>(nullptr,
        TEXT("/Game/Shade/Player/BP_ShadePlayerState.BP_ShadePlayerState_C"));
    if (!TestNotNull(TEXT("PlayerState class"), StateClass)) return false;
    auto* State = World->SpawnActor<AShadePlayerState>(StateClass, Spawn);
    auto* Player = World->SpawnActor<AShadePlayerCharacter>(Spawn);
    if (!TestNotNull(TEXT("Controller"), PC) || !TestNotNull(TEXT("State"), State) ||
        !TestNotNull(TEXT("Player"), Player)) return false;
    PC->SetPlayerState(State);
    PC->Possess(Player);
    Player->SetActorLocation(FVector(1000, 0, 0));
    auto* A = World->SpawnActor<AShadeBushVolume>(Spawn);
    auto* B = World->SpawnActor<AShadeBushVolume>(Spawn);
    if (!TestNotNull(TEXT("First bush"), A) || !TestNotNull(TEXT("Second bush"), B)) return false;
    Player->GetCapsuleComponent()->UpdateOverlaps();
    Player->SetActorLocation(FVector::ZeroVector);
    Player->GetCapsuleComponent()->UpdateOverlaps();
    auto* ASC = State->GetAbilitySystemComponent();
    TestEqual(TEXT("Two overlapping bushes each own a grant"), ASC->GetTagCount(ShadeGameplayTags::State_Stealth_InBush), 2);
    A->Destroy();
    TestEqual(TEXT("Removing one bush preserves the other grant"), ASC->GetTagCount(ShadeGameplayTags::State_Stealth_InBush), 1);
    Player->SetActorLocation(FVector(1000, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
    Player->GetCapsuleComponent()->UpdateOverlaps();
    TestEqual(TEXT("Leaving all bushes removes concealment immediately"), ASC->GetTagCount(ShadeGameplayTags::State_Stealth_InBush), 0);

    Player->SetActorLocation(FVector::ZeroVector);
    Player->GetCapsuleComponent()->UpdateOverlaps();
    TestEqual(TEXT("Re-entering grants exactly one effect"), ASC->GetTagCount(ShadeGameplayTags::State_Stealth_InBush), 1);
    B->Destroy();
    TestEqual(TEXT("Bush EndPlay removes its remaining grant"), ASC->GetTagCount(ShadeGameplayTags::State_Stealth_InBush), 0);
    return true;
}
#endif
