#include "LoginFlowSubsystem.h"

#include "AccountSessionSubsystem.h"
#include "SessionMaintenanceSubsystem.h"
#include "Engine/GameInstance.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 성공 후 계정 세션 정보를 저장하고 세션 ping을 시작하는 함수
// UserIndex : 로그인에 성공한 유저의 DB user_Index
// Username : 로그인에 성공한 유저의 DB username
// LoginToken : 로그인 서버에서 발급받은 세션 토큰
void ULoginFlowSubsystem::HandleLoginSuccess(int32 UserIndex, const FString& Username, const FString& LoginToken)
{
    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot handle login success because GameInstance is null."));
        return;
    }

    UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance->GetSubsystem<UAccountSessionSubsystem>();
    if (AccountSessionSubsystem == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot handle login success because AccountSessionSubsystem is missing."));
        return;
    }

    AccountSessionSubsystem->SetLoginInfo(UserIndex, Username);
    AccountSessionSubsystem->SetLoginToken(LoginToken);

    if (USessionMaintenanceSubsystem* SessionMaintenanceSubsystem = GameInstance->GetSubsystem<USessionMaintenanceSubsystem>())
    {
        SessionMaintenanceSubsystem->StartSessionPing();
    }
}
