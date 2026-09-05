////////////////////////////
//! \page MyStreamingPayloads.h
//! \brief Streaming System이 수신할 모든 Payload 타입 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MyStreamingPayloads.generated.h"

////////////////////////////
//! \struct FMyStreamingCombatPayload
//! \author 장효제
//! \brief GAS/전투 로직이 스트리밍 시스템에 전달하는 전투 사실 데이터다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingCombatPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Combat", meta = (Categories = "Streaming.Event.Combat"))
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Combat", meta = (Categories = "Character"))
	FGameplayTag InstigatorTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Combat", meta = (Categories = "Character"))
	FGameplayTag TargetTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Combat", meta = (Categories = "Skill"))
	FGameplayTag SkillTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Combat")
	float DamageAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetCurrentHPRatio = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Combat", meta = (DisplayName = "Is Critical"))
	bool bIsCritical = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Combat", meta = (DisplayName = "Is Kill"))
	bool bIsKill = false;
};

////////////////////////////
//! \struct FMyStreamingMesoPayload
//! \author 장효제
//! \brief 경제 시스템(상점과 스트리밍)이 스트리밍 시스템에 전달하는 전투 사실 데이터다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingMesoPayload
{ 
	GENERATED_BODY()

	//! 어떤 종류의 Meso 사건인지 나타낸다.
	//! ex) Streaming.Event.Meso.Earned / Streaming.Event.Meso.Spent
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Meso", meta = (Categories = "Streaming.Event.Meso"))
	FGameplayTag EventTag;

	//! Meso 변화가 발생한 출처를 나타낸다.
	//! ex) Meso.Source.CombatReward / Meso.Source.Streaming.Donation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Meso", meta = (Categories = "Meso.Source"))
	FGameplayTag SourceTag;

	//! Meso가 변화한 플레이어의 UserIndex.
	//! Standalone 등의 경우 -1을 허용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Meso")
	int32 UserIndex = -1;

	//! 실제로 적용된 Meso 변화량.
	//! 양수: 획득 / 음수: 소비
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Meso")
	int32 AppliedDelta = 0;

	//! 변화 적용 직후의 최종 Meso 보유량.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Meso")
	int32 CurrentMeso = 0;
};

////////////////////////////
//! \struct FMyStreamingPlayerInputPayload
//! \author 장효제
//! \brief 플레이어가 무언가를 조작했다는 최소 사실이다.
//! \details 잠수 판정은 "적을 때렸나"가 아니라 "조작했나"를 재야 한다.
//!          이동·마우스는 프레임마다 들어오므로 클라이언트가 눌러서 보낸다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingPlayerInputPayload
{
	GENERATED_BODY()

	//! 조작한 플레이어의 UserIndex다. 확인되지 않으면 -1이다.
	UPROPERTY(BlueprintReadOnly, Category = "Streaming|PlayerInput")
	int32 UserIndex = -1;
};

namespace MyStreamingPlayerInput
{
	//! \brief 클라이언트가 서버에 조작을 알릴 최소 간격이다.
	//! \details 잠수 판정이 수십 초 단위라 이 정도 해상도면 충분하다.
	//!          이동·마우스 입력을 그대로 보내면 3인 기준 초당 수백 건이 된다.
	inline constexpr float ReportIntervalSeconds = 3.0f;

	//! \brief 지금 조작을 서버에 보고할 때가 됐는지 판정한다.
	//! \param NowSeconds 현재 시각이다.
	//! \param LastReportedSeconds 마지막으로 보고한 시각이다. 아직 없으면 음수다.
	//! \return 보고해야 하면 true다.
	inline bool ShouldReport(const double NowSeconds, const double LastReportedSeconds)
	{
		return LastReportedSeconds < 0.0
			|| NowSeconds - LastReportedSeconds >= static_cast<double>(ReportIntervalSeconds);
	}

	//! \brief 조작 사실을 스트리밍 채널로 발행한다. 서버에서만 부른다.
	//! \param WorldContextObject GameplayMessageSubsystem World를 찾을 객체다.
	//! \param UserIndex 조작한 플레이어의 UserIndex다.
	PROJECTP_API void BroadcastPlayerInput(const UObject* WorldContextObject, int32 UserIndex);
}

////////////////////////////
//! \struct FMyStreamingCountEventPayload
//! \author 장효제
//! \brief 파티가 어떤 사건을 한 번 겪었다는 최소 사실이다.
//! \details 항아리 파괴·플레이어 사망·도네이션 수령이 이 채널로 온다.
//!          UserIndex를 지목할 수 없거나 지목이 뜻이 없는 사건들이다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingCountEventPayload
{
	GENERATED_BODY()

	//! 무슨 사건인지다.
	UPROPERTY(BlueprintReadOnly, Category = "Streaming|CountEvent")
	FGameplayTag EventTag;

	//! 사건을 좁히는 태그다. 도네이션에서는 준 신을 가리킨다.
	UPROPERTY(BlueprintReadOnly, Category = "Streaming|CountEvent")
	FGameplayTag SourceTag;
};

