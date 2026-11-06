// One-time asset authoring and read-only reload tests. Kept outside Source.
// Temporarily copy to Source/Shade/Tests and add Editor dependencies: UnrealEd,
// BlueprintGraph, BehaviorTreeEditor, AIGraph, AssetRegistry. AIModule is runtime.
// Run Shade.Setup.PendingWorks.EnemyBehavior once. Then use a separate Editor
// process to run Shade.Setup.PendingWorks.VerifyEnemyBehaviorSaved.
// Remove the temporary source, restore dependencies, and rebuild afterward.
// Existing new-asset destinations are always preserved.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTreeGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "EdGraphSchema_BehaviorTree.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Character.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace ShadeEnemyBehaviorSetup
{
const TCHAR* BlackboardPackage = TEXT("/Game/Shade/Enemy/AI/BB_ShadeEnemy");
const TCHAR* TreePackage = TEXT("/Game/Shade/Enemy/AI/BT_ShadeEnemy");
const TCHAR* ControllerPackage = TEXT("/Game/Shade/Enemy/AI/BP_ShadeAIController");
const TCHAR* DataPackage = TEXT("/Game/Shade/Data/DA_EnemyBehavior");
const TCHAR* EnemyPath = TEXT("/Game/Shade/Enemy/BP_ShadeEnemy.BP_ShadeEnemy");

bool Save(UObject* Asset, bool bNew)
{
    if (bNew) FAssetRegistryModule::AssetCreated(Asset);
    Asset->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    return UPackage::SavePackage(Asset->GetOutermost(), Asset,
        *FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}

bool SetObject(UObject* Target, FName Name, UObject* Value)
{
    auto* Property = FindFProperty<FObjectPropertyBase>(Target->GetClass(), Name);
    if (!Property || !Value || !Value->IsA(Property->PropertyClass)) return false;
    Property->SetObjectPropertyValue_InContainer(Target, Value);
    return true;
}

bool SetFloat(UObject* Target, FName Name, float Value)
{
    auto* Property = FindFProperty<FFloatProperty>(Target->GetClass(), Name);
    if (!Property) return false;
    Property->SetPropertyValue_InContainer(Target, Value);
    return true;
}

// AutoArrange requires live Slate graph widgets. Headless authoring instead lays
// out leaves in their existing link order, preserving Selector child priority.
void LayoutNode(UBehaviorTreeGraphNode* Node, int32 Depth, int32& NextLeafX)
{
    TArray<UBehaviorTreeGraphNode*> Children;
    if (UEdGraphPin* Output = Node->GetOutputPin())
    {
        for (UEdGraphPin* Linked : Output->LinkedTo)
        {
            if (auto* Child = Cast<UBehaviorTreeGraphNode>(Linked->GetOwningNode()))
                Children.Add(Child);
        }
    }
    if (Children.IsEmpty())
    {
        Node->NodePosX = NextLeafX;
        NextLeafX += 360;
    }
    else
    {
        for (auto* Child : Children) LayoutNode(Child, Depth + 1, NextLeafX);
        Node->NodePosX = (Children[0]->NodePosX + Children.Last()->NodePosX) / 2;
    }
    Node->NodePosY = Depth * 240;
}

bool SetByte(UObject* Target, FName Name, uint8 Value)
{
    auto* Property = FindFProperty<FByteProperty>(Target->GetClass(), Name);
    if (!Property) return false;
    Property->SetPropertyValue_InContainer(Target, Value);
    return true;
}

bool SelectKey(UObject* Node, UBlackboardData* Blackboard, FName Name)
{
    auto* Property = FindFProperty<FStructProperty>(Node->GetClass(), TEXT("BlackboardKey"));
    if (!Property || Property->Struct != FBlackboardKeySelector::StaticStruct()) return false;
    auto* Selector = Property->ContainerPtrToValuePtr<FBlackboardKeySelector>(Node);
    Selector->SelectedKeyName = Name;
    Selector->ResolveSelectedKey(*Blackboard);
    return Selector->GetSelectedKeyID() != FBlackboard::InvalidKey;
}

void Label(UObject* Node, const TCHAR* Name)
{
    if (auto* Property = FindFProperty<FStrProperty>(Node->GetClass(), TEXT("NodeName")))
        Property->SetPropertyValue_InContainer(Node, Name);
}

UBTTask_MoveTo* Move(UBehaviorTree* Tree, UBlackboardData* Blackboard, FName Key, const TCHAR* Name)
{
    auto* Node = NewObject<UBTTask_MoveTo>(Tree);
    if (!SelectKey(Node, Blackboard, Key)) return nullptr;
    Node->AcceptableRadius = FValueOrBBKey_Float(65.f);
    Node->bObserveBlackboardValue = true;
    Label(Node, Name);
    return Node;
}

UBTTask_Wait* Wait(UBehaviorTree* Tree, float Seconds, FName Key = NAME_None)
{
    auto* Node = NewObject<UBTTask_Wait>(Tree);
    Node->WaitTime = FValueOrBBKey_Float(Seconds);
    if (!Key.IsNone()) Node->WaitTime.SetKey(Key);
    Node->RandomDeviation = FValueOrBBKey_Float(0.f);
    return Node;
}

UBTDecorator_Blackboard* IsSet(UBehaviorTree* Tree, UBlackboardData* Blackboard, FName Key)
{
    auto* Node = NewObject<UBTDecorator_Blackboard>(Tree);
    if (!SelectKey(Node, Blackboard, Key) ||
        !SetByte(Node, TEXT("BasicOperation"), EBasicKeyOperation::Set) ||
        !SetByte(Node, TEXT("OperationType"), EBasicKeyOperation::Set) ||
        !SetByte(Node, TEXT("FlowAbortMode"), EBTFlowAbortMode::Both) ||
        !SetByte(Node, TEXT("NotifyObserver"), EBTBlackboardRestart::ResultChange)) return nullptr;
    return Node;
}

void AddTask(UBTCompositeNode* Parent, UBTTaskNode* Task, UBTDecorator* Decorator = nullptr)
{
    FBTCompositeChild Child;
    Child.ChildTask = Task;
    if (Decorator) Child.Decorators.Add(Decorator);
    Parent->Children.Add(Child);
}

void AddComposite(UBTCompositeNode* Parent, UBTCompositeNode* Composite, UBTDecorator* Decorator = nullptr)
{
    FBTCompositeChild Child;
    Child.ChildComposite = Composite;
    if (Decorator) Child.Decorators.Add(Decorator);
    Parent->Children.Add(Child);
}

template <typename T> T* AddKey(UBlackboardData* Data, const TCHAR* Name)
{
    FBlackboardEntry Entry;
    Entry.EntryName = Name;
    auto* Type = NewObject<T>(Data);
    Entry.KeyType = Type;
    Data->Keys.Add(Entry);
    return Type;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeSetupEnemyBehavior,
    "Shade.Setup.PendingWorks.EnemyBehavior",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeSetupEnemyBehavior::RunTest(const FString& Parameters)
{
    using namespace ShadeEnemyBehaviorSetup;
    for (const TCHAR* Name : {BlackboardPackage, TreePackage, ControllerPackage})
    {
        if (FPackageName::DoesPackageExist(Name) || FindPackage(nullptr, Name))
        {
            AddError(FString::Printf(TEXT("Existing destination is preserved: %s"), Name));
            return false;
        }
    }
    auto* EnemyBP = LoadObject<UBlueprint>(nullptr, EnemyPath);
    UClass* ControllerClass = LoadClass<AAIController>(nullptr, TEXT("/Script/Shade.ShadeAIController"));
    UObject* Settings = LoadObject<UObject>(nullptr, TEXT("/Game/Shade/Data/DA_EnemyBehavior.DA_EnemyBehavior"));
    UClass* FinishClass = LoadClass<UBTTaskNode>(nullptr, TEXT("/Script/Shade.BTTask_ShadeFinishSearch"));
    if (!TestNotNull(TEXT("Existing authored Enemy Blueprint"), EnemyBP) ||
        !TestNotNull(TEXT("Native AIController"), ControllerClass) ||
        !TestNotNull(TEXT("Existing behavior settings"), Settings) ||
        !TestNotNull(TEXT("Native finish-search task"), FinishClass)) return false;

    auto* Blackboard = NewObject<UBlackboardData>(CreatePackage(BlackboardPackage), TEXT("BB_ShadeEnemy"), RF_Public | RF_Standalone);
    AddKey<UBlackboardKeyType_Object>(Blackboard, TEXT("TargetActor"))->BaseClass = AActor::StaticClass();
    AddKey<UBlackboardKeyType_Vector>(Blackboard, TEXT("LastKnownLocation"));
    AddKey<UBlackboardKeyType_Vector>(Blackboard, TEXT("HomeLocation"));
    AddKey<UBlackboardKeyType_Float>(Blackboard, TEXT("SearchDuration"));
    Blackboard->PostEditChange();
    if (!TestTrue(TEXT("Blackboard keys are valid"), Blackboard->IsValid())) return false;

    auto* Tree = NewObject<UBehaviorTree>(CreatePackage(TreePackage), TEXT("BT_ShadeEnemy"), RF_Public | RF_Standalone);
    Tree->BlackboardAsset = Blackboard;
    auto* Root = NewObject<UBTComposite_Selector>(Tree);
    Tree->RootNode = Root;
    Label(Root, TEXT("Chase, search, or return home"));

    auto* Chase = Move(Tree, Blackboard, TEXT("TargetActor"), TEXT("Chase visible alert target"));
    auto* HasTarget = IsSet(Tree, Blackboard, TEXT("TargetActor"));
    auto* HasMemory = IsSet(Tree, Blackboard, TEXT("LastKnownLocation"));
    auto* SearchMove = Move(Tree, Blackboard, TEXT("LastKnownLocation"), TEXT("Investigate last known location"));
    auto* HomeMove = Move(Tree, Blackboard, TEXT("HomeLocation"), TEXT("Return home"));
    if (!TestTrue(TEXT("Movement and observer keys resolve"), Chase && HasTarget && HasMemory && SearchMove && HomeMove))
        return false;
    // Failed chase paths retry after a short pause instead of falling into search.
    auto* ChaseOrRetry = NewObject<UBTComposite_Selector>(Tree);
    Label(ChaseOrRetry, TEXT("Chase visible target or retry"));
    AddTask(ChaseOrRetry, Chase);
    AddTask(ChaseOrRetry, Wait(Tree, 0.2f));
    AddComposite(Root, ChaseOrRetry, HasTarget);

    auto* Search = NewObject<UBTComposite_Sequence>(Tree);
    Label(Search, TEXT("Search remembered location"));
    auto* ReachOrContinue = NewObject<UBTComposite_Selector>(Tree);
    Label(ReachOrContinue, TEXT("Continue search even if location is unreachable"));
    AddTask(ReachOrContinue, SearchMove);
    AddTask(ReachOrContinue, Wait(Tree, 0.1f));
    AddComposite(Search, ReachOrContinue);
    AddTask(Search, Wait(Tree, 3.f, TEXT("SearchDuration")));
    AddTask(Search, NewObject<UBTTaskNode>(Tree, FinishClass));
    AddComposite(Root, Search, HasMemory);

    auto* Home = NewObject<UBTComposite_Sequence>(Tree);
    Label(Home, TEXT("Return and idle"));
    auto* HomeOrRetry = NewObject<UBTComposite_Selector>(Tree);
    Label(HomeOrRetry, TEXT("Return home or defer unreachable path"));
    AddTask(HomeOrRetry, HomeMove);
    AddTask(HomeOrRetry, Wait(Tree, 0.2f));
    AddComposite(Home, HomeOrRetry);
    AddTask(Home, Wait(Tree, 1.f));
    AddComposite(Root, Home);

    // Reconstruct the actual editable graph from the runtime tree before save.
    // OnCreated calls SpawnMissingNodes and connects the editor root.
    auto* Graph = Cast<UBehaviorTreeGraph>(FBlueprintEditorUtils::CreateNewGraph(Tree, TEXT("Behavior Tree"),
        UBehaviorTreeGraph::StaticClass(), UEdGraphSchema_BehaviorTree::StaticClass()));
    if (!TestNotNull(TEXT("Editable Behavior Tree graph"), Graph)) return false;
    Tree->BTGraph = Graph;
    // AddSubNode calls UpdateAsset before assigning the recovered NodeInstance.
    // Keep graph updates locked until all runtime nodes and links are restored.
    Graph->LockUpdates();
    Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
    TArray<UBehaviorTreeGraphNode_Root*> EditorRoots;
    Graph->GetNodesOfClass(EditorRoots);
    if (!TestTrue(TEXT("Exactly one editor graph root"), EditorRoots.Num() == 1)) return false;
    EditorRoots[0]->BlackboardAsset = Blackboard;
    Tree->BlackboardAsset = Blackboard;
    Graph->OnCreated();
    Graph->Initialize();
    int32 NextLeafX = 0;
    LayoutNode(EditorRoots[0], 0, NextLeafX);
    Graph->UnlockUpdates();
    Graph->OnSave();
    if (!TestTrue(TEXT("Tree retained all three branches after graph rebuild"),
        Tree->RootNode && Tree->RootNode->Children.Num() == 3 && Graph->Nodes.Num() > 4)) return false;


    if (!TestTrue(TEXT("Sight and search settings are configured"),
        SetFloat(Settings, TEXT("SightRadius"), 1600.f) &&
        SetFloat(Settings, TEXT("SightHalfAngleDegrees"), 60.f) &&
        SetFloat(Settings, TEXT("SightUpdateInterval"), 0.1f) &&
        SetFloat(Settings, TEXT("SearchDuration"), 3.f))) return false;
    Settings->PostEditChange();
    auto* ControllerBP = FKismetEditorUtilities::CreateBlueprint(ControllerClass, CreatePackage(ControllerPackage),
        TEXT("BP_ShadeAIController"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("AIController Blueprint"), ControllerBP)) return false;
    FKismetEditorUtilities::CompileBlueprint(ControllerBP);
    if (!TestTrue(TEXT("AIController Blueprint compiled"), ControllerBP->Status != BS_Error && ControllerBP->GeneratedClass)) return false;
    UObject* ControllerDefaults = ControllerBP->GeneratedClass->GetDefaultObject();
    if (!TestTrue(TEXT("Controller points to authored Behavior Tree"), SetObject(ControllerDefaults, TEXT("BehaviorTreeAsset"), Tree))) return false;
    FBlueprintEditorUtils::MarkBlueprintAsModified(ControllerBP);

    auto* EnemyDefaults = Cast<ACharacter>(EnemyBP->GeneratedClass->GetDefaultObject());
    if (!TestNotNull(TEXT("Enemy defaults"), EnemyDefaults) ||
        !TestTrue(TEXT("Enemy references behavior settings"), SetObject(EnemyDefaults, TEXT("BehaviorData"), Settings))) return false;
    EnemyDefaults->AIControllerClass = ControllerBP->GeneratedClass;
    EnemyDefaults->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    FBlueprintEditorUtils::MarkBlueprintAsModified(EnemyBP);

    if (!TestTrue(TEXT("Blackboard saved"), Save(Blackboard, true)) ||
        !TestTrue(TEXT("Editable Behavior Tree saved"), Save(Tree, true)) ||
        !TestTrue(TEXT("Enemy behavior data saved"), Save(Settings, false)) ||
        !TestTrue(TEXT("AIController Blueprint saved"), Save(ControllerBP, true)) ||
        !TestTrue(TEXT("Own Enemy Blueprint updated"), Save(EnemyBP, false))) return false;
    AddInfo(TEXT("Separate-process reload and chase/search/abort PIE checks remain required."));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeVerifyEnemyBehaviorSaved,
    "Shade.Setup.PendingWorks.VerifyEnemyBehaviorSaved",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeVerifyEnemyBehaviorSaved::RunTest(const FString& Parameters)
{
    auto* Tree = LoadObject<UBehaviorTree>(nullptr, TEXT("/Game/Shade/Enemy/AI/BT_ShadeEnemy.BT_ShadeEnemy"));
    auto* Blackboard = LoadObject<UBlackboardData>(nullptr, TEXT("/Game/Shade/Enemy/AI/BB_ShadeEnemy.BB_ShadeEnemy"));
    auto* ControllerBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Shade/Enemy/AI/BP_ShadeAIController.BP_ShadeAIController"));
    auto* EnemyBP = LoadObject<UBlueprint>(nullptr, ShadeEnemyBehaviorSetup::EnemyPath);
    auto* Settings = LoadObject<UObject>(nullptr, TEXT("/Game/Shade/Data/DA_EnemyBehavior.DA_EnemyBehavior"));
    if (!TestNotNull(TEXT("Saved Behavior Tree"), Tree) || !TestNotNull(TEXT("Saved Blackboard"), Blackboard) ||
        !TestNotNull(TEXT("Saved AIController Blueprint"), ControllerBP) || !TestNotNull(TEXT("Saved Enemy Blueprint"), EnemyBP) ||
        !TestNotNull(TEXT("Saved behavior settings"), Settings)) return false;
    auto* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
    if (!TestTrue(TEXT("Actual editable graph survived reload"), Graph && Graph->Nodes.Num() > 4) ||
        !TestTrue(TEXT("All three runtime branches survived reload"),
            Tree->RootNode && Tree->RootNode->Children.Num() == 3) ||
        !TestTrue(TEXT("Runtime tree references the saved Blackboard"), Tree->BlackboardAsset == Blackboard))
        return false;

    for (const FName Key : {FName(TEXT("TargetActor")), FName(TEXT("LastKnownLocation")),
        FName(TEXT("HomeLocation")), FName(TEXT("SearchDuration"))})
    {
        if (!TestTrue(*FString::Printf(TEXT("Blackboard key %s survived reload"), *Key.ToString()),
            Blackboard->GetKeyID(Key) != FBlackboard::InvalidKey)) return false;
    }
    auto* Chase = Tree->RootNode->Children[0].ChildComposite.Get();
    auto* Search = Tree->RootNode->Children[1].ChildComposite.Get();
    auto* Home = Tree->RootNode->Children[2].ChildComposite.Get();
    if (!TestTrue(TEXT("Chase/search/home composite order survived reload"), Chase && Search && Home)) return false;
    auto* ChaseMove = Chase->Children.IsEmpty() ? nullptr : Cast<UBTTask_MoveTo>(Chase->Children[0].ChildTask);
    if (!TestTrue(TEXT("Chase uses TargetActor"), ChaseMove && ChaseMove->GetSelectedBlackboardKey() == TEXT("TargetActor")))
        return false;
    if (!TestTrue(TEXT("Search retains movement, timed wait, and finish task"), Search->Children.Num() == 3)) return false;
    auto* SearchWait = Cast<UBTTask_Wait>(Search->Children[1].ChildTask);
    if (!TestTrue(TEXT("Search wait uses Blackboard duration"), SearchWait && SearchWait->WaitTime.GetKey() == TEXT("SearchDuration")))
        return false;

    UObject* ControllerDefaults = ControllerBP->GeneratedClass->GetDefaultObject();
    auto* TreeProperty = FindFProperty<FObjectPropertyBase>(ControllerDefaults->GetClass(), TEXT("BehaviorTreeAsset"));
    if (!TestTrue(TEXT("Controller still references saved tree"),
        TreeProperty && TreeProperty->GetObjectPropertyValue_InContainer(ControllerDefaults) == Tree)) return false;
    auto* EnemyDefaults = Cast<ACharacter>(EnemyBP->GeneratedClass->GetDefaultObject());
    auto* BehaviorProperty = EnemyDefaults ? FindFProperty<FObjectPropertyBase>(EnemyDefaults->GetClass(), TEXT("BehaviorData")) : nullptr;
    if (!TestTrue(TEXT("Enemy uses authored controller and behavior data"),
        EnemyDefaults && EnemyDefaults->AIControllerClass == ControllerBP->GeneratedClass &&
        EnemyDefaults->AutoPossessAI == EAutoPossessAI::PlacedInWorldOrSpawned && BehaviorProperty &&
        BehaviorProperty->GetObjectPropertyValue_InContainer(EnemyDefaults) == Settings)) return false;
    AddInfo(TEXT("Saved BT graph, runtime branches, Blackboard and Blueprint wiring verified without modifying assets."));
    return true;
}
#endif
