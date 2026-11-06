// One-time asset authoring tool. Kept outside Source; not compiled normally.
// To author from scratch, temporarily copy this file to Source/Shade/Tests.
// Add Editor-only dependencies: UnrealEd, AnimGraph, AnimGraphRuntime,
// BlueprintGraph, AssetRegistry. Build ShadeEditor and run the automation test
// Shade.Setup.PendingWorks.EnemyVisual, which refuses existing destinations.
// Then remove the temporary Source copy, restore Build.cs, rebuild ShadeEditor,
// and run verify_enemy_visual.py in a separate Editor process.
// Native Enemy animation uses Engine only; no authoring dependency is permanent.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_Root.h"
#include "AnimationGraphSchema.h"
#include "Factories/AnimBlueprintFactory.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace ShadeEnemyVisualSetup
{
constexpr float LocomotionSpeed = 300.f;
const TCHAR* MeshPath = TEXT("/Game/ParagonMinions/Characters/Minions/Down_Minions/Meshes/Minion_Lane_Melee_Dawn.Minion_Lane_Melee_Dawn");
const TCHAR* SkeletonPath = TEXT("/Game/ParagonMinions/Characters/Minions/Down_Minions/Meshes/Minion_Lane_Core_Skeleton.Minion_Lane_Core_Skeleton");
const TCHAR* IdlePath = TEXT("/Game/ParagonMinions/Characters/Minions/Down_Minions/Animations/Melee/NonCombat_Idle.NonCombat_Idle");
const TCHAR* JogPath = TEXT("/Game/ParagonMinions/Characters/Minions/Down_Minions/Animations/Melee/NonCombat_JogFwd.NonCombat_JogFwd");
const TCHAR* BlendPackage = TEXT("/Game/Shade/Enemy/BS_ShadeEnemyLocomotion");
const TCHAR* AnimPackage = TEXT("/Game/Shade/Enemy/ABP_ShadeEnemy");
const TCHAR* CharacterPackage = TEXT("/Game/Shade/Enemy/BP_ShadeEnemy");
const TCHAR* DataPackage = TEXT("/Game/Shade/Data/DA_EnemyInitialAttributes");
const TCHAR* BehaviorPackage = TEXT("/Game/Shade/Data/DA_EnemyBehavior");

bool SaveNew(UObject* Asset)
{
    FAssetRegistryModule::AssetCreated(Asset);
    Asset->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    return UPackage::SavePackage(Asset->GetOutermost(), Asset,
        *FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}

UEdGraphPin* FirstPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == Direction && !Pin->bHidden) return Pin;
    }
    return nullptr;
}

