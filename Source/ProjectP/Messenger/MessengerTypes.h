// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MessengerTypes.generated.h"

/**
 * 메신저 채널 구분
 * All   : 전체 채널 (현재 접속 서버의 모든 유저에게 전달)
 * Party : 파티 채널 (현재 파티 멤버에게만 전달)
 */
UENUM(BlueprintType)
enum class EMessengerChannel : uint8
{
    All     UMETA(DisplayName = "전체"),
    Party   UMETA(DisplayName = "파티")
};

/**
 * 메신저 메시지 1건 데이터.
 * 서버에서 시각/이름/캐릭터명을 채워 생성한 뒤, Client RPC로 대상 클라에 전달한다.
 * 모든 필드는 기본 net 직렬화가 되므로 RPC 파라미터로 그대로 사용 가능하다.
 */
USTRUCT(BlueprintType)
struct FMessengerMessage
{
    GENERATED_BODY()

public:
    // 메시지가 속한 채널 (전체/파티)
    UPROPERTY(BlueprintReadWrite, Category = "Messenger")
    EMessengerChannel Channel = EMessengerChannel::All;

    // 서버 기준 시각 문자열 "hh:mm"
    UPROPERTY(BlueprintReadWrite, Category = "Messenger")
    FString TimeText;

    // 보낸 유저의 DB 이름 (username)
    UPROPERTY(BlueprintReadWrite, Category = "Messenger")
    FString Username;

    // 던전에서만 채워지는 캐릭터명 (Nefer/Inpu/Heru), 로비에서는 빈 문자열
    UPROPERTY(BlueprintReadWrite, Category = "Messenger")
    FString CharacterName;

    // 메시지 본문
    UPROPERTY(BlueprintReadWrite, Category = "Messenger")
    FString Body;

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // 메시지 필드를 최종 표시 문자열로 조합한다.
    // CharacterName 유무에 따라 "[hh:mm] username : Body" 또는
    // "[hh:mm] username(CharacterName) : Body" 형태로 분기한다.
    // 반환값 : 화면에 그대로 출력 가능한 한 줄 문자열
    FString ToDisplayString() const
    {
        if (CharacterName.IsEmpty())
        {
            // 로비 등 캐릭터명이 없는 경우
            return FString::Printf(TEXT("[%s] %s : %s"), *TimeText, *Username, *Body);
        }

        // 던전 등 캐릭터명이 있는 경우
        return FString::Printf(TEXT("[%s] %s(%s) : %s"), *TimeText, *Username, *CharacterName, *Body);
    }
};

/**
 * 메신저 공용 헬퍼 모음.
 */
namespace MessengerUtil
{
    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // CharacterId를 캐릭터 표시 이름으로 변환한다.
    // CharacterId : 선택한 캐릭터 ID (100=Nefer, 200=Inpu, 300=Heru)
    // 반환값 : 매칭되는 캐릭터명, 매칭되지 않으면 빈 문자열
    inline FString GetCharacterDisplayName(int32 CharacterId)
    {
        switch (CharacterId)
        {
        case 100: return TEXT("Nefer");
        case 200: return TEXT("Inpu");
        case 300: return TEXT("Heru");
        default:  return FString();
        }
    }
}
