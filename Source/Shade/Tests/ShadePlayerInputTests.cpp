#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "../Character/ShadePlayerCharacter.h"
#include "../Player/ShadePlayerController.h"
#include "../Player/ShadePlayerState.h"
#include "../AbilitySystem/ShadeAttributeSet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadePlayerInputTest,
    "Shade.Player.InputConnectionPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadePlayerInputTest::RunTest(const FString& Parameters)
{
    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!TestTrue(TEXT("Open the isolated L_InputConnection verification map before running this test"),
        EditorWorld && EditorWorld->GetName().Contains(TEXT("L_InputConnection")))) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]()
    {
        UWorld* World = nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && Context.World() &&
                Context.World()->GetName().Contains(TEXT("L_InputConnection")))
            {
                World = Context.World();
                break;
            }
        }
        if (!TestNotNull(TEXT("PIE running in the isolated L_InputConnection verification map"), World)) return true;
        auto* PC = Cast<AShadePlayerController>(World->GetFirstPlayerController());
        if (!TestNotNull(TEXT("GameMode spawned configured PlayerController"), PC)) return true;
        auto* Body = Cast<AShadePlayerCharacter>(PC->GetPawn());
        auto* Input = Cast<UEnhancedPlayerInput>(PC->PlayerInput);
        auto* LocalPlayer = PC->GetLocalPlayer();
        auto* State = PC->GetPlayerState<AShadePlayerState>();
        if (!TestNotNull(TEXT("Possessed Character"), Body) || !TestNotNull(TEXT("EnhancedPlayerInput"), Input) ||
            !TestNotNull(TEXT("LocalPlayer"), LocalPlayer) || !TestNotNull(TEXT("GAS PlayerState"), State)) return true;
        auto* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
        auto* Mapping = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Shade/Input/IMC_Player.IMC_Player"));
        auto* Move = LoadObject<UInputAction>(nullptr, TEXT("/Game/Shade/Input/IA_Move.IA_Move"));
        if (!TestNotNull(TEXT("Input subsystem"), Subsystem) || !TestNotNull(TEXT("Saved mapping"), Mapping) ||
            !TestNotNull(TEXT("Saved move action"), Move)) return true;
        TestTrue(TEXT("BeginPlay registered the mapping"), Subsystem->HasMappingContext(Mapping));
        TestEqual(TEXT("GameMode PlayerState initialized Health"), State->GetAttributeSet()->GetHealth(), 100.f);

        const FVector OriginalLocation = Body->GetActorLocation();
        const FRotator OriginalRotation = PC->GetControlRotation();
        ON_SCOPE_EXIT
        {
            Input->FlushPressedKeys();
            Body->ConsumeMovementInputVector();
            Body->GetCharacterMovement()->StopMovementImmediately();
            Body->SetActorLocation(OriginalLocation);
            PC->SetControlRotation(OriginalRotation);
        };
        const TArray<UInputComponent*> Stack{PC->InputComponent};
        auto Process = [&]() { Input->ProcessInputStack(Stack, 1.f / 60.f, false); };
        auto Key = [&](FKey KeyName, EInputEvent Event, float Amount)
        {
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(KeyName, Event, Amount));
        };
        auto MoveCase = [&](const TCHAR* Label, TArray<FKey> Keys, float Yaw, FVector Expected)
        {
            Input->FlushPressedKeys();
            Body->ConsumeMovementInputVector();
            PC->SetControlRotation(FRotator(0.f, Yaw, 0.f));
            for (FKey K : Keys) Key(K, IE_Pressed, 1.f);
            Process();
            TestTrue(Label, Body->ConsumeMovementInputVector().Equals(Expected, 0.001f));
            for (FKey K : Keys) Key(K, IE_Released, 0.f);
            Process();
        };
        MoveCase(TEXT("W maps input Y to forward world X"), {EKeys::W}, 0.f, FVector(1,0,0));
        MoveCase(TEXT("A maps negative input X to world left"), {EKeys::A}, 0.f, FVector(0,-1,0));
        MoveCase(TEXT("S maps negative input Y to world backward"), {EKeys::S}, 0.f, FVector(-1,0,0));
        MoveCase(TEXT("D maps input X to world right"), {EKeys::D}, 0.f, FVector(0,1,0));
        MoveCase(TEXT("A+D cancels"), {EKeys::A,EKeys::D}, 0.f, FVector::ZeroVector);
        MoveCase(TEXT("W+S cancels"), {EKeys::W,EKeys::S}, 0.f, FVector::ZeroVector);
        MoveCase(TEXT("W+D preserves both input axes"), {EKeys::W,EKeys::D}, 0.f, FVector(1,1,0));
        MoveCase(TEXT("Camera yaw rotates forward movement"), {EKeys::W}, 90.f, FVector(0,1,0));
        TestTrue(TEXT("Released keys leave zero Action value"), Input->GetActionValue(Move).IsNonZero() == false);

        PC->SetControlRotation(FRotator::ZeroRotator);
        Body->GetCharacterMovement()->StopMovementImmediately();
        Key(EKeys::W, IE_Pressed, 1.f);
        Process();
        const FVector BeforeMove = Body->GetActorLocation();
        Body->GetCharacterMovement()->TickComponent(0.1f, LEVELTICK_All, nullptr);
        TestTrue(TEXT("CharacterMovement moves the possessed body"), Body->GetActorLocation().X > BeforeMove.X);
        Key(EKeys::W, IE_Released, 0.f);
        Process();

        PC->SetControlRotation(FRotator::ZeroRotator);
        Key(EKeys::MouseX, IE_Axis, 10.f);
        Key(EKeys::MouseY, IE_Axis, 10.f);
        Process();
        PC->UpdateRotation(1.f / 60.f);
        const FRotator LookRotation = PC->GetControlRotation().GetNormalized();
        TestTrue(TEXT("MouseX changes ControlRotation yaw"), LookRotation.Yaw > 0.f);
        TestTrue(TEXT("MouseY uses existing negative legacy pitch scale"), LookRotation.Pitch < 0.f);
        AddInfo(FString::Printf(TEXT("Verified Look yaw=%f pitch=%f; simulated keys entered through PlayerController::InputKey."), LookRotation.Yaw, LookRotation.Pitch));
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
