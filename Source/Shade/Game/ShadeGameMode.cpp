#include "Game/ShadeGameMode.h"
#include "Character/ShadePlayerCharacter.h"
#include "Player/ShadePlayerController.h"
#include "Player/ShadePlayerState.h"

AShadeGameMode::AShadeGameMode()
{
    DefaultPawnClass = AShadePlayerCharacter::StaticClass();
    PlayerControllerClass = AShadePlayerController::StaticClass();
    PlayerStateClass = AShadePlayerState::StaticClass();
}
