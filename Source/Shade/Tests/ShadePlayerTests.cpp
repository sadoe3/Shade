#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ShadeHealthSet.h"
#include "AbilitySystem/ShadeGameplayTags.h"
#include "Character/ShadePlayerCharacter.h"
#include "Player/ShadePlayerState.h"
#include "Player/ShadePlayerController.h"
#include "UI/ShadeHealthWidget.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "InputActionValue.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeHealthLifecycleTest, "Shade.Player.HealthLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeHealthLifecycleTest::RunTest(const FString& Parameters)
{
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ShadeHealthTestWorld"),
        nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Isolated test world"), World)) return false;
    World->InitializeActorsForPlay(FURL());
    auto* State = World->SpawnActor<AShadePlayerState>();
    AActor* FirstBody = World->SpawnActor<AActor>();
    AActor* NextBody = World->SpawnActor<AActor>();
    if (!State || !FirstBody || !NextBody)
    {
        AddError(TEXT("Could not create isolated test actors"));
        World->DestroyWorld(false);
        return false;
    }
    State->InitializeAbilitySystem(FirstBody);
    auto* ASC = State->GetAbilitySystemComponent();
    auto* Health = State->GetHealthSet();
    TestEqual(TEXT("Owner is PlayerState"), ASC->GetOwnerActor(), static_cast<AActor*>(State));
    TestEqual(TEXT("Avatar is initial body"), ASC->GetAvatarActor(), FirstBody);
    TestEqual(TEXT("Initial Health GE"), Health->GetHealth(), 100.f);
    TestEqual(TEXT("Initial MaxHealth GE"), Health->GetMaxHealth(), 100.f);
    int32 Notifications = 0;
    const FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(UShadeHealthSet::GetHealthAttribute())
        .AddLambda([&Notifications](const FOnAttributeChangeData&) { ++Notifications; });
    State->ApplyHealthChange(-25.f);
    TestEqual(TEXT("Damage through GE"), Health->GetHealth(), 75.f);
    TestEqual(TEXT("One notification for one damage change"), Notifications, 1);
    State->InitializeAbilitySystem(FirstBody);
    State->InitializeAbilitySystem(NextBody);
    TestEqual(TEXT("Avatar rebind preserves Health"), Health->GetHealth(), 75.f);
    TestEqual(TEXT("Same ASC with new Avatar"), ASC->GetAvatarActor(), NextBody);
    State->ApplyHealthChange(500.f);
    TestEqual(TEXT("Overheal clamp"), Health->GetHealth(), 100.f);
    ASC->SetNumericAttributeBase(UShadeHealthSet::GetMaxHealthAttribute(), 40.f);
    TestEqual(TEXT("Lower MaxHealth clamps current Health"), Health->GetHealth(), 40.f);
    State->ApplyHealthChange(-500.f);
    TestEqual(TEXT("Lethal damage clamp"), Health->GetHealth(), 0.f);
    TestTrue(TEXT("Death effect grants state tag"), ASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead));
    TestFalse(TEXT("Ordinary healing is not implicit revival"), State->ApplyHealthChange(20.f));
    State->InitializeAbilitySystem(FirstBody);
    TestEqual(TEXT("Rebind does not revive or reinitialize"), Health->GetHealth(), 0.f);
    TestEqual(TEXT("Dead tag not duplicated"), ASC->GetTagCount(ShadeGameplayTags::State_Dead), 1);
    ASC->GetGameplayAttributeValueChangeDelegate(UShadeHealthSet::GetHealthAttribute()).Remove(Handle);
    World->DestroyWorld(false);
    return true;
}

