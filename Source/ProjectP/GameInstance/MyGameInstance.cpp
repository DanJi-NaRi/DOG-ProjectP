
#include "MyGameInstance.h"

void UMyGameInstance::Init()
{
	Super::Init();

	// Initialize subsystems.
	// Initialize logic.
	
}


void UMyGameInstance::Shutdown()
{
	Super::Shutdown();
	
	//Shutdown logic.

}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 실행 중인 채팅창 배경 표시 설정을 반환하는 함수
// Return Value : 채팅창 배경 표시 여부
bool UMyGameInstance::IsChatBackgroundEnabled() const
{
	return bChatBackgroundEnabled;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 실행 중인 채팅창 배경 표시 설정을 저장하는 함수
// bInEnabled : 채팅창 배경 표시 여부
void UMyGameInstance::SetChatBackgroundEnabled(bool bInEnabled)
{
	bChatBackgroundEnabled = bInEnabled;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 실행 중인 채팅 메시지 폰트 크기 단계를 반환하는 함수
// Return Value : 폰트 크기 단계(0=작음, 1=중간, 2=큼)
int32 UMyGameInstance::GetChatFontSizeLevel() const
{
	return ChatFontSizeLevel;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 실행 중인 채팅 메시지 폰트 크기 단계를 저장하는 함수
// InLevel : 폰트 크기 단계(0=작음, 1=중간, 2=큼)
void UMyGameInstance::SetChatFontSizeLevel(int32 InLevel)
{
	ChatFontSizeLevel = FMath::Clamp(InLevel, 0, 2);
}
