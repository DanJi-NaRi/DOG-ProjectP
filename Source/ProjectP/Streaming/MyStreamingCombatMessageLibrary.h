////////////////////////////
//! \page MyStreamingCombatMessageLibrary.h
//! \brief 전투 로직이 스트리밍 Combat 채널로 Payload를 발행할 때 사용하는 헬퍼 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MyStreamingPayloads.h"
#include "MyStreamingCombatMessageLibrary.generated.h"

struct FGameplayEffectSpec;

////////////////////////////
//! \class UMyStreamingCombatMessageLibrary
//! \author 장효제
//! \brief GAS/전투 코드가 Streaming Manager를 직접 참조하지 않고 Combat Payload만 발행하도록 돕는다.
UCLASS()
class PROJECTP_API UMyStreamingCombatMessageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Streaming|Combat", meta = (WorldContext = "WorldContextObject"))
	static void BroadcastCombatPayload(const UObject* WorldContextObject, const FMyStreamingCombatPayload& Payload);

	UFUNCTION(BlueprintCallable, Category = "Streaming|Combat", meta = (WorldContext = "WorldContextObject"))
	static void BroadcastSkillUsed(const UObject* WorldContextObject, const AActor* InstigatorActor, FGameplayTag SkillTag);

	UFUNCTION(BlueprintPure, Category = "Streaming|Combat")
	static FGameplayTag ResolveStreamingActorTag(const AActor* Actor);

	////////////////////////////

	static FGameplayTag ResolveStreamingSkillTagFromEffectSpec(const FGameplayEffectSpec& EffectSpec);
};