class FShadePIEInputCheck : public IAutomationLatentCommand
{
public:
    FShadePIEInputCheck(FAutomationTestBase* InTest, AShadePlayerController* InPC)
        : Test(InTest), PC(InPC), StageStart(FPlatformTime::Seconds())
    {
        if (auto* Character = Cast<AShadePlayerCharacter>(InPC->GetPawn())) OriginalLocation = Character->GetActorLocation();
        OriginalRotation = InPC->GetControlRotation();
        InPC->SetControlRotation(FRotator(0.f, 0.f, 0.f));
    }
    virtual bool Update() override
    {
        auto* Controller = PC.Get();
        auto* Character = Controller ? Cast<AShadePlayerCharacter>(Controller->GetPawn()) : nullptr;
        auto* Input = Controller ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()) : nullptr;
        if (!Character || !Input)
        {
            Test->AddError(TEXT("PIE Player/Input disappeared"));
            return true;
        }
        const double Elapsed = FPlatformTime::Seconds() - StageStart;
        if (Stage < 2)
        {
            Input->InjectInputForAction(Character->GetMoveAction(), FInputActionValue(FVector2D(0.f, 1.f)), {}, {});
            if (Stage == 1) Input->InjectInputForAction(Character->GetRunAction(), FInputActionValue(true), {}, {});
            if (Elapsed < 1.5) return false;
            if (Stage == 0)
            {
                Test->TestTrue(TEXT("Move action produces world movement"), FVector::Dist2D(OriginalLocation, Character->GetActorLocation()) > 50.f);
                Test->TestTrue(TEXT("Walking reaches configured speed"), Character->GetVelocity().Size2D() > Character->GetWalkSpeed() * 0.7f);
            }
            else
            {
                Test->TestEqual(TEXT("Run action changes speed limit"), Character->GetCharacterMovement()->MaxWalkSpeed, Character->GetRunSpeed());
                Test->TestTrue(TEXT("Running moves faster than walking"), Character->GetVelocity().Size2D() > Character->GetWalkSpeed() * 1.2f);
            }
            ++Stage; StageStart = FPlatformTime::Seconds();
            return false;
        }
        if (Stage == 2)
        {
            if (Elapsed < 0.5) return false;
            Test->TestEqual(TEXT("Run release returns to walking"), Character->GetCharacterMovement()->MaxWalkSpeed, Character->GetWalkSpeed());
            Input->InjectInputForAction(Character->GetLookAction(), FInputActionValue(FVector2D(10.f, 0.f)), {}, {});
            ++Stage; StageStart = FPlatformTime::Seconds();
            return false;
        }
        if (Elapsed < 0.3) return false;
        Test->TestTrue(TEXT("Look action changes camera control rotation"), FMath::Abs(Controller->GetControlRotation().Yaw) > 1.f);
        Character->GetCharacterMovement()->StopMovementImmediately();
        Character->SetActorLocation(OriginalLocation, false, nullptr, ETeleportType::TeleportPhysics);
        Controller->SetControlRotation(OriginalRotation);
        return true;
    }
private:
    FAutomationTestBase* Test;
    TWeakObjectPtr<AShadePlayerController> PC;
    FVector OriginalLocation = FVector::ZeroVector;
    FRotator OriginalRotation = FRotator::ZeroRotator;
    int32 Stage = 0;
    double StageStart;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadePIEPlayerTest, "Shade.Player.PIEControlsAndHUD",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadePIEPlayerTest::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    for (const auto& Context : GEngine->GetWorldContexts())
        if (Context.WorldType == EWorldType::PIE) { World = Context.World(); break; }
    if (!TestNotNull(TEXT("Start Player prototype PIE before running this integration test"), World)) return false;
    auto* PC = Cast<AShadePlayerController>(World->GetFirstPlayerController());
    if (!TestNotNull(TEXT("Shade PlayerController"), PC)) return false;
    auto* Character = Cast<AShadePlayerCharacter>(PC->GetPawn());
    auto* State = PC->GetPlayerState<AShadePlayerState>();
    if (!TestNotNull(TEXT("Player Character"), Character) || !TestNotNull(TEXT("PlayerState"), State)) return false;
    TestEqual(TEXT("Character uses PlayerState ASC"), Character->GetAbilitySystemComponent(), State->GetAbilitySystemComponent());
    TestEqual(TEXT("Avatar is possessed Player"), State->GetAbilitySystemComponent()->GetAvatarActor(), static_cast<AActor*>(Character));
    TestNotNull(TEXT("Player skeletal mesh"), Character->GetMesh()->GetSkeletalMeshAsset());
    TestNotNull(TEXT("Player animation instance"), Character->GetMesh()->GetAnimInstance());
    auto* Widget = PC->GetHealthWidget();
    if (!TestNotNull(TEXT("Health HUD"), Widget)) return false;
    const float OriginalHealth = State->GetHealthSet()->GetHealth();
    TestEqual(TEXT("HUD initial snapshot"), Widget->GetDisplayedHealth(), OriginalHealth);
    State->ApplyHealthChange(-20.f);
    TestEqual(TEXT("HUD receives GE damage"), Widget->GetDisplayedHealth(), OriginalHealth - 20.f);
    PC->RefreshHealthHUD();
    State->ApplyHealthChange(20.f);
    TestEqual(TEXT("HUD receives GE heal after rebind"), Widget->GetDisplayedHealth(), OriginalHealth);
    TestEqual(TEXT("HUD MaxHealth"), Widget->GetDisplayedMaxHealth(), State->GetHealthSet()->GetMaxHealth());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FShadePIEInputCheck>(this, PC));
    return true;
}
#endif
