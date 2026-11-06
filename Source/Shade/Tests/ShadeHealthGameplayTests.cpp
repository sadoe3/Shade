#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "../Player/ShadePlayerController.h"
#include "../Player/ShadePlayerState.h"
#include "../Character/ShadePlayerCharacter.h"
#include "../AbilitySystem/ShadeAttributeSet.h"
#include "../AbilitySystem/ShadeHealthEffects.h"
#include "../AbilitySystem/ShadeGameplayTags.h"
#include "../UI/ShadeHUD.h"
#include "../UI/ShadeHealthWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeHealthGameplayTest, "Shade.Health.GameplayPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeHealthGameplayTest::RunTest(const FString& Parameters)
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
        if (!TestNotNull(TEXT("PlayerController"), PC)) return true;
        auto* State = PC->GetPlayerState<AShadePlayerState>();
        auto* Body = Cast<AShadePlayerCharacter>(PC->GetPawn());
        auto* HUD = PC->GetHUD<AShadeHUD>();
        if (!TestNotNull(TEXT("PlayerState"), State) || !TestNotNull(TEXT("Body"), Body) ||
            !TestNotNull(TEXT("HUD"), HUD)) return true;
        auto* ASC = State->GetAbilitySystemComponent();
        auto* Attributes = State->GetAttributeSet();
        auto* WidgetProperty = FindFProperty<FObjectPropertyBase>(HUD->GetClass(), TEXT("HealthWidget"));
        if (!TestNotNull(TEXT("ASC"), ASC) || !TestNotNull(TEXT("Attributes"), Attributes) ||
            !TestNotNull(TEXT("Widget property"), WidgetProperty)) return true;
        auto* Widget = Cast<UShadeHealthWidget>(WidgetProperty->GetObjectPropertyValue_InContainer(HUD));
        if (!TestNotNull(TEXT("Widget"), Widget) || !TestNotNull(TEXT("Widget tree"), Widget->WidgetTree.Get())) return true;
        auto* Bar = Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("HealthBar")));
        auto* Text = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("HealthText")));
        if (!TestNotNull(TEXT("Health bar"), Bar) || !TestNotNull(TEXT("Health text"), Text)) return true;
        TestFalse(TEXT("Initial zero before possession did not mark body dead"), Body->IsDead());
        TestTrue(TEXT("Initialized body can move"), Body->GetCharacterMovement()->MovementMode != MOVE_None);

        // Exercise the development command's actual console routing through the same signed GE API.
        PC->ConsoleCommand(TEXT("Shade.Health -25"), true);
        TestEqual(TEXT("Console damage reaches attributes"), Attributes->GetHealth(), 75.f);
        TestEqual(TEXT("GE damage reaches UI"), Text->GetText().ToString(), FString(TEXT("75 / 100")));
        TestEqual(TEXT("GE damage reaches progress bar"), Bar->GetPercent(), 0.75f);
        TestTrue(TEXT("Living target can heal"), ShadeHealthEffects::ApplyHealthChange(ASC, 10.f));
        TestEqual(TEXT("Healing reaches UI"), Text->GetText().ToString(), FString(TEXT("85 / 100")));
        ShadeHealthEffects::ApplyHealthChange(ASC, 1000.f);
        TestEqual(TEXT("Overheal UI remains bounded"), Bar->GetPercent(), 1.f);

        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* NextBody = World->SpawnActor<AShadePlayerCharacter>(Body->GetClass(), Spawn);
        if (!TestNotNull(TEXT("Next body"), NextBody)) return true;
        PC->Possess(NextBody);
        TestNull(TEXT("Previous body removes its ASC reference"), Body->GetAbilitySystemComponent());
        TestTrue(TEXT("Next body reuses the same ASC"), NextBody->GetAbilitySystemComponent() == ASC);

        // The engine's native base ability stays active until explicitly ended, so death can cancel it.
        const FGameplayAbilitySpecHandle AbilityHandle = ASC->GiveAbility(FGameplayAbilitySpec(UGameplayAbility::StaticClass()));
        TestTrue(TEXT("Test ability activates"), ASC->TryActivateAbility(AbilityHandle));
        auto* AbilitySpec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
        if (!TestNotNull(TEXT("Granted ability"), AbilitySpec)) return true;
        TestTrue(TEXT("Ability is active before death"), AbilitySpec->IsActive());
        NextBody->GetCharacterMovement()->Velocity = FVector(100.f, 0.f, 0.f);
        NextBody->AddMovementInput(FVector::ForwardVector);
        TestTrue(TEXT("Lethal damage applies"), ShadeHealthEffects::ApplyHealthChange(ASC, -1000.f));
        TestEqual(TEXT("Death displays zero"), Text->GetText().ToString(), FString(TEXT("0 / 100")));
        TestEqual(TEXT("Death empties progress bar"), Bar->GetPercent(), 0.f);
        TestTrue(TEXT("Dead tag reaches Character"), NextBody->IsDead());
        TestEqual(TEXT("Death disables movement"), static_cast<EMovementMode>(NextBody->GetCharacterMovement()->MovementMode), MOVE_None);
        TestTrue(TEXT("Death clears velocity"), NextBody->GetVelocity().IsNearlyZero());
        TestTrue(TEXT("Death clears queued movement"), NextBody->GetPendingMovementInputVector().IsNearlyZero());
        TestFalse(TEXT("Death cancels the active ability"), AbilitySpec->IsActive());
        TestTrue(TEXT("Detached old body is no longer subscribed to death"), Body->GetCharacterMovement()->MovementMode != MOVE_None);
        TestEqual(TEXT("Exactly one dead tag grant"), ASC->GetTagCount(ShadeGameplayTags::State_Dead), 1);
        const auto DeadEffects = ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(
            FGameplayTagContainer(ShadeGameplayTags::State_Dead)));
        TestEqual(TEXT("Dead is carried by a persistent GE"), DeadEffects.Num(), 1);
        if (DeadEffects.Num() == 1)
        {
            const FActiveGameplayEffect* DeadEffect = ASC->GetActiveGameplayEffect(DeadEffects[0]);
            if (TestNotNull(TEXT("Active death effect"), DeadEffect))
                TestEqual(TEXT("Death effect is infinite"), DeadEffect->Spec.GetDuration(), UGameplayEffect::INFINITE_DURATION);
        }
        TestFalse(TEXT("Dead target cannot heal through helper"), ShadeHealthEffects::ApplyHealthChange(ASC, 50.f));
        FGameplayEffectSpecHandle HealSpec = ASC->MakeOutgoingSpec(UShadeHealthChangeEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
        if (TestTrue(TEXT("Direct heal spec"), HealSpec.IsValid()))
        {
            HealSpec.Data->SetSetByCallerMagnitude(ShadeGameplayTags::Data_Health_Change, 50.f);
            ASC->ApplyGameplayEffectSpecToSelf(*HealSpec.Data.Get());
        }
        TestEqual(TEXT("AttributeSet also rejects bypassed healing"), Attributes->GetHealth(), 0.f);
        TestEqual(TEXT("Rejected healing cannot update UI"), Text->GetText().ToString(), FString(TEXT("0 / 100")));
        TestEqual(TEXT("Rejected effects cannot stack death"), ASC->GetTagCount(ShadeGameplayTags::State_Dead), 1);

        PC->UnPossess();
        PC->Possess(NextBody);
        TestTrue(TEXT("Repossess keeps the same dead ASC"), NextBody->GetAbilitySystemComponent() == ASC && NextBody->IsDead());
        TestEqual(TEXT("Repossess does not restore Health"), Attributes->GetHealth(), 0.f);
        TestEqual(TEXT("Repossess does not restore movement"), static_cast<EMovementMode>(NextBody->GetCharacterMovement()->MovementMode), MOVE_None);
        auto* Replacement = World->SpawnActor<AShadePlayerCharacter>(Body->GetClass(), Spawn);
        if (TestNotNull(TEXT("Replacement body"), Replacement))
        {
            PC->Possess(Replacement);
            TestTrue(TEXT("New body receives existing dead tag"), Replacement->IsDead());
            TestEqual(TEXT("Existing death disables the replacement body"), static_cast<EMovementMode>(Replacement->GetCharacterMovement()->MovementMode),
                MOVE_None);
            TestEqual(TEXT("New body still does not restore Health"), Attributes->GetHealth(), 0.f);
        }
        ASC->ClearAbility(AbilityHandle);
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
