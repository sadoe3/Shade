#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "AbilitySystemComponent.h"
#include "EnhancedPlayerInput.h"
#include "InputKeyEventArgs.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/SkeletalMeshComponent.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"
#include "../Character/ShadeEnemyCharacter.h"
#include "../Player/ShadePlayerController.h"
#include "../Player/ShadePlayerState.h"
#include "../AbilitySystem/ShadeAssassinateAbility.h"
#include "../AbilitySystem/ShadeAttributeSet.h"
#include "../AbilitySystem/ShadeGameplayTags.h"
#include "../AbilitySystem/ShadeHealthEffects.h"
#include "../UI/ShadeHUD.h"
#include "../UI/ShadeHealthWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeStandaloneSmoke, "Shade.Runtime.StealthSmoke",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FShadeStandaloneSmoke::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("Engine context owner"), GEngine)) return false;
    const double Deadline = FPlatformTime::Seconds() + 30.0;
    const bool bCapture = FParse::Param(FCommandLine::Get(), TEXT("ShadeCaptureSmoke"));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Deadline, bCapture]()
    {
        UWorld* World=nullptr;
        for (const FWorldContext& Context:GEngine->GetWorldContexts())
            if (Context.WorldType==EWorldType::Game && Context.World()) {World=Context.World();break;}
        if (!TestNotNull(TEXT("Standalone Game world (not PIE)"),World)) return true;
        TestEqual(TEXT("Standalone net mode"),World->GetNetMode(),NM_Standalone);
        TestTrue(TEXT("Saved prototype map loaded"),World->GetName().Contains(TEXT("L_StealthPrototype")));
        auto* PC=Cast<AShadePlayerController>(World->GetFirstPlayerController());
        if (!TestNotNull(TEXT("Local player controller"),PC)) return true;
        APawn* const Body = PC->GetPawn();
        auto* PS=PC->GetPlayerState<AShadePlayerState>();
        if (!TestNotNull(TEXT("PlayerState"),PS)||!TestNotNull(TEXT("Player pawn"),Body)) return true;
        auto* ASC=PS->GetAbilitySystemComponent();
        if (!TestNotNull(TEXT("Player ASC"), ASC)) return true;
        TestEqual(TEXT("Player initial health"),ASC->GetNumericAttribute(UShadeAttributeSet::GetHealthAttribute()),100.f);
        if (!TestNotNull(TEXT("Assassination ability granted"),
            ASC->FindAbilitySpecFromClass(UShadeAssassinateAbility::StaticClass()))) return true;
        auto* HUD=PC->GetHUD<AShadeHUD>();
        if (!TestNotNull(TEXT("HUD"),HUD)) return true;
        auto* Property=FindFProperty<FObjectPropertyBase>(HUD->GetClass(),TEXT("HealthWidget"));
        if (!TestNotNull(TEXT("HUD widget property"), Property)) return true;
        auto* Widget = Cast<UShadeHealthWidget>(Property->GetObjectPropertyValue_InContainer(HUD));
        if (!TestNotNull(TEXT("HUD initialized the screen widget"),Widget)) return true;
        TestTrue(TEXT("Widget on screen"),Widget->IsInViewport());
        if (!TestNotNull(TEXT("Health widget tree"), Widget->WidgetTree.Get())) return true;
        auto* Text=Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("HealthText")));
        if (!TestNotNull(TEXT("Health text"),Text)) return true;
        TestEqual(TEXT("Standalone initial UI"),Text->GetText().ToString(),FString(TEXT("100 / 100")));
        TArray<AShadeEnemyCharacter*> Enemies;
        for(TActorIterator<AShadeEnemyCharacter> It(World);It;++It) Enemies.Add(*It);
        if (!TestEqual(TEXT("Three configured enemies"),Enemies.Num(),3)) return true;
        bool bNavigationReady = true;
        for (auto* Candidate : Enemies)
        {
            auto* Probe = UNavigationSystemV1::FindPathToLocationSynchronously(World, Candidate->GetActorLocation(), Body->GetActorLocation(), Candidate);
            bNavigationReady &= Probe && Probe->IsValid() && !Probe->IsPartial();
        }
        if (!bNavigationReady && FPlatformTime::Seconds() < Deadline) return false;
        for (auto* Enemy:Enemies)
        {
            auto* AI=Cast<AAIController>(Enemy->GetController());
            TestTrue(TEXT("Enemy has running behavior tree"),AI && AI->GetBrainComponent() && AI->GetBrainComponent()->IsRunning());
            auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(World,Enemy->GetActorLocation(),Body->GetActorLocation(),Enemy);
            TestTrue(TEXT("Enemy can navigate to player start"),Path && Path->IsValid() && !Path->IsPartial());
            auto* Mesh = Enemy->GetMesh();
            if (!TestNotNull(TEXT("Enemy skeletal mesh component"), Mesh)) return true;
            TestNotNull(TEXT("Enemy animation instance"), Mesh->GetAnimInstance());
        }
        auto* Enemy = Enemies[0];
        auto* VictimASC = Enemy->GetAbilitySystemComponent();
        auto* EnemyMovement = Enemy->GetCharacterMovement();
        if (!TestNotNull(TEXT("Target Enemy ASC"), VictimASC) ||
            !TestNotNull(TEXT("Target Enemy movement"), EnemyMovement)) return true;
        if (!TestTrue(TEXT("Player reaches the assassination test position"),
            Body->SetActorLocation(Enemy->GetActorLocation() - Enemy->GetActorForwardVector() * 150.f,
                false, nullptr, ETeleportType::TeleportPhysics))) return true;
        auto* Input=Cast<UEnhancedPlayerInput>(PC->PlayerInput);
        if (!TestNotNull(TEXT("Enhanced input"), Input) ||
            !TestNotNull(TEXT("Controller input component"), PC->InputComponent.Get())) return true;
        const TArray<UInputComponent*> Stack{PC->InputComponent};
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1.f));
        Input->ProcessInputStack(Stack,1.f/60.f,false);
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0.f));
        Input->ProcessInputStack(Stack,1.f/60.f,false);
        TestTrue(TEXT("E executes a valid assassination"),VictimASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead));
        TestEqual(TEXT("Enemy death stops movement"),static_cast<EMovementMode>(EnemyMovement->MovementMode),MOVE_None);
        TestTrue(TEXT("Standalone health GE applies"), ShadeHealthEffects::ApplyHealthChange(ASC, -25.f));
        TestEqual(TEXT("Standalone GE updates HUD"),Text->GetText().ToString(),FString(TEXT("75 / 100")));
        if (bCapture) PC->SetControlRotation((Enemy->GetActorLocation() - Body->GetActorLocation()).Rotation());
        return true;
    }));
    if (bCapture)
    {
        ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
        ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]()
        {
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),
                TEXT("PendingWorks/Standalone/Smoke.png")), true, false);
            return true;
        }));
        ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
    }
    return true;
}
#endif
