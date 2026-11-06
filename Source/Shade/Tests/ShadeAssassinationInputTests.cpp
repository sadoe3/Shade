#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "AbilitySystemComponent.h"
#include "UObject/UnrealType.h"
#include "../Player/ShadePlayerController.h"
#include "../Player/ShadePlayerState.h"
#include "../Character/ShadePlayerCharacter.h"
#include "../Character/ShadeEnemyCharacter.h"
#include "../AbilitySystem/ShadeAssassinateAbility.h"
#include "../AbilitySystem/ShadeAttributeSet.h"
#include "../AbilitySystem/ShadeGameplayTags.h"
#include "../AbilitySystem/ShadeEnemyTags.h"
#include "../AbilitySystem/Data/ShadeInitialAttributesData.h"
#include "../AbilitySystem/Data/ShadeEnemyBehaviorData.h"
#include "../AbilitySystem/Data/ShadeAssassinationData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeAssassinationInputTest, "Shade.Stealth.AssassinationInputPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeAssassinationInputTest::RunTest(const FString& Parameters)
{
    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!TestTrue(TEXT("Use the existing isolated verification map"),
        EditorWorld && EditorWorld->GetName().Contains(TEXT("L_InputConnection")))) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]()
    {
        UWorld* World = nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && Context.World())
            {
                World = Context.World();
                break;
            }
        }
        if (!TestNotNull(TEXT("PIE world"), World)) return true;
        auto* PC = Cast<AShadePlayerController>(World->GetFirstPlayerController());
        if (!TestNotNull(TEXT("Configured controller"), PC)) return true;
        auto* State = PC->GetPlayerState<AShadePlayerState>();
        auto* Body = Cast<AShadePlayerCharacter>(PC->GetPawn());
        auto* Input = Cast<UEnhancedPlayerInput>(PC->PlayerInput);
        if (!TestNotNull(TEXT("PlayerState"), State) || !TestNotNull(TEXT("Player body"), Body) ||
            !TestNotNull(TEXT("Enhanced input"), Input)) return true;
        const auto* Settings = State->GetAssassinationData();
        if (!TestNotNull(TEXT("Saved assassination settings"), Settings)) return true;
        auto* ASC = State->GetAbilitySystemComponent();
        auto* Spec = ASC->FindAbilitySpecFromClass(UShadeAssassinateAbility::StaticClass());
        if (!TestNotNull(TEXT("PlayerState granted assassination"), Spec)) return true;
        TestTrue(TEXT("Ability reads the authored data asset"), Spec->SourceObject.Get() == Settings);
        const FGameplayAbilitySpecHandle OriginalHandle = Spec->Handle;
        PC->UnPossess();
        PC->Possess(Body);
        int32 GrantedCount = 0;
        for (const FGameplayAbilitySpec& Entry : ASC->GetActivatableAbilities())
            if (Entry.Ability && Entry.Ability->IsA<UShadeAssassinateAbility>()) ++GrantedCount;
        TestEqual(TEXT("Repossess does not duplicate the ability"), GrantedCount, 1);
        TestTrue(TEXT("Repossess keeps the same spec"),
            ASC->FindAbilitySpecFromClass(UShadeAssassinateAbility::StaticClass())->Handle == OriginalHandle);

        if (!TestTrue(TEXT("Authored assassination range is valid"), FMath::IsFinite(Settings->Range) && Settings->Range > 0.f)) return true;
        const FVector NearLocation = Body->GetActorLocation() + FVector(Settings->Range * 0.5f, 0.f, 0.f);
        const FTransform SpawnTransform(FRotator::ZeroRotator, NearLocation);
        auto* Enemy = World->SpawnActorDeferred<AShadeEnemyCharacter>(AShadeEnemyCharacter::StaticClass(),
            SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!TestNotNull(TEXT("Deferred test enemy"), Enemy)) return true;
        Enemy->AutoPossessAI = EAutoPossessAI::Disabled;
        auto* HealthProperty = FindFProperty<FObjectPropertyBase>(Enemy->GetClass(), TEXT("InitialAttributesData"));
        auto* BehaviorProperty = FindFProperty<FObjectPropertyBase>(Enemy->GetClass(), TEXT("BehaviorData"));
        if (!TestNotNull(TEXT("Enemy health settings property"), HealthProperty) ||
            !TestNotNull(TEXT("Enemy behavior settings property"), BehaviorProperty)) return true;
        auto* EnemyHealthData = LoadObject<UShadeInitialAttributesData>(nullptr,
            TEXT("/Game/Shade/Data/DA_EnemyInitialAttributes.DA_EnemyInitialAttributes"));
        if (!TestNotNull(TEXT("Authored Enemy health data"), EnemyHealthData)) return true;
        HealthProperty->SetObjectPropertyValue_InContainer(Enemy, EnemyHealthData);
        BehaviorProperty->SetObjectPropertyValue_InContainer(Enemy, NewObject<UShadeEnemyBehaviorData>(Enemy));
        Enemy->FinishSpawning(SpawnTransform);
        auto* Victim = Enemy->GetAbilitySystemComponent();
        ON_SCOPE_EXIT { Input->FlushPressedKeys(); Enemy->Destroy(); };
        const TArray<UInputComponent*> Stack{PC->InputComponent};
        auto PressAssassinate = [&]()
        {
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E, IE_Pressed, 1.f));
            Input->ProcessInputStack(Stack, 1.f / 60.f, false);
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E, IE_Released, 0.f));
            Input->ProcessInputStack(Stack, 1.f / 60.f, false);
        };
        Enemy->SetActorLocation(Body->GetActorLocation() + FVector(Settings->Range + 100.f, 0.f, 0.f));
        PressAssassinate();
        TestEqual(TEXT("Input does not kill a distant target"), Enemy->GetAttributeSet()->GetHealth(), 100.f);
        Enemy->SetActorLocation(NearLocation);
        Victim->AddLooseGameplayTag(ShadeEnemyTags::State_Awareness_Alert);
        PressAssassinate();
        TestEqual(TEXT("Input does not kill an alert target"), Enemy->GetAttributeSet()->GetHealth(), 100.f);
        Victim->RemoveLooseGameplayTag(ShadeEnemyTags::State_Awareness_Alert);
        PressAssassinate();
        TestEqual(TEXT("E mapping reaches the ability and lethal GE"), Enemy->GetAttributeSet()->GetHealth(), 0.f);
        TestTrue(TEXT("Input execution applies common death"), Enemy->IsDead());
        TestFalse(TEXT("Ability ends after input execution"), ASC->FindAbilitySpecFromHandle(OriginalHandle)->IsActive());
        TestFalse(TEXT("Player remains alive"), Body->IsDead());
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
