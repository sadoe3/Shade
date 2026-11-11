#include "ShadeBushVolume.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "../Character/ShadePlayerCharacter.h"
#include "ShadeStealthEffects.h"

AShadeBushVolume::AShadeBushVolume()
{
    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
    SetRootComponent(Bounds);
    Bounds->SetBoxExtent(FVector(180.f, 180.f, 110.f));
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
}

void AShadeBushVolume::Enter(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent* OtherComponent,
    int32, bool, const FHitResult&)
{
    if (!HasAuthority()) return;
    auto* Player = Cast<AShadePlayerCharacter>(Other);
    // Only the player's capsule owns the overlap; mesh overlaps must not duplicate the grant.
    if (!Player || OtherComponent != Player->GetCapsuleComponent() || Occupant.IsValid()) return;
    auto* ASC = Player->GetAbilitySystemComponent();
    // The player's ASC must be ready when entering. No readiness polling or body-transfer recovery.
    if (!ASC) return;
    const auto Spec = ASC->MakeOutgoingSpec(UShadeInBushEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
    if (!Spec.IsValid()) return;
    const auto Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    if (!Handle.IsValid()) return;
    Occupant = Player;
    OccupantASC = ASC;
    BushEffectHandle = Handle;
}

void AShadeBushVolume::Exit(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent* OtherComponent, int32)
{
    if (!HasAuthority()) return;
    auto* Player = Cast<AShadePlayerCharacter>(Other);
    if (!Player || OtherComponent != Player->GetCapsuleComponent() || Occupant.Get() != Player) return;
    RemoveBushEffect();
}

void AShadeBushVolume::RemoveBushEffect()
{
    // Keep the original ASC: the player may no longer expose it when leaving.
    if (auto* ASC = OccupantASC.Get()) ASC->RemoveActiveGameplayEffect(BushEffectHandle);
    BushEffectHandle.Invalidate();
    OccupantASC.Reset();
    Occupant.Reset();
}

void AShadeBushVolume::EndPlay(const EEndPlayReason::Type Reason)
{
    RemoveBushEffect();
    Super::EndPlay(Reason);
}
