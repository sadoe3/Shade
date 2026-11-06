#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"
#include "../Player/ShadePlayerState.h"
#include "../Character/ShadePlayerCharacter.h"
#include "../Character/ShadeEnemyCharacter.h"
#include "../AbilitySystem/ShadeAssassinateAbility.h"
#include "../AbilitySystem/ShadeGameplayTags.h"
#include "../AbilitySystem/ShadeEnemyTags.h"
#include "../AbilitySystem/ShadeStealthTags.h"
#include "../AbilitySystem/ShadeAttributeSet.h"
#include "../AbilitySystem/Data/ShadeInitialAttributesData.h"
#include "../AbilitySystem/Data/ShadeAssassinationData.h"
#include "../AbilitySystem/Data/ShadeEnemyBehaviorData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeAssassinationTest, "Shade.Stealth.Assassination",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShadeAssassinationTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("Engine"), GEngine)) return false;
    const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("ShadeAssassinationTest"),nullptr,true,ERHIFeatureLevel::Num,&Values);
    if (!TestNotNull(TEXT("World"),World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
    World->InitializeActorsForPlay(FURL());
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* PC=World->SpawnActor<APlayerController>();
    auto* StateClass=LoadClass<AShadePlayerState>(nullptr,TEXT("/Game/Shade/Player/BP_ShadePlayerState.BP_ShadePlayerState_C"));
    if (!TestNotNull(TEXT("State class"), StateClass)) return false;
    auto* State=World->SpawnActor<AShadePlayerState>(StateClass,Spawn);
    auto* Player=World->SpawnActor<AShadePlayerCharacter>(Spawn);
    if (!TestNotNull(TEXT("Controller"), PC) || !TestNotNull(TEXT("State"), State) ||
        !TestNotNull(TEXT("Player"), Player)) return false;
    PC->SetPlayerState(State);PC->Possess(Player);
    auto* ASC=State->GetAbilitySystemComponent();
    auto* Settings=NewObject<UShadeAssassinationData>(State);
    Settings->Range=220.f;
    if (!ASC->FindAbilitySpecFromClass(UShadeAssassinateAbility::StaticClass()))
        ASC->GiveAbility(FGameplayAbilitySpec(UShadeAssassinateAbility::StaticClass(),1,INDEX_NONE,Settings));
    else
        ASC->FindAbilitySpecFromClass(UShadeAssassinateAbility::StaticClass())->SourceObject=Settings;
    auto* Enemy=World->SpawnActor<AShadeEnemyCharacter>(AShadeEnemyCharacter::StaticClass(),FVector(150,0,0),FRotator::ZeroRotator,Spawn);
    if (!TestNotNull(TEXT("Enemy"), Enemy)) return false;
    auto* EnemySettings=NewObject<UShadeInitialAttributesData>(Enemy);
    EnemySettings->InitialHealth = 100.f;
    EnemySettings->WalkSpeed = 180.f;
    EnemySettings->RunSpeed = 300.f;
    EnemySettings->BrakingDecelerationWalking = 1800.f;
    auto* EnemyDataProperty = FindFProperty<FObjectPropertyBase>(Enemy->GetClass(), TEXT("InitialAttributesData"));
    if (!TestNotNull(TEXT("Enemy settings property"), EnemyDataProperty)) return false;
    EnemyDataProperty->SetObjectPropertyValue_InContainer(Enemy,EnemySettings);
    auto* BehaviorProperty = FindFProperty<FObjectPropertyBase>(Enemy->GetClass(), TEXT("BehaviorData"));
    if (!TestNotNull(TEXT("Enemy behavior data property"), BehaviorProperty)) return false;
    BehaviorProperty->SetObjectPropertyValue_InContainer(Enemy, NewObject<UShadeEnemyBehaviorData>(Enemy));
    Enemy->InitializeAbilitySystem();
    auto* Victim=Enemy->GetAbilitySystemComponent();
    auto Request=[&]()
    {
        FGameplayEventData Payload;Payload.Instigator=Player;Payload.Target=Enemy;
        return ASC->HandleGameplayEvent(ShadeStealthTags::Event_Assassinate,&Payload);
    };
    TestFalse(TEXT("Player cannot be an assassination target"), UShadeAssassinateAbility::IsValidTarget(Player, Player, Settings->Range));
    Settings->Range = 0.f;
    TestEqual(TEXT("Invalid authored range rejects event"), Request(), 0);
    Settings->Range = 220.f;
    Victim->AddLooseGameplayTag(ShadeStealthTags::State_Action_BeingAssassinated);
    TestEqual(TEXT("Already claimed target rejects event"), Request(), 0);
    TestEqual(TEXT("Rejected request preserves someone else's claim"),
        Victim->GetTagCount(ShadeStealthTags::State_Action_BeingAssassinated), 1);
    Victim->RemoveLooseGameplayTag(ShadeStealthTags::State_Action_BeingAssassinated);
    Enemy->SetActorLocation(FVector(1000,0,0));
    TestEqual(TEXT("Far target rejects event"),Request(),0);
    Enemy->SetActorLocation(FVector(150,0,0));
    Victim->AddLooseGameplayTag(ShadeEnemyTags::State_Awareness_Alert);
    TestEqual(TEXT("Alert blocks assassination"),Request(),0);
    TestEqual(TEXT("Rejected action preserves Health"),Victim->GetNumericAttribute(UShadeAttributeSet::GetHealthAttribute()),100.f);
    Victim->RemoveLooseGameplayTag(ShadeEnemyTags::State_Awareness_Alert);
    auto* Wall=World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Occluding wall"), Wall)) return false;
    auto* Box=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(10,100,100));Box->SetCollisionProfileName(TEXT("BlockAll"));Box->RegisterComponent();
    Wall->SetActorLocation(FVector(75,0,0));
    TestEqual(TEXT("Wall blocks execution"),Request(),0);
    Wall->Destroy();
    ASC->AddLooseGameplayTag(ShadeGameplayTags::State_Dead);
    TestEqual(TEXT("Dead attacker rejected"),Request(),0);
    ASC->RemoveLooseGameplayTag(ShadeGameplayTags::State_Dead);
    Victim->AddLooseGameplayTag(ShadeEnemyTags::State_Awareness_Suspicious);
    TestEqual(TEXT("Suspicious target permits immediate ability"),Request(),1);
    TestEqual(TEXT("Assassination GE sets Health to zero"),Victim->GetNumericAttribute(UShadeAttributeSet::GetHealthAttribute()),0.f);
    TestTrue(TEXT("Common death state applied"),Victim->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead));
    TestEqual(TEXT("Dead enemy movement disabled"),static_cast<EMovementMode>(Enemy->GetCharacterMovement()->MovementMode),MOVE_None);
    TestFalse(TEXT("Attacker action state cleaned"),ASC->HasMatchingGameplayTag(ShadeStealthTags::State_Action_Assassinating));
    TestFalse(TEXT("Victim claim cleaned"),Victim->HasMatchingGameplayTag(ShadeStealthTags::State_Action_BeingAssassinated));
    const FGameplayAbilitySpec* FinishedAbility = ASC->FindAbilitySpecFromClass(UShadeAssassinateAbility::StaticClass());
    if (TestNotNull(TEXT("Granted ability after use"), FinishedAbility))
        TestFalse(TEXT("Immediate ability ends after applying death"), FinishedAbility->IsActive());
    TestEqual(TEXT("Already dead target rejects second request"),Request(),0);
    return true;
}
#endif
