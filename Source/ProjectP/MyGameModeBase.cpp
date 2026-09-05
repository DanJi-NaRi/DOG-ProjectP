////////////////////////////
//! \page MyGameModeBase.cpp

#include "MyGameModeBase.h"
#include "GAS/MyPlayerState.h"

////////////////////////////
//! \brief 기본 PlayerStateClass를 AMyPlayerState로 설정한다.
//! \note BP GameMode가 PlayerStateClass를 덮어쓰면 에디터에서 별도로 MyPlayerState를 지정해야 한다.
AMyGameModeBase::AMyGameModeBase()
{
	PlayerStateClass = AMyPlayerState::StaticClass();
}
