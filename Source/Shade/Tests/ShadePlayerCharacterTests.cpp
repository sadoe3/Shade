#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemComponent.h"
#include "../AbilitySystem/ShadeAttributeSet.h"
#include "../Character/ShadePlayerCharacter.h"
#include "../Player/ShadePlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadePlayerCharacterConnectionTest,
    "Shade.Player.CharacterConnection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadePlayerCharacterConnectionTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine context owner"), GEngine)) return false;
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false)
        .ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ShadeCharacterConnectionTestWorld"),
        nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    World->InitializeActorsForPlay(FURL());

    UClass* StateClass = LoadClass<AShadePlayerState>(nullptr,
        TEXT("/Game/Shade/Player/BP_ShadePlayerState.BP_ShadePlayerState_C"));
    if (!TestNotNull(TEXT("Configured PlayerState class"), StateClass)) return false;
    auto* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("PlayerController"), Controller)) return false;
    FActorSpawnParameters StateSpawn;
    StateSpawn.Owner = Controller;
    auto* State = World->SpawnActor<AShadePlayerState>(StateClass, StateSpawn);
    if (!TestNotNull(TEXT("PlayerState"), State)) return false;
    Controller->SetPlayerState(State);

    FActorSpawnParameters BodySpawn;
    BodySpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* FirstBody = World->SpawnActor<AShadePlayerCharacter>(BodySpawn);
    auto* NextBody = World->SpawnActor<AShadePlayerCharacter>(BodySpawn);
    if (!TestNotNull(TEXT("First Character"), FirstBody) || !TestNotNull(TEXT("Next Character"), NextBody)) return false;
    FirstBody->DispatchBeginPlay();
    NextBody->DispatchBeginPlay();

    auto* ASC = State->GetAbilitySystemComponent();
    TestNull(TEXT("Unpossessed Character has no ASC reference"), FirstBody->GetAbilitySystemComponent());
    TestNull(TEXT("Character does not own an extra ASC"), FirstBody->FindComponentByClass<UAbilitySystemComponent>());

    Controller->Possess(FirstBody);
    TestEqual(TEXT("Possession supplies PlayerState"), FirstBody->GetPlayerState<AShadePlayerState>(), State);
    TestEqual(TEXT("Character uses PlayerState ASC"), FirstBody->GetAbilitySystemComponent(), ASC);
    TestEqual(TEXT("ASC owner is PlayerState"), ASC->GetOwnerActor(), static_cast<AActor*>(State));
    TestEqual(TEXT("ASC Avatar is possessed Character"), ASC->GetAvatarActor(), static_cast<AActor*>(FirstBody));
    TestEqual(TEXT("Possession applies initial Health"), State->GetAttributeSet()->GetHealth(), 100.f);

    ASC->SetNumericAttributeBase(UShadeAttributeSet::GetHealthAttribute(), 25.f);
    Controller->Possess(NextBody);
    TestNull(TEXT("Old body releases ASC reference"), FirstBody->GetAbilitySystemComponent());
    TestEqual(TEXT("New body uses same ASC"), NextBody->GetAbilitySystemComponent(), ASC);
    TestEqual(TEXT("ASC follows new body"), ASC->GetAvatarActor(), static_cast<AActor*>(NextBody));
    TestEqual(TEXT("Body change preserves Health"), State->GetAttributeSet()->GetHealth(), 25.f);
    FirstBody->Destroy();
    TestEqual(TEXT("Old body destruction preserves current Avatar"), ASC->GetAvatarActor(), static_cast<AActor*>(NextBody));

    Controller->UnPossess();
    TestNull(TEXT("Unpossess releases Character reference"), NextBody->GetAbilitySystemComponent());
    TestNull(TEXT("Unpossess clears current Avatar"), ASC->GetAvatarActor());
    TestEqual(TEXT("Unpossess preserves Health"), State->GetAttributeSet()->GetHealth(), 25.f);
    Controller->Possess(NextBody);
    TestEqual(TEXT("Repossess restores ASC owner"), ASC->GetOwnerActor(), static_cast<AActor*>(State));
    TestEqual(TEXT("Repossess preserves Health"), State->GetAttributeSet()->GetHealth(), 25.f);
    NextBody->Destroy();
    TestNull(TEXT("Character EndPlay clears its Avatar"), ASC->GetAvatarActor());
    TestTrue(TEXT("PlayerState ASC survives body destruction"), IsValid(ASC));
    return true;
}

#endif