////////////////////////////
//! \struct FMyStreamingItemEventPayload
//! \author 장효제
//! \brief 플레이어가 아이템을 쓰거나 샀다는 최소 사실이다.
//! \details 인벤토리는 PlayerState 소유라 UserIndex를 알 수 있다.
//!          그래서 사건을 일으킨 개인에게 보상할 수 있다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingItemEventPayload
{
	GENERATED_BODY()

	//! 사용인지 구매인지다.
	UPROPERTY(BlueprintReadOnly, Category = "Streaming|ItemEvent")
	FGameplayTag EventTag;

	//! DT_ItemTable의 RowName이다.
	UPROPERTY(BlueprintReadOnly, Category = "Streaming|ItemEvent")
	FName ItemId;

	//! 사건을 일으킨 플레이어의 UserIndex다. 확인되지 않으면 -1이다.
	UPROPERTY(BlueprintReadOnly, Category = "Streaming|ItemEvent")
	int32 UserIndex = -1;

	//! 한 번에 쓰거나 산 개수다. 누계에 이 값을 더한다.
	UPROPERTY(BlueprintReadOnly, Category = "Streaming|ItemEvent")
	int32 Count = 1;
};

namespace MyStreamingCountEvent
{
	//! \brief 파티 사건 누계 사실을 발행한다. 서버에서만 부른다.
	//! \param WorldContextObject GameplayMessageSubsystem World를 찾을 객체다.
	//! \param EventTag 무슨 사건인지다.
	//! \param SourceTag 사건을 좁히는 태그다. 없으면 비운다.
	PROJECTP_API void BroadcastCountEvent(
		const UObject* WorldContextObject,
		FGameplayTag EventTag,
		FGameplayTag SourceTag);

	//! \brief 아이템 사건 사실을 발행한다. 서버에서만 부른다.
	//! \param WorldContextObject GameplayMessageSubsystem World를 찾을 객체다.
	//! \param EventTag 사용인지 구매인지다.
	//! \param ItemId DT_ItemTable의 RowName이다.
	//! \param UserIndex 사건을 일으킨 플레이어다.
	//! \param Count 한 번에 쓰거나 산 개수다.
	PROJECTP_API void BroadcastItemEvent(
		const UObject* WorldContextObject,
		FGameplayTag EventTag,
		FName ItemId,
		int32 UserIndex,
		int32 Count);
}

////////////////////////////
//! \struct FMyStreamingStatePayload
//! \author 장효제
//! \brief 어떤 상태가 켜지거나 꺼졌다는 최소 사실이다.
//! \details 사실은 한 번 일어나고 끝이지만 상태는 켜짐과 꺼짐이 짝을 이룬다.
//!          발행하는 쪽은 반드시 켜짐과 꺼짐을 짝으로 보내야 한다.
//!          꺼짐을 빠뜨리면 스트리밍이 그 상태를 영원히 켜진 것으로 본다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingStatePayload
{
	GENERATED_BODY()

	//! 어떤 상태인지다.
	UPROPERTY(BlueprintReadOnly, Category = "Streaming|State")
	FGameplayTag StateTag;

	//! 이 상태를 켜거나 끈 객체다. 같은 상태를 켜는 객체가 여럿일 수 있어
	//! 출처를 구분해야 하나가 꺼질 때 나머지까지 꺼지지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Streaming|State")
	TWeakObjectPtr<AActor> SourceActor;

	//! 켜졌으면 true, 꺼졌으면 false다.
	UPROPERTY(BlueprintReadOnly, Category = "Streaming|State")
	bool bEntered = false;
};

namespace MyStreamingMoveWatch
{
	//! 서버가 속도를 보는 주기다. 이동에는 전이 지점이 없어 주기로 볼 수밖에 없다.
	inline constexpr float IntervalSeconds = 0.25f;
	//! 정지에서 이동으로 넘어가는 문턱이다.
	inline constexpr float EnterMovingSpeed = 120.0f;
	//! 이동에서 정지로 넘어가는 문턱이다. 켜지는 문턱보다 낮아야 한다.
	inline constexpr float ExitMovingSpeed = 20.0f;

	//! \brief 지금 속도로 이동 상태를 판정한다.
	//! \details 문턱이 하나면 그 언저리에서 상태가 쉴 새 없이 뒤집힌다.
	//!          두 문턱 사이에서는 지금 상태를 그대로 둔다.
	//! \param Speed 수평 속력이다.
	//! \param bWasMoving 직전에 이동 상태였는지다.
	//! \return 이동 상태로 봐야 하면 true다.
	inline bool ResolveMoving(const float Speed, const bool bWasMoving)
	{
		if (Speed >= EnterMovingSpeed)
		{
			return true;
		}
		if (Speed <= ExitMovingSpeed)
		{
			return false;
		}
		return bWasMoving;
	}
}

namespace MyStreamingState
{
	//! \brief 상태 켜짐·꺼짐 사실을 발행한다. 서버에서만 부른다.
	//! \param SourceActor 상태를 켜거나 끈 객체다. World 문맥이자 출처다.
	//! \param StateTag 어떤 상태인지다.
	//! \param bEntered 켜졌으면 true, 꺼졌으면 false다.
	PROJECTP_API void BroadcastState(
		AActor* SourceActor,
		FGameplayTag StateTag,
		bool bEntered);
}
