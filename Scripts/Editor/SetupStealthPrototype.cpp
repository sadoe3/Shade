// One-time map authoring and read-only reload tests. Kept outside Source.
// Temporarily copy to Source/Shade/Tests and add Editor-only dependencies:
// UnrealEd, LevelEditor, NavigationSystem, AssetRegistry.
// Open a clean map and run Shade.Setup.PendingWorks.StealthPrototype once.
// Load /Game/Shade/Maps/L_StealthPrototype in a NEW Editor process and run
// Shade.Setup.PendingWorks.VerifyStealthPrototypeSaved to verify saved nav paths.
// Remove the temporary source, restore temporary dependencies, and rebuild.
// This tool never changes GameDefaultMap or EditorStartupMap.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Editor.h"
#include "LevelEditorSubsystem.h"
#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BrushComponent.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "AI/NavigationSystemBase.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformTime.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace ShadePrototypeSetup
{
const TCHAR* MapPackage = TEXT("/Game/Shade/Maps/L_StealthPrototype");
const TCHAR* BushMaterialPackage = TEXT("/Game/Shade/Prototype/MI_BushPlaceholder");
const FVector PlayerLocation(-1400.f, 0.f, 100.f);
const FVector EnemyLocations[] = {
    FVector(600.f, -950.f, 100.f),
    FVector(1100.f, 0.f, 100.f),
    FVector(600.f, 950.f, 100.f)
};

AStaticMeshActor* Box(UWorld* World, UStaticMesh* Cube, const TCHAR* Name,
    const FVector& Center, const FVector& Size, bool bCollide = true, UMaterialInterface* Material = nullptr)
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Actor = World->SpawnActor<AStaticMeshActor>(Center, FRotator::ZeroRotator, Params);
    if (!Actor) return nullptr;
    Actor->SetActorLabel(Name);
    auto* Mesh = Actor->GetStaticMeshComponent();
    Mesh->SetStaticMesh(Cube);
    Mesh->SetMobility(EComponentMobility::Static);
    Mesh->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
    Mesh->SetCanEverAffectNavigation(bCollide);
    Actor->SetActorScale3D(Size / 100.f);
    if (Material) Mesh->SetMaterial(0, Material);
    return Actor;
}

