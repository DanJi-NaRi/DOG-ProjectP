////////////////////////////
//! \page MyAbilitySystemLibrary.h
//! \brief MyGAS AbilitySystem 접근과 SetByCaller 값 할당 유틸리티 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "CollisionQueryParams.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MyAbilitySystemLibrary.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

////////////////////////////
//! \class UMyAbilitySystemLibrary
//! \author 장효제
//! \brief MyGAS AbilitySystemComponent 조회와 GameplayEffect SetByCaller 값 할당을 제공하는 Blueprint 함수 라이브러리다.
UCLASS()
class PROJECTP_API UMyAbilitySystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "MyGAS|AbilitySystem")
	static UAbilitySystemComponent* GetAbilitySystemComponentFromActor(const AActor* Actor);


	UFUNCTION(BlueprintCallable, Category = "MyGAS|Ability")
	static bool TryActivateAbilityByInputTag(UAbilitySystemComponent* ASC, FGameplayTag InputTag);

	////////////////////////////
	//! \brief 플레이어 공격이 검색할 Pawn과 Destructible Object Type 조건을 생성한다.
	//! \return ECC_Pawn과 ECC_Destructible이 등록된 Object Query 조건
	static FCollisionObjectQueryParams MakePlayerAttackObjectQuery();

	////////////////////////////
	//! \brief ASC에 부여된 Ability 중 InputTag와 일치하는 Ability를 GameplayEventData payload와 함께 발동한다.
	//! \param ASC Ability를 보유한 AbilitySystemComponent
	//! \param InputTag 발동할 Ability를 찾는 입력 GameplayTag
	//! \param EventData Ability ActivateAbility에 전달할 GameplayEventData payload
	//! \return Ability 발동 요청에 성공하면 true, 차단되거나 실패하면 false
	UFUNCTION(BlueprintCallable, Category = "MyGAS|Ability")
	static bool TryActivateAbilityByInputTagWithEventData(UAbilitySystemComponent* ASC, FGameplayTag InputTag, const FGameplayEventData& EventData);

	UFUNCTION(BlueprintCallable, Category = "MyGAS|Ability")
	static bool TryActivateChargeAbilityByInputTag(UAbilitySystemComponent* ASC, FGameplayTag InputTag);


	UFUNCTION(BlueprintCallable, Category = "MyGAS|SetByCaller")
	static bool AssignSetByCallerDamage(UPARAM(ref) FGameplayEffectSpecHandle& SpecHandle, float Damage);

	////////////////////////////
	//! \brief GameplayEffectSpecHandle에 Data.Coefficient SetByCaller 크기를 할당한다.
	//!        ExecCalc가 캡처된 Source AttackPower x 계수로 기본 피해량을 계산한다.
	//! \param SpecHandle 값을 적용할 GameplayEffectSpecHandle
	//! \param Coefficient 할당할 스킬 피해 계수
	//! \return 값 할당에 성공하면 true, SpecHandle이 유효하지 않으면 false
	UFUNCTION(BlueprintCallable, Category = "MyGAS|SetByCaller")
	static bool AssignSetByCallerCoefficient(UPARAM(ref) FGameplayEffectSpecHandle& SpecHandle, float Coefficient);

	UFUNCTION(BlueprintCallable, Category = "MyGAS|SetByCaller")
	static bool AssignSetByCallerHeal(UPARAM(ref) FGameplayEffectSpecHandle& SpecHandle, float Heal);

	UFUNCTION(BlueprintCallable, Category = "MyGAS|SetByCaller")
	static bool AssignSetByCallerShield(UPARAM(ref) FGameplayEffectSpecHandle& SpecHandle, float Shield);

	UFUNCTION(BlueprintCallable, Category = "MyGAS|GameplayEffect")
	static bool ApplySetByCallerDamageEffectToTargetActor(
		UAbilitySystemComponent* SourceASC,
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		float Damage,
		float Level = 1.0f,
		float CurseGaugeAmount = 0.0f
	);

	////////////////////////////
	//! \brief Source ASC가 TargetActor의 ASC에 Data.Coefficient SetByCaller GameplayEffect를 적용한다.
	//!        공격력 곱셈은 ExecCalc가 담당하므로 콜사이트는 스킬 계수만 넘기면 된다.
	//! \param SourceASC GameplayEffect Spec을 생성하고 적용을 요청할 Source ASC
	//! \param TargetActor Damage GameplayEffect를 받을 Actor
	//! \param DamageEffectClass UMyDamageExecutionCalculation을 사용하는 GameplayEffect class
	//! \param Coefficient 스킬 피해 계수(Source AttackPower에 곱해짐)
	//! \param Level GameplayEffect Spec level
	//! \return 적용 요청을 만들 수 있으면 true, 필수 입력이 없으면 false
	UFUNCTION(BlueprintCallable, Category = "MyGAS|GameplayEffect")
	static bool ApplyCoefficientDamageEffectToTargetActor(
		UAbilitySystemComponent* SourceASC,
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		float Coefficient,
		float Level = 1.0f,
		float CurseGaugeAmount = 0.0f
	);

	////////////////////////////
	//! \brief 계수 피해에 스킬 쿨다운 태그를 꼬리표(DynamicAssetTag)로 실어 적용한다.
	//!        대상 사망 시 처치 스킬 식별(처치 시 쿨 초기화 등)에 사용된다.
	//! \param SourceASC GameplayEffect Spec을 생성하고 적용을 요청할 Source ASC
	//! \param TargetActor Damage GameplayEffect를 받을 Actor
	//! \param DamageEffectClass UMyDamageExecutionCalculation을 사용하는 GameplayEffect class
	//! \param Coefficient 스킬 피해 계수(Source AttackPower에 곱해짐)
	//! \param SkillCooldownTag Spec에 실을 스킬 쿨다운 태그(무효면 꼬리표 생략)
	//! \param Level GameplayEffect Spec level
	//! \return 적용 요청을 만들 수 있으면 true, 필수 입력이 없으면 false
	UFUNCTION(BlueprintCallable, Category = "MyGAS|GameplayEffect")
	static bool ApplySkillCoefficientDamageEffectToTargetActor(
		UAbilitySystemComponent* SourceASC,
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		float Coefficient,
		FGameplayTag SkillCooldownTag,
		float Level = 1.0f,
		float CurseGaugeAmount = 0.0f
	);

	////////////////////////////
	//! \brief 플레이어 스킬 계수 피해에 쿨다운 태그와 공격자 적중 카메라 피드백 태그를 함께 실어 적용한다.
	//! \param SourceASC GameplayEffect Spec을 생성하고 적용을 요청할 Source ASC
	//! \param TargetActor Damage GameplayEffect를 받을 Actor
	//! \param DamageEffectClass UMyDamageExecutionCalculation을 사용하는 GameplayEffect class
	//! \param Coefficient 스킬 피해 계수(Source AttackPower에 곱해짐)
	//! \param SkillCooldownTag Spec에 실을 스킬 쿨다운 태그
	//! \param SkillInputTag Basic/Q/E/R/C를 구분할 스킬 입력 태그
	//! \param Level GameplayEffect Spec level
	//! \return 적용 요청을 만들 수 있으면 true, 필수 입력이 없으면 false
	static bool ApplyPlayerSkillCoefficientDamageEffectToTargetActor(
		UAbilitySystemComponent* SourceASC,
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		float Coefficient,
		FGameplayTag SkillCooldownTag,
		FGameplayTag SkillInputTag,
		float Level = 1.0f,
		float CurseGaugeAmount = 0.0f
	);

	////////////////////////////
	//! \brief 스킬 입력 태그를 Basic/Skill/Ultimate 공격자 적중 카메라 피드백 태그로 변환한다.
	//! \param SkillInputTag 분류할 스킬 입력 태그
	//! \return 대응하는 카메라 피드백 태그, Move 또는 미지원 입력이면 무효 태그
	UFUNCTION(BlueprintPure, Category = "MyGAS|CameraFeedback")
	static FGameplayTag ResolveAttackerHitCameraFeedbackTag(FGameplayTag SkillInputTag);

	////////////////////////////
	//! \brief 유효한 스킬 입력 태그의 공격자 적중 카메라 피드백 태그를 EffectSpec에 추가한다.
	//! \param SpecHandle 태그를 추가할 GameplayEffectSpecHandle
	//! \param SkillInputTag Basic/Q/E/R/C를 구분할 스킬 입력 태그
	//! \return 피드백 태그를 추가했으면 true
	static bool AddAttackerHitCameraFeedbackTag(
		UPARAM(ref) FGameplayEffectSpecHandle& SpecHandle,
		FGameplayTag SkillInputTag
	);

	////////////////////////////
	//! \brief Effect AssetTag 목록에서 지원하는 공격자 적중 카메라 피드백 태그를 찾는다.
	//! \param EffectAssetTags GameplayEffect의 AssetTag 목록
	//! \return 발견한 피드백 태그, 없으면 무효 태그
	static FGameplayTag FindAttackerHitCameraFeedbackTag(const FGameplayTagContainer& EffectAssetTags);

	////////////////////////////
	//! \brief 쿨다운 태그를 부여한 활성 GameplayEffect의 남은 시간을 반환한다.
	//!        GE가 복제되는 클라이언트(소유자)에서도 동작하므로 UI 표시에 사용할 수 있다.
	//! \param ASC 조회할 AbilitySystemComponent
	//! \param CooldownTag 쿨다운 GameplayTag
	//! \return 남은 쿨다운(초). 쿨다운이 없거나 끝났으면 0
	UFUNCTION(BlueprintPure, Category = "MyGAS|Cooldown")
	static float GetCooldownRemainingByTag(const UAbilitySystemComponent* ASC, FGameplayTag CooldownTag);

	////////////////////////////
	//! \brief 쿨다운 태그를 부여한 활성 쿨다운 GameplayEffect의 남은 시간을 지정 초만큼 단축한다.
	//!        남은 시간을 조회해 GE를 제거하고 (남은시간 - 단축시간)으로 재적용한다. 서버에서만 호출해야 한다.
	//! \param ASC 대상 AbilitySystemComponent
	//! \param CooldownTag 쿨다운 GameplayTag
	//! \param ReduceSeconds 단축할 시간(초, 0 이하 무시)
	//! \return 단축 후 남은 쿨다운(초). 쿨다운이 없었거나 전부 소진되면 0
	UFUNCTION(BlueprintCallable, Category = "MyGAS|Cooldown")
	static float ReduceCooldownByTag(UAbilitySystemComponent* ASC, FGameplayTag CooldownTag, float ReduceSeconds);

	////////////////////////////
	//! \brief 부여 태그로 활성 GameplayEffect를 찾아 스택 수 합을 반환한다.
	//!        태그 카운트가 아닌 GE 스택 수를 읽으므로 스태킹 GE(표식 등) 조회에 사용한다.
	//! \param ASC 조회할 AbilitySystemComponent
	//! \param GrantedTag 스택 GE가 부여하는 GameplayTag
	//! \return 활성 스택 수 합, 없으면 0
	UFUNCTION(BlueprintPure, Category = "MyGAS|Stack")
	static int32 GetStackCountByGrantedTag(const UAbilitySystemComponent* ASC, FGameplayTag GrantedTag);

	////////////////////////////
	//! \brief 부여 태그로 활성 스택 GameplayEffect를 전량 소비(제거)하고 소비한 스택 수를 반환한다.
	//! \param ASC 대상 AbilitySystemComponent
	//! \param GrantedTag 스택 GE가 부여하는 GameplayTag
	//! \return 소비한 스택 수, 없었으면 0
	UFUNCTION(BlueprintCallable, Category = "MyGAS|Stack")
	static int32 ConsumeStacksByGrantedTag(UAbilitySystemComponent* ASC, FGameplayTag GrantedTag);

	////////////////////////////
	//! \brief Source 기준으로 Target이 적대 진영(Faction 태그)인지 판정한다.
	//! \param SourceActor 판정 기준 Actor
	//! \param TargetActor 판정 대상 Actor
	//! \param bRequireAlive true면 Target의 Health가 0 이하일 때 false를 반환한다
	//! \return 두 Actor의 Faction 태그가 서로 적대 관계이면 true
	//! \note 플레이어가 Target.Destructible 대상을 검사하는 경우에도 공격 가능한 대상으로 반환한다.
	UFUNCTION(BlueprintPure, Category = "MyGAS|Faction")
	static bool IsHostile(const AActor* SourceActor, const AActor* TargetActor, bool bRequireAlive = true);

	////////////////////////////
	//! \brief Source 기준으로 Target이 같은 진영(Faction 태그)인지 판정한다. 자기 자신도 아군으로 취급한다.
	//! \param SourceActor 판정 기준 Actor
	//! \param TargetActor 판정 대상 Actor
	//! \param bRequireAlive true면 Target의 Health가 0 이하일 때 false를 반환한다
	//! \return 두 Actor의 Faction 태그가 같은 진영이면 true
	UFUNCTION(BlueprintPure, Category = "MyGAS|Faction")
	static bool IsFriendly(const AActor* SourceActor, const AActor* TargetActor, bool bRequireAlive = true);

	////////////////////////////
	//! \brief Actor가 살아있는지(Health 속성 > 0) 여부를 반환한다.
	//! \param Actor 검사할 Actor
	//! \return ASC가 있고 Health가 0보다 크면 true
	UFUNCTION(BlueprintPure, Category = "MyGAS|Player")
	static bool IsLivingPawn(const AActor* Actor);

	////////////////////////////
	//! \brief 월드의 모든 PlayerController가 소유한 살아있는 Pawn을 수집한다.
	//! \param WorldContextObject 월드 접근용 컨텍스트 오브젝트
	//! \param OutLivingPlayers 살아있는 플레이어 Pawn 배열(출력)
	UFUNCTION(BlueprintCallable, Category = "MyGAS|Player", meta = (WorldContext = "WorldContextObject"))
	static void GetLivingPlayerPawns(const UObject* WorldContextObject, TArray<AActor*>& OutLivingPlayers);

	////////////////////////////
	//! \brief 살아있는 플레이어 Pawn 중 무작위로 하나를 반환한다.
	//! \param WorldContextObject 월드 접근용 컨텍스트 오브젝트
	//! \return 무작위 살아있는 플레이어 Pawn, 없으면 nullptr
	UFUNCTION(BlueprintCallable, Category = "MyGAS|Player", meta = (WorldContext = "WorldContextObject"))
	static AActor* GetRandomLivingPlayer(const UObject* WorldContextObject);

	////////////////////////////
	//! \brief Origin에서 수평 거리상 가장 가까운 살아있는 플레이어 Pawn을 반환한다.
	//! \param WorldContextObject 월드 접근용 컨텍스트 오브젝트
	//! \param Origin 거리 계산 기준 위치
	//! \return 가장 가까운 살아있는 플레이어 Pawn, 없으면 nullptr
	UFUNCTION(BlueprintCallable, Category = "MyGAS|Player", meta = (WorldContext = "WorldContextObject"))
	static AActor* GetNearestLivingPlayer(const UObject* WorldContextObject, const FVector& Origin);
};
