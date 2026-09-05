#include "AccountSessionSubsystem.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인한 계정의 기본 정보를 저장하는 함수
// NewUserIndex : 로그인한 유저의 DB user_Index
// NewUsername : 로그인한 유저의 DB username
void UAccountSessionSubsystem::SetLoginInfo(int32 NewUserIndex, const FString& NewUsername)
{
    UserIndex = NewUserIndex;
    Username = NewUsername;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 세션 토큰을 저장하는 함수
// NewLoginToken : 로그인 서버에서 발급받은 세션 토큰
void UAccountSessionSubsystem::SetLoginToken(const FString& NewLoginToken)
{
    LoginToken = NewLoginToken;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 저장된 로그인 계정 정보와 세션 토큰을 초기화하는 함수
void UAccountSessionSubsystem::ClearLoginInfo()
{
    UserIndex = -1;
    Username.Empty();
    LoginToken.Empty();
    LastUsedCharacterId = -1;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버 이동 직전에 사용하던 캐릭터 ID를 보관하는 함수
// NewCharacterId : 보관할 캐릭터 ID
void UAccountSessionSubsystem::SetLastUsedCharacterId(int32 NewCharacterId)
{
    LastUsedCharacterId = NewCharacterId;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 보관된 직전 사용 캐릭터 ID를 초기화하는 함수
void UAccountSessionSubsystem::ClearLastUsedCharacterId()
{
    LastUsedCharacterId = -1;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버 이동 직전에 사용하던 캐릭터 ID를 반환하는 함수
// Return Value : 보관된 캐릭터 ID, 없으면 -1
int32 UAccountSessionSubsystem::GetLastUsedCharacterId() const
{
    return LastUsedCharacterId;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인한 유저의 DB user_Index를 반환하는 함수
// Return Value : 로그인한 유저의 DB user_Index, 로그인 정보가 없으면 -1
int32 UAccountSessionSubsystem::GetUserIndex() const
{
    return UserIndex;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인한 유저의 DB username을 반환하는 함수
// Return Value : 로그인한 유저의 DB username, 로그인 정보가 없으면 빈 문자열
const FString& UAccountSessionSubsystem::GetUsername() const
{
    return Username;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 세션 토큰을 반환하는 함수
// Return Value : 로그인 서버에서 발급받은 세션 토큰, 로그인 정보가 없으면 빈 문자열
const FString& UAccountSessionSubsystem::GetLoginToken() const
{
    return LoginToken;
}
