#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "UObject/UnrealType.h"
#include "../AbilitySystem/Data/ShadeInitialAttributesData.h"
#include "../AbilitySystem/ShadeAttributeSet.h"
#include "../Player/ShadePlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadePlayerStateInitializationTest,
    "Shade.Player.StateInitialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadePlayerStateInitializationTest::RunTest(const FString& Parameters)
{
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false)
        .ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ShadePlayerStateTestWorld"),
        nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());

    UClass* PlayerStateClass = LoadClass<AShadePlayerState>(nullptr,
        TEXT("/Game/Shade/Player/BP_ShadePlayerState.BP_ShadePlayerState_C"));
    if (!TestNotNull(TEXT("Configured PlayerState Blueprint reloads"), PlayerStateClass)) return false;
    auto* State = World->SpawnActor<AShadePlayerState>(PlayerStateClass);
    AActor* FirstAvatar = World->SpawnActor<AActor>();
    AActor* NextAvatar = World->SpawnActor<AActor>();
    if (!State || !FirstAvatar || !NextAvatar)
    {
        AddError(TEXT("Could not create test actors"));
        return false;
    }

    auto* ASC = State->GetAbilitySystemComponent();
    auto* Attributes = State->GetAttributeSet();
    if (!TestNotNull(TEXT("ASC exists"), ASC) || !TestNotNull(TEXT("AttributeSet exists"), Attributes)) return false;
    TestEqual(TEXT("AttributeSet registered with ASC"), ASC->GetSet<UShadeAttributeSet>(),
        static_cast<const UShadeAttributeSet*>(Attributes));

    State->InitializeAbilitySystem(nullptr);
    TestEqual(TEXT("No initialization without Avatar"), Attributes->GetHealth(), 0.f);
    State->InitializeAbilitySystem(FirstAvatar);
    TestEqual(TEXT("Owner is PlayerState"), ASC->GetOwnerActor(), static_cast<AActor*>(State));
    TestEqual(TEXT("Avatar is first body"), ASC->GetAvatarActor(), FirstAvatar);
    TestEqual(TEXT("Initial Health GE applied"), Attributes->GetHealth(), 100.f);
    TestEqual(TEXT("Initial MaxHealth GE applied"), Attributes->GetMaxHealth(), 100.f);

    // Arrange changed health without introducing the later damage/healing feature.
    ASC->SetNumericAttributeBase(UShadeAttributeSet::GetHealthAttribute(), 25.f);
    State->InitializeAbilitySystem(FirstAvatar);
    TestEqual(TEXT("Repeated initialization preserves Health"), Attributes->GetHealth(), 25.f);
    State->InitializeAbilitySystem(NextAvatar);
    TestEqual(TEXT("New Avatar is connected"), ASC->GetAvatarActor(), NextAvatar);
    TestEqual(TEXT("Avatar change preserves Health"), Attributes->GetHealth(), 25.f);
    TestEqual(TEXT("Avatar change preserves ASC"), State->GetAbilitySystemComponent(), ASC);
    State->InitializeAbilitySystem(nullptr);
    TestEqual(TEXT("Invalid Avatar preserves current connection"), ASC->GetAvatarActor(), NextAvatar);
    // Use transient settings to verify data changes without modifying the shared asset.
    auto* SettingsProperty = FindFProperty<FObjectPropertyBase>(AShadePlayerState::StaticClass(),
        TEXT("InitialAttributesData"));
    if (!TestNotNull(TEXT("Initial settings property"), SettingsProperty)) return false;
    auto* SharedSettings = Cast<UShadeInitialAttributesData>(SettingsProperty->GetObjectPropertyValue_InContainer(State));
    if (!TestNotNull(TEXT("Blueprint references the saved Data Asset"), SharedSettings)) return false;
    TestEqual(TEXT("Shared asset path"), SharedSettings->GetPathName(),
        FString(TEXT("/Game/Shade/Data/DA_PlayerInitialAttributes.DA_PlayerInitialAttributes")));

    auto* UnconfiguredState = World->SpawnActor<AShadePlayerState>();
    if (!TestNotNull(TEXT("Unconfigured PlayerState"), UnconfiguredState)) return false;
    AddExpectedError(TEXT("InitialAttributesData must be assigned."), EAutomationExpectedErrorFlags::Contains, 1);
    UnconfiguredState->InitializeAbilitySystem(FirstAvatar);
    TestEqual(TEXT("Missing data does not initialize Health"), UnconfiguredState->GetAttributeSet()->GetHealth(), 0.f);

    auto* TestSettings = NewObject<UShadeInitialAttributesData>(UnconfiguredState);
    SettingsProperty->SetObjectPropertyValue_InContainer(UnconfiguredState, TestSettings);
    AddExpectedError(TEXT("InitialHealth must be finite and at least 1."), EAutomationExpectedErrorFlags::Contains, 1);
    UnconfiguredState->InitializeAbilitySystem(FirstAvatar);
    TestEqual(TEXT("Invalid data does not initialize Health"), UnconfiguredState->GetAttributeSet()->GetHealth(), 0.f);

    TestSettings->InitialHealth = 275.f;
    UnconfiguredState->InitializeAbilitySystem(FirstAvatar);
    TestEqual(TEXT("Corrected data initializes Health from asset value"), UnconfiguredState->GetAttributeSet()->GetHealth(), 275.f);
    TestEqual(TEXT("Corrected data initializes MaxHealth from asset value"), UnconfiguredState->GetAttributeSet()->GetMaxHealth(), 275.f);
    TestSettings->InitialHealth = 350.f;
    UnconfiguredState->InitializeAbilitySystem(NextAvatar);
    TestEqual(TEXT("Settings edits do not reinitialize existing state"), UnconfiguredState->GetAttributeSet()->GetHealth(), 275.f);
    TestEqual(TEXT("Shared asset is not changed by runtime tests"), SharedSettings->InitialHealth, 100.f);
    return true;
}

#endif
