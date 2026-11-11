#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "../AbilitySystem/Data/ShadeInitialAttributesData.h"
#include "../AbilitySystem/Data/ShadeEnemyBehaviorData.h"
#include "../AbilitySystem/ShadeAttributeSet.h"
#include "../AbilitySystem/ShadeAwarenessSet.h"
#include "../Character/ShadeEnemyCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeEnemyInitializationTest,
    "Shade.Enemy.Initialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeEnemyInitializationTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine context owner"), GEngine)) return false;
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false)
        .ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ShadeEnemyInitializationTestWorld"),
        nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    World->InitializeActorsForPlay(FURL());

    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* First = World->SpawnActor<AShadeEnemyCharacter>(Spawn);
    auto* Second = World->SpawnActor<AShadeEnemyCharacter>(Spawn);
    if (!TestNotNull(TEXT("First Enemy"), First) || !TestNotNull(TEXT("Second Enemy"), Second)) return false;

    auto* FirstASC = First->GetAbilitySystemComponent();
    auto* SecondASC = Second->GetAbilitySystemComponent();
    TestNotEqual(TEXT("Enemies own separate ASC instances"), FirstASC, SecondASC);
    TestNotEqual(TEXT("Enemies own separate attribute instances"), First->GetAttributeSet(), Second->GetAttributeSet());
    TestEqual(TEXT("ASC owner is the Character"), FirstASC->GetOwner(), static_cast<AActor*>(First));
    TestEqual(TEXT("Owned AttributeSet is registered with the ASC"), FirstASC->GetSet<UShadeAttributeSet>(),
        static_cast<const UShadeAttributeSet*>(First->GetAttributeSet()));

    TestNotEqual(TEXT("Enemies own separate awareness sets"), First->GetAwarenessSet(), Second->GetAwarenessSet());
    TestEqual(TEXT("Awareness set is registered with ASC"), FirstASC->GetSet<UShadeAwarenessSet>(),
        static_cast<const UShadeAwarenessSet*>(First->GetAwarenessSet()));
    TestEqual(TEXT("Initial alertness is zero"), First->GetAwarenessSet()->GetAlertness(), 0.f);
    FirstASC->SetNumericAttributeBase(UShadeAwarenessSet::GetMaxAlertnessAttribute(), 100.f);
    FirstASC->SetNumericAttributeBase(UShadeAwarenessSet::GetAlertnessAttribute(), 140.f);
    TestEqual(TEXT("Alertness current respects maximum"), First->GetAwarenessSet()->GetAlertness(), 100.f);
    TestEqual(TEXT("Alertness base respects maximum"), First->GetAwarenessSet()->Alertness.GetBaseValue(), 100.f);
    FirstASC->SetNumericAttributeBase(UShadeAwarenessSet::GetMaxAlertnessAttribute(), 40.f);
    TestEqual(TEXT("Lower maximum clamps current alertness"), First->GetAwarenessSet()->GetAlertness(), 40.f);
    TestEqual(TEXT("Lower maximum clamps base alertness"), First->GetAwarenessSet()->Alertness.GetBaseValue(), 40.f);
    FirstASC->SetNumericAttributeBase(UShadeAwarenessSet::GetMaxAlertnessAttribute(), 80.f);
    TestEqual(TEXT("Higher maximum does not raise alertness"), First->GetAwarenessSet()->GetAlertness(), 40.f);
    FirstASC->SetNumericAttributeBase(UShadeAwarenessSet::GetAlertnessAttribute(), -20.f);
    TestEqual(TEXT("Alertness cannot be negative"), First->GetAwarenessSet()->GetAlertness(), 0.f);
    FirstASC->SetNumericAttributeBase(UShadeAwarenessSet::GetMaxAlertnessAttribute(), 0.f);
    TestEqual(TEXT("Maximum remains positive"), First->GetAwarenessSet()->GetMaxAlertness(), 1.f);
    TestEqual(TEXT("Other Enemy awareness stays independent"), Second->GetAwarenessSet()->GetAlertness(), 0.f);

    auto* SettingsProperty = FindFProperty<FObjectPropertyBase>(AShadeEnemyCharacter::StaticClass(), TEXT("InitialAttributesData"));
    if (!TestNotNull(TEXT("Settings property"), SettingsProperty)) return false;
    auto* Settings = NewObject<UShadeInitialAttributesData>(World);
    Settings->InitialHealth = 150.f;
    Settings->WalkSpeed = 180.f;
    Settings->RunSpeed = 420.f;
    Settings->BrakingDecelerationWalking = 1600.f;
    SettingsProperty->SetObjectPropertyValue_InContainer(First, Settings);
    SettingsProperty->SetObjectPropertyValue_InContainer(Second, Settings);
    auto* BehaviorProperty = FindFProperty<FObjectPropertyBase>(AShadeEnemyCharacter::StaticClass(), TEXT("BehaviorData"));
    if (!TestNotNull(TEXT("Behavior data property"), BehaviorProperty)) return false;
    auto* Behavior = NewObject<UShadeEnemyBehaviorData>(World);
    Behavior->MaxAlertness = 150.f;
    BehaviorProperty->SetObjectPropertyValue_InContainer(First, Behavior);
    BehaviorProperty->SetObjectPropertyValue_InContainer(Second, Behavior);
    First->DispatchBeginPlay();
    Second->DispatchBeginPlay();
    TestEqual(TEXT("Awareness maximum comes from data via GE"), First->GetAwarenessSet()->GetMaxAlertness(), 150.f);
    TestEqual(TEXT("Initial awareness GE resets current alertness"), First->GetAwarenessSet()->GetAlertness(), 0.f);
    FirstASC->SetNumericAttributeBase(UShadeAwarenessSet::GetAlertnessAttribute(), 60.f);
    First->InitializeAbilitySystem();
    TestEqual(TEXT("Repeated initialization preserves live alertness"), First->GetAwarenessSet()->GetAlertness(), 60.f);
    TestEqual(TEXT("Other Enemy alertness remains independent"), Second->GetAwarenessSet()->GetAlertness(), 0.f);

    TestEqual(TEXT("GAS Owner is the Enemy"), FirstASC->GetOwnerActor(), static_cast<AActor*>(First));
    TestEqual(TEXT("GAS Avatar is the same Enemy"), FirstASC->GetAvatarActor(), static_cast<AActor*>(First));
    TestEqual(TEXT("Initial Health comes from settings through GE"), First->GetAttributeSet()->GetHealth(), 150.f);
    TestEqual(TEXT("Initial MaxHealth comes from settings through GE"), First->GetAttributeSet()->GetMaxHealth(), 150.f);
    TestEqual(TEXT("Initial walking speed comes from settings"), First->GetCharacterMovement()->MaxWalkSpeed, 180.f);
    TestEqual(TEXT("Braking comes from settings"), First->GetCharacterMovement()->BrakingDecelerationWalking, 1600.f);
    FirstASC->SetNumericAttributeBase(UShadeAttributeSet::GetHealthAttribute(), 35.f);
    First->InitializeAbilitySystem();
    TestEqual(TEXT("Repeated initialization preserves live Health"), First->GetAttributeSet()->GetHealth(), 35.f);
    TestEqual(TEXT("Other Enemy keeps independent Health"), Second->GetAttributeSet()->GetHealth(), 150.f);
    TestEqual(TEXT("Shared settings are not gameplay state"), Settings->InitialHealth, 150.f);

    auto* Unconfigured = World->SpawnActor<AShadeEnemyCharacter>(Spawn);
    if (!TestNotNull(TEXT("Unconfigured Enemy"), Unconfigured)) return false;
    AddExpectedError(TEXT("Enemy InitialAttributesData must be assigned."), EAutomationExpectedErrorFlags::Contains, 1);
    Unconfigured->InitializeAbilitySystem();
    TestEqual(TEXT("Missing settings leave Health uninitialized"), Unconfigured->GetAttributeSet()->GetHealth(), 0.f);
    SettingsProperty->SetObjectPropertyValue_InContainer(Unconfigured, Settings);
    AddExpectedError(TEXT("Enemy BehaviorData must be assigned."), EAutomationExpectedErrorFlags::Contains, 1);
    Unconfigured->InitializeAbilitySystem();
    Unconfigured->GetAbilitySystemComponent()->SetNumericAttributeBase(UShadeAttributeSet::GetHealthAttribute(), 80.f);
    auto* InvalidBehavior = NewObject<UShadeEnemyBehaviorData>(World);
    InvalidBehavior->MaxAlertness = 0.f;
    BehaviorProperty->SetObjectPropertyValue_InContainer(Unconfigured, InvalidBehavior);
    AddExpectedError(TEXT("Enemy MaxAlertness must be finite and at least 1."), EAutomationExpectedErrorFlags::Contains, 1);
    Unconfigured->InitializeAbilitySystem();
    TestEqual(TEXT("Invalid settings leave awareness placeholder"), Unconfigured->GetAwarenessSet()->GetMaxAlertness(), 1.f);
    BehaviorProperty->SetObjectPropertyValue_InContainer(Unconfigured, Behavior);
    Unconfigured->InitializeAbilitySystem();
    TestEqual(TEXT("Awareness recovery preserves live Health"), Unconfigured->GetAttributeSet()->GetHealth(), 80.f);
    TestEqual(TEXT("Correcting settings permits awareness initialization"), Unconfigured->GetAwarenessSet()->GetMaxAlertness(), 150.f);
    First->Destroy();
    TestNull(TEXT("Destroyed body clears its GAS Avatar connection"), FirstASC->GetAvatarActor());
    TestEqual(TEXT("Destroying one Enemy preserves the other"), Second->GetAttributeSet()->GetHealth(), 150.f);
    return true;
}

#endif