bool SetFloat(UObject* Target, FName PropertyName, float Value)
{
    auto* Property = FindFProperty<FFloatProperty>(Target->GetClass(), PropertyName);
    if (!Property) return false;
    Property->SetPropertyValue_InContainer(Target, Value);
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeSetupEnemyVisual,
    "Shade.Setup.PendingWorks.EnemyVisual",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeSetupEnemyVisual::RunTest(const FString& Parameters)
{
    using namespace ShadeEnemyVisualSetup;

    // Preserve existing game assets; this authoring test is deliberately one-shot.
    for (const TCHAR* Name : {BlendPackage, AnimPackage, CharacterPackage, DataPackage, BehaviorPackage})
    {
        if (FPackageName::DoesPackageExist(Name) || FindPackage(nullptr, Name))
        {
            AddError(FString::Printf(TEXT("Existing destination is preserved: %s"), Name));
            return false;
        }
    }

    auto* Mesh = LoadObject<USkeletalMesh>(nullptr, MeshPath);
    auto* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath);
    auto* Idle = LoadObject<UAnimSequence>(nullptr, IdlePath);
    auto* Jog = LoadObject<UAnimSequence>(nullptr, JogPath);
    UClass* AnimParent = LoadClass<UAnimInstance>(nullptr, TEXT("/Script/Shade.ShadeEnemyAnimInstance"));
    UClass* EnemyParent = LoadClass<ACharacter>(nullptr, TEXT("/Script/Shade.ShadeEnemyCharacter"));
    UClass* DataClass = LoadClass<UObject>(nullptr, TEXT("/Script/Shade.ShadeInitialAttributesData"));
    UClass* BehaviorClass = LoadClass<UObject>(nullptr, TEXT("/Script/Shade.ShadeEnemyBehaviorData"));
    if (!TestNotNull(TEXT("Minion mesh"), Mesh) || !TestNotNull(TEXT("Minion skeleton"), Skeleton) ||
        !TestNotNull(TEXT("Idle sequence"), Idle) || !TestNotNull(TEXT("Jog sequence"), Jog) ||
        !TestNotNull(TEXT("Native Enemy animation class"), AnimParent) ||
        !TestNotNull(TEXT("Native Enemy character"), EnemyParent) || !TestNotNull(TEXT("Initial data class"), DataClass) || !TestNotNull(TEXT("Behavior data class"), BehaviorClass))
        return false;
    if (!TestTrue(TEXT("Mesh and animations share the exact skeleton"),
        Mesh->GetSkeleton() == Skeleton && Idle->GetSkeleton() == Skeleton && Jog->GetSkeleton() == Skeleton))
        return false;

    // Author a dedicated one-axis BlendSpace rather than changing the Paragon source.
    UPackage* BlendOuter = CreatePackage(BlendPackage);
    auto* Blend = NewObject<UBlendSpace1D>(BlendOuter, TEXT("BS_ShadeEnemyLocomotion"), RF_Public | RF_Standalone);
    Blend->SetSkeleton(Skeleton);
    Blend->SetPreviewMesh(Mesh);
    auto* ParameterProperty = FindFProperty<FStructProperty>(UBlendSpace::StaticClass(), TEXT("BlendParameters"));
    if (!TestNotNull(TEXT("Editable BlendSpace parameter"), ParameterProperty)) return false;
    FBlendParameter* Axis = ParameterProperty->ContainerPtrToValuePtr<FBlendParameter>(Blend, 0);
    Axis->DisplayName = TEXT("GroundSpeed");
    Axis->Min = 0.f;
    Axis->Max = LocomotionSpeed;
    Axis->GridNum = 6;
    Axis->bSnapToGrid = false;
    Axis->bWrapInput = false;
    if (!TestTrue(TEXT("Idle sample added"), Blend->AddSample(Idle, FVector::ZeroVector) != INDEX_NONE) ||
        !TestTrue(TEXT("Jog sample added"), Blend->AddSample(Jog, FVector(LocomotionSpeed, 0, 0)) != INDEX_NONE))
        return false;
    Blend->ValidateSampleData();
    Blend->ResampleData();
    Blend->PostEditChange();

    auto* Factory = NewObject<UAnimBlueprintFactory>();
    Factory->ParentClass = AnimParent;
    Factory->TargetSkeleton = Skeleton;
    Factory->PreviewSkeletalMesh = Mesh;
    auto* AnimBP = Cast<UAnimBlueprint>(Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(),
        CreatePackage(AnimPackage), TEXT("ABP_ShadeEnemy"), RF_Public | RF_Standalone, nullptr, GWarn));
    if (!TestNotNull(TEXT("Enemy animation Blueprint"), AnimBP)) return false;

    TArray<UEdGraph*> Graphs;
    AnimBP->GetAllGraphs(Graphs);
    UEdGraph* Graph = nullptr;
    UAnimGraphNode_Root* Root = nullptr;
    for (UEdGraph* Candidate : Graphs)
    {
        if (Candidate && Candidate->GetSchema() && Candidate->GetSchema()->IsA<UAnimationGraphSchema>())
        {
            TArray<UAnimGraphNode_Root*> Roots;
            Candidate->GetNodesOfClass(Roots);
            if (Roots.Num() == 1) { Graph = Candidate; Root = Roots[0]; break; }
        }
    }
    if (!TestNotNull(TEXT("Factory-created editable AnimGraph"), Graph) ||
        !TestNotNull(TEXT("Factory-created output pose"), Root)) return false;

    FGraphNodeCreator<UAnimGraphNode_BlendSpacePlayer> PlayerCreator(*Graph);
    auto* Player = PlayerCreator.CreateNode();
    Player->SetAnimationAsset(Blend);
    Player->NodePosX = -300;
    PlayerCreator.Finalize();

    FGraphNodeCreator<UK2Node_VariableGet> SpeedCreator(*Graph);
    auto* Speed = SpeedCreator.CreateNode();
    auto* SpeedProperty = FindFProperty<FFloatProperty>(AnimParent, TEXT("GroundSpeed"));
    if (!TestNotNull(TEXT("Native speed property"), SpeedProperty)) return false;
    Speed->SetFromProperty(SpeedProperty, true, AnimParent);
    Speed->NodePosX = -550;
    SpeedCreator.Finalize();

    const UEdGraphSchema* Schema = Graph->GetSchema();
    UEdGraphPin* SpeedOutput = FirstPin(Speed, EGPD_Output);
    UEdGraphPin* SpeedInput = Player->FindPin(TEXT("X"), EGPD_Input);
    UEdGraphPin* PoseOutput = FirstPin(Player, EGPD_Output);
    UEdGraphPin* PoseInput = Root->FindPin(TEXT("Result"), EGPD_Input);
    if (!TestNotNull(TEXT("GroundSpeed output"), SpeedOutput) || !TestNotNull(TEXT("BlendSpace X input"), SpeedInput) ||
        !TestNotNull(TEXT("BlendSpace pose"), PoseOutput) || !TestNotNull(TEXT("Output pose input"), PoseInput))
        return false;
    if (!TestTrue(TEXT("GroundSpeed connects to BlendSpace"), Schema->TryCreateConnection(SpeedOutput, SpeedInput)) ||
        !TestTrue(TEXT("BlendSpace connects to output pose"), Schema->TryCreateConnection(PoseOutput, PoseInput)))
        return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
    FKismetEditorUtilities::CompileBlueprint(AnimBP);
    if (!TestTrue(TEXT("Animation Blueprint compiles"), AnimBP->Status != BS_Error && AnimBP->GeneratedClass)) return false;

    UObject* Data = NewObject<UObject>(CreatePackage(DataPackage), DataClass,
        TEXT("DA_EnemyInitialAttributes"), RF_Public | RF_Standalone);
    if (!TestTrue(TEXT("Enemy settings exist"),
        SetFloat(Data, TEXT("InitialHealth"), 100.f) &&
        SetFloat(Data, TEXT("WalkSpeed"), 180.f) &&
        SetFloat(Data, TEXT("RunSpeed"), LocomotionSpeed) &&
        SetFloat(Data, TEXT("BrakingDecelerationWalking"), 1800.f))) return false;

    UObject* BehaviorData = NewObject<UObject>(CreatePackage(BehaviorPackage), BehaviorClass,
        TEXT("DA_EnemyBehavior"), RF_Public | RF_Standalone);
    // Awareness is initialized in Enemy BeginPlay, before the later sight/BT stage.
    if (!TestTrue(TEXT("Enemy awareness defaults are configured"),
        SetFloat(BehaviorData, TEXT("MaxAlertness"), 100.f) &&
        SetFloat(BehaviorData, TEXT("AlertnessGainPerSecond"), 100.f) &&
        SetFloat(BehaviorData, TEXT("AlertnessDecayPerSecond"), 25.f) &&
        SetFloat(BehaviorData, TEXT("AlertnessDecayDelay"), 2.f) &&
        SetFloat(BehaviorData, TEXT("AlertnessUpdatePeriod"), 0.1f))) return false;

    auto* CharacterBP = FKismetEditorUtilities::CreateBlueprint(EnemyParent, CreatePackage(CharacterPackage),
        TEXT("BP_ShadeEnemy"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Enemy Blueprint"), CharacterBP)) return false;
    FKismetEditorUtilities::CompileBlueprint(CharacterBP);
    auto* CDO = Cast<ACharacter>(CharacterBP->GeneratedClass->GetDefaultObject());
    if (!TestNotNull(TEXT("Enemy defaults"), CDO)) return false;
    auto* SettingsProperty = FindFProperty<FObjectPropertyBase>(CDO->GetClass(), TEXT("InitialAttributesData"));
    if (!TestNotNull(TEXT("Enemy initial data property"), SettingsProperty)) return false;
    SettingsProperty->SetObjectPropertyValue_InContainer(CDO, Data);
    auto* BehaviorProperty = FindFProperty<FObjectPropertyBase>(CDO->GetClass(), TEXT("BehaviorData"));
    if (!TestNotNull(TEXT("Enemy behavior data property"), BehaviorProperty)) return false;
    BehaviorProperty->SetObjectPropertyValue_InContainer(CDO, BehaviorData);
    CDO->GetMesh()->SetSkeletalMeshAsset(Mesh);
    CDO->GetMesh()->SetRelativeLocation(FVector(0, 0, -CDO->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
    CDO->GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
    CDO->GetMesh()->SetAnimInstanceClass(AnimBP->GeneratedClass);
    // Native Enemy source owns AI possession and movement policy.
    FBlueprintEditorUtils::MarkBlueprintAsModified(CharacterBP);

    // Persist referenced assets before their consumers; never save source Paragon assets.
    if (!TestTrue(TEXT("Saved custom BlendSpace"), SaveNew(Blend)) ||
        !TestTrue(TEXT("Saved animation Blueprint"), SaveNew(AnimBP)) ||
        !TestTrue(TEXT("Saved Enemy initial settings"), SaveNew(Data)) ||
        !TestTrue(TEXT("Saved Enemy behavior settings"), SaveNew(BehaviorData)) ||
        !TestTrue(TEXT("Saved Enemy Blueprint"), SaveNew(CharacterBP))) return false;
    AddInfo(TEXT("Saved only Enemy visual assets. Required next checks: separate-process reload, graph inspection, and PIE animation."));
    return true;
}
#endif