bool SaveMaterial(UMaterialInstanceConstant* Asset)
{
    FAssetRegistryModule::AssetCreated(Asset);
    Asset->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    return UPackage::SavePackage(Asset->GetOutermost(), Asset,
        *FPackageName::LongPackageNameToFilename(BushMaterialPackage, FPackageName::GetAssetPackageExtension()), Args);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeSetupStealthPrototype,
    "Shade.Setup.PendingWorks.StealthPrototype",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeSetupStealthPrototype::RunTest(const FString& Parameters)
{
    using namespace ShadePrototypeSetup;
    for (const TCHAR* Name : {MapPackage, BushMaterialPackage})
    {
        if (FPackageName::DoesPackageExist(Name) || FindPackage(nullptr, Name))
        {
            AddError(FString::Printf(TEXT("Existing destination is preserved: %s"), Name));
            return false;
        }
    }
    UWorld* PreviousWorld = GEditor->GetEditorWorldContext().World();
    if (PreviousWorld && PreviousWorld->GetOutermost()->IsDirty())
    {
        AddError(TEXT("Open a clean map before this one-time setup; the current unsaved map is preserved."));
        return false;
    }

    UClass* GameMode = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/Shade/Framework/BP_ShadeGameMode.BP_ShadeGameMode_C"));
    UClass* EnemyClass = LoadClass<AActor>(nullptr, TEXT("/Game/Shade/Enemy/BP_ShadeEnemy.BP_ShadeEnemy_C"));
    UClass* BushClass = LoadClass<AActor>(nullptr, TEXT("/Script/Shade.ShadeBushVolume"));
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!TestNotNull(TEXT("Configured GameMode"), GameMode) || !TestNotNull(TEXT("Configured Enemy Blueprint"), EnemyClass) ||
        !TestNotNull(TEXT("Bush volume class"), BushClass) || !TestNotNull(TEXT("Engine cube"), Cube) ||
        !TestNotNull(TEXT("Engine shape material"), ParentMaterial)) return false;

    auto* BushMaterial = NewObject<UMaterialInstanceConstant>(CreatePackage(BushMaterialPackage),
        TEXT("MI_BushPlaceholder"), RF_Public | RF_Standalone);
    BushMaterial->SetParentEditorOnly(ParentMaterial);
    BushMaterial->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Color")), FLinearColor(0.08f, 0.40f, 0.07f));
    BushMaterial->PostEditChange();
    if (!TestTrue(TEXT("Saved independent bush proxy material"), SaveMaterial(BushMaterial))) return false;

    auto* LevelEditor = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
    if (!TestNotNull(TEXT("Level editor subsystem"), LevelEditor) ||
        !TestTrue(TEXT("Created new prototype level"), LevelEditor->NewLevel(MapPackage, false))) return false;
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!TestNotNull(TEXT("Prototype world"), World)) return false;
    World->GetWorldSettings()->DefaultGameMode = GameMode;

    // 40 m x 40 m floor, one genuine visibility blocker, and a nonblocking green bush marker.
    if (!TestNotNull(TEXT("Prototype floor"), Box(World, Cube, TEXT("Prototype_Floor_4000"),
            FVector(0, 0, -50), FVector(4000, 4000, 100))) ||
        !TestNotNull(TEXT("Visibility wall"), Box(World, Cube, TEXT("Prototype_SightWall"),
            FVector(-100, 0, 150), FVector(160, 1000, 300)))) return false;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Start = World->SpawnActor<APlayerStart>(PlayerLocation, FRotator::ZeroRotator, Params);
    if (!TestNotNull(TEXT("Safe player start"), Start)) return false;
    Start->SetActorLabel(TEXT("Prototype_PlayerStart"));

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(EnemyLocations); ++Index)
    {
        AActor* Enemy = World->SpawnActor<AActor>(EnemyClass, EnemyLocations[Index], FRotator(0, 180, 0), Params);
        if (!TestNotNull(TEXT("Prototype Enemy"), Enemy)) return false;
        Enemy->SetActorLabel(FString::Printf(TEXT("Prototype_Enemy_%d"), Index + 1));
    }

    AActor* Bush = World->SpawnActor<AActor>(BushClass, FVector(-650, 650, 110), FRotator::ZeroRotator, Params);
    if (!TestNotNull(TEXT("Bush concealment volume"), Bush)) return false;
    Bush->SetActorLabel(TEXT("Prototype_BushVolume"));
    if (!TestNotNull(TEXT("Visible bush marker"), Box(World, Cube, TEXT("Prototype_BushVisual"),
        FVector(-650, 650, 30), FVector(360, 360, 60), false, BushMaterial))) return false;

    auto* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 600), FRotator(-55, -25, 0), Params);
    auto* Sky = World->SpawnActor<ASkyLight>(FVector(0, 0, 600), FRotator::ZeroRotator, Params);
    if (!TestNotNull(TEXT("Prototype sun"), Sun) || !TestNotNull(TEXT("Prototype skylight"), Sky)) return false;
    Sun->SetActorLabel(TEXT("Prototype_Sun"));
    Sky->SetActorLabel(TEXT("Prototype_SkyLight"));

    // A raw NavMeshBoundsVolume spawn has no authored brush. Build the real volume.
    auto* Bounds = World->SpawnActor<ANavMeshBoundsVolume>(FVector(0, 0, 150), FRotator::ZeroRotator, Params);
    if (!TestNotNull(TEXT("Navigation bounds actor"), Bounds)) return false;
    Bounds->SetActorLabel(TEXT("Prototype_NavMeshBounds"));
    auto* Builder = NewObject<UCubeBuilder>();
    Builder->X = 4000.f;
    Builder->Y = 4000.f;
    Builder->Z = 600.f;
    UActorFactory::CreateBrushForVolumeActor(Bounds, Builder);
    if (!TestTrue(TEXT("Navigation bounds owns a built brush"),
        Bounds->GetBrushComponent() && Bounds->GetBrushComponent()->Bounds.BoxExtent.X > 1000.f)) return false;

    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!Nav)
    {
        FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::EditorMode,
            World->GetWorldSettings()->GetNavigationSystemConfig(), true);
        Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    }
    if (!TestNotNull(TEXT("Navigation system"), Nav)) return false;
    Nav->OnNavigationBoundsUpdated(Bounds);
    const TWeakObjectPtr<UWorld> WeakWorld(World);
    const double StartSeconds = FPlatformTime::Seconds();
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WeakWorld, StartSeconds, bConfiguredNavData = false, bRequestedBuild = false]() mutable
    {
        UWorld* CurrentWorld = WeakWorld.Get();
        if (!CurrentWorld)
        {
            AddError(TEXT("Prototype world was closed before navigation verification."));
            return true;
        }
        auto* CurrentNav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(CurrentWorld);
        if (!CurrentNav)
        {
            AddError(TEXT("Prototype navigation system disappeared."));
            return true;
        }
        const bool bTimedOut = FPlatformTime::Seconds() - StartSeconds > 60.0;
        // Buildable bounds actors and asset-compilation locks are separate prerequisites.
        // The complete-path checks below confirm actual registration and generated data.
        const uint8 BlockingLocks = static_cast<uint8>(~ENavigationBuildLock::NoUpdateInEditor);
        const bool bHasBuildableWorld = CurrentNav->IsThereAnywhereToBuildNavigation();
        if (!bConfiguredNavData)
        {
            if (!bHasBuildableWorld)
            {
                if (!bTimedOut) return false;
                AddError(TEXT("World never reported a buildable navigation area."));
                return true;
            }
            auto* NavData = Cast<ARecastNavMesh>(CurrentNav->GetDefaultNavDataInstance(FNavigationSystem::Create));
            if (!TestNotNull(TEXT("Recast navigation actor"), NavData)) return true;
            auto* RuntimeProperty = FindFProperty<FEnumProperty>(NavData->GetClass(), TEXT("RuntimeGeneration"));
            if (!TestNotNull(TEXT("Runtime generation property"), RuntimeProperty)) return true;
            RuntimeProperty->GetUnderlyingProperty()->SetIntPropertyValue(
                RuntimeProperty->ContainerPtrToValuePtr<void>(NavData), static_cast<int64>(ERuntimeGenerationType::Dynamic));
            FPropertyChangedEvent Change(RuntimeProperty, EPropertyChangeType::ValueSet);
            NavData->PostEditChangeProperty(Change);
            if (!TestTrue(TEXT("Runtime navigation generation is Dynamic"),
                NavData->GetRuntimeGenerationMode() == ERuntimeGenerationType::Dynamic)) return true;
            bConfiguredNavData = true;
        }
        if (!bRequestedBuild)
        {
            if (!bHasBuildableWorld || CurrentNav->IsNavigationBuildingLocked(BlockingLocks))
            {
                if (!bTimedOut) return false;
                AddError(FString::Printf(TEXT("Navigation readiness timed out: BuildableWorld=%d BlockingLock=%d"),
                    bHasBuildableWorld, CurrentNav->IsNavigationBuildingLocked(BlockingLocks)));
                return true;
            }
            CurrentNav->Build();
            bRequestedBuild = true;
        }
        if (CurrentNav->IsNavigationBuildInProgress() && !bTimedOut) return false;
        FNavLocation ProjectedStart;
        bool bPathsReady = CurrentNav->ProjectPointToNavigation(PlayerLocation, ProjectedStart, FVector(100, 100, 500));
        for (const FVector& EnemyLocation : EnemyLocations)
        {
            FNavLocation ProjectedEnemy;
            if (!CurrentNav->ProjectPointToNavigation(EnemyLocation, ProjectedEnemy, FVector(100, 100, 500)))
            {
                bPathsReady = false;
                continue;
            }
            UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
                CurrentWorld, ProjectedEnemy.Location, ProjectedStart.Location);
            bPathsReady &= Path && Path->IsValid() && !Path->IsPartial() && Path->PathPoints.Num() >= 2;
        }
        if (!bPathsReady && !bTimedOut) return false;
        if (!TestTrue(TEXT("All three Enemy positions have complete navigation paths to PlayerStart"), bPathsReady))
            return true;
        auto* Subsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
        if (!TestNotNull(TEXT("Level editor subsystem remains available"), Subsystem)) return true;
        TestTrue(TEXT("Saved completed prototype map with navigation data"), Subsystem->SaveCurrentLevel());
        AddInfo(TEXT("GameDefaultMap is unchanged. Separate-process reload and standalone dynamic-navigation checks remain required."));
        return true;
    }));
    return true;
}
// Launch a new Editor process with /Game/Shade/Maps/L_StealthPrototype, then run
// this test. It does not call Build() or save, proving saved/dynamic navigation.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeVerifyStealthPrototypeSaved,
    "Shade.Setup.PendingWorks.VerifyStealthPrototypeSaved",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeVerifyStealthPrototypeSaved::RunTest(const FString& Parameters)
{
    using namespace ShadePrototypeSetup;
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!TestTrue(TEXT("Separate process loaded the prototype map"),
        World && World->GetOutermost()->GetName() == MapPackage)) return false;
    UClass* ExpectedEnemy = LoadClass<AActor>(nullptr, TEXT("/Game/Shade/Enemy/BP_ShadeEnemy.BP_ShadeEnemy_C"));
    UClass* ExpectedBush = LoadClass<AActor>(nullptr, TEXT("/Script/Shade.ShadeBushVolume"));
    int32 Enemies = 0;
    int32 Bushes = 0;
    int32 Starts = 0;
    ANavMeshBoundsVolume* Bounds = nullptr;
    ARecastNavMesh* Recast = nullptr;
    AStaticMeshActor* BushVisual = nullptr;
    AStaticMeshActor* Wall = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (ExpectedEnemy && It->IsA(ExpectedEnemy)) ++Enemies;
        if (ExpectedBush && It->IsA(ExpectedBush)) ++Bushes;
        if (It->IsA<APlayerStart>()) ++Starts;
        if (auto* Candidate = Cast<ANavMeshBoundsVolume>(*It)) Bounds = Candidate;
        if (auto* Candidate = Cast<ARecastNavMesh>(*It)) Recast = Candidate;
        if (It->GetActorLabel() == TEXT("Prototype_BushVisual")) BushVisual = Cast<AStaticMeshActor>(*It);
        if (It->GetActorLabel() == TEXT("Prototype_SightWall")) Wall = Cast<AStaticMeshActor>(*It);
    }
    if (!TestEqual(TEXT("Three configured Enemies survived reload"), Enemies, 3) ||
        !TestEqual(TEXT("One Bush concealment volume survived reload"), Bushes, 1) ||
        !TestEqual(TEXT("One safe PlayerStart survived reload"), Starts, 1) ||
        !TestTrue(TEXT("Navigation brush survived reload"),
            Bounds && Bounds->GetBrushComponent() && Bounds->GetBrushComponent()->Bounds.BoxExtent.X > 1000.f) ||
        !TestTrue(TEXT("Dynamic Recast actor survived reload"),
            Recast && Recast->GetRuntimeGenerationMode() == ERuntimeGenerationType::Dynamic) ||
        !TestTrue(TEXT("Bush proxy stays nonblocking"),
            BushVisual && BushVisual->GetStaticMeshComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision) ||
        !TestTrue(TEXT("Sight wall blocks Visibility"),
            Wall && Wall->GetStaticMeshComponent()->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block))
        return false;

    const TWeakObjectPtr<UWorld> WeakWorld(World);
    const double StartSeconds = FPlatformTime::Seconds();
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WeakWorld, StartSeconds]()
    {
        UWorld* CurrentWorld = WeakWorld.Get();
        auto* Nav = CurrentWorld ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(CurrentWorld) : nullptr;
        if (!Nav)
        {
            AddError(TEXT("Reloaded map has no navigation system."));
            return true;
        }
        const bool bTimedOut = FPlatformTime::Seconds() - StartSeconds > 60.0;
        if (Nav->IsNavigationBuildInProgress() && !bTimedOut) return false;
        FNavLocation Start;
        bool bAllPaths = Nav->ProjectPointToNavigation(PlayerLocation, Start, FVector(100, 100, 500));
        if (bAllPaths)
        {
            for (const FVector& EnemyLocation : EnemyLocations)
            {
                FNavLocation Enemy;
                if (!Nav->ProjectPointToNavigation(EnemyLocation, Enemy, FVector(100, 100, 500)))
                {
                    bAllPaths = false;
                    break;
                }
                auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(CurrentWorld, Enemy.Location, Start.Location);
                if (!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2)
                {
                    bAllPaths = false;
                    break;
                }
            }
        }
        if (!bAllPaths && !bTimedOut) return false;
        TestTrue(TEXT("All three complete nav paths work after separate-process map reload"), bAllPaths);
        return true;
    }));
    return true;
}
#endif
