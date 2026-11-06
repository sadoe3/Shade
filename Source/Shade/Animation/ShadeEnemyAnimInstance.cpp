#include "ShadeEnemyAnimInstance.h"

#include "GameFramework/Pawn.h"

void UShadeEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const APawn* Pawn = TryGetPawnOwner();
    GroundSpeed = IsValid(Pawn) ? Pawn->GetVelocity().Size2D() : 0.f;
}
