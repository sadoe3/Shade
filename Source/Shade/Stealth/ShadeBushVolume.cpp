#include "ShadeBushVolume.h"
#include "Components/BoxComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"
#include "../Character/ShadePlayerCharacter.h"
#include "TimerManager.h"
#include "ShadeStealthEffects.h"

AShadeBushVolume::AShadeBushVolume()
{
    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
    SetRootComponent(Bounds);
    Bounds->SetBoxExtent(FVector(180.f,180.f,110.f));
    Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    Bounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Bounds->SetGenerateOverlapEvents(true);
}
void AShadeBushVolume::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority()) return;
    Bounds->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::Enter);
    Bounds->OnComponentEndOverlap.AddDynamic(this, &ThisClass::Exit);
    RefreshOccupants();
    // Covers spawn inside a bush and a PlayerState/ASC becoming ready after overlap.
    GetWorldTimerManager().SetTimer(RefreshTimer, this, &ThisClass::RefreshOccupants, 0.2f, true);
}
void AShadeBushVolume::Enter(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    RefreshOccupants();
}
void AShadeBushVolume::Exit(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32)
{
    RefreshOccupants();
}
void AShadeBushVolume::RefreshOccupants()
{
    if (!HasAuthority()) return;
    TArray<AActor*> Players;
    Bounds->GetOverlappingActors(Players, AShadePlayerCharacter::StaticClass());
    TSet<TWeakObjectPtr<UAbilitySystemComponent>> Present;
    for (AActor* Player : Players)
    {
        auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player);
        if (!ASC) continue;
        Present.Add(ASC);
        if (!Occupants.Contains(ASC))
        {
            const auto Spec = ASC->MakeOutgoingSpec(UShadeInBushEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
            if (Spec.IsValid())
            {
                const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
                if (Handle.IsValid()) Occupants.Add(ASC, Handle);
            }
        }
    }
    for (auto It = Occupants.CreateIterator(); It; ++It)
    {
        if (!Present.Contains(It.Key()))
        {
            if (auto* ASC = It.Key().Get()) ASC->RemoveActiveGameplayEffect(It.Value());
            It.RemoveCurrent();
        }
    }
}
void AShadeBushVolume::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(RefreshTimer);
    for (auto& Entry : Occupants)
        if (auto* ASC = Entry.Key.Get()) ASC->RemoveActiveGameplayEffect(Entry.Value);
    Occupants.Empty();
    Super::EndPlay(Reason);
}
