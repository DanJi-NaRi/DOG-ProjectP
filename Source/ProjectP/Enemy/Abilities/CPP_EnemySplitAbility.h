// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "CPP_EnemySplitAbility.generated.h"

class ACPP_EnemyBase;

////////////////////////////
//! \class UCPP_EnemySplitAbility
//! \brief 분열(MON_PUN_001_PAT_03): 원본을 숨기고, 맵 중앙 마커 좌우에 현재 HP 절반짜리 분신 2기를 소환한다.
//!        분신은 SplitDuration 동안 분열 외 모든 패턴으로 자율 전투하고, (시간 경과 또는 둘 다 사망 시) 소멸한다.
//!        원본은 마커 위치에 복귀하며 현재 HP = 두 분신의 남은 HP 합. 합이 0이면 사망, 아니면 0.5초 무방비 후 종료.
//! \note  중앙 마커는 스포너 소환 대응을 위해 레벨의 액터 태그(CenterMarkerTag)로 런타임 조회한다(미발견 시 자기 위치).
//!        분신은 SetAbilitySpawnedMinion(true)로 분열 금지·보상 제외. 원본은 숨김+콜리전off라 소멸 동안 피격 불가.
UCLASS()
class PROJECTP_API UCPP_EnemySplitAbility : public UMyGameplayAbilityBase
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) const override;

	void FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar);

protected:
	//! \brief 분열 예고 연출 시간(초). 이 동안 원본은 보인 채 정지(피격 가능=버스트로 저지 가능), 이후 숨김+분신 스폰. 0이면 즉시.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split", meta = (ClampMin = "0.0"))
	float SplitWindupDuration = 0.3f;

	//! \brief 병합 등장 연출 시간(초). 분신 소멸 후 이 동안 뜸(모으기 연출), 이후 원본 등장. 0이면 즉시.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split", meta = (ClampMin = "0.0"))
	float MergeWindupDuration = 0.4f;

	//! \brief 분신 전투 지속 시간(초). 이후 병합.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split", meta = (ClampMin = "0.0"))
	float SplitDuration = 10.0f;

	//! \brief 병합 복귀 후 무방비(피격 가능) 대기 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split", meta = (ClampMin = "0.0"))
	float ReturnStaggerDuration = 0.5f;

	//! \brief 중앙 마커 기준 분신 좌우 간격(cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split", meta = (ClampMin = "0.0"))
	float CloneSideSpacing = 400.0f;

	//! \brief 맵 중앙 마커를 찾을 액터 태그. 레벨에 배치한 TargetPoint 등에 이 태그를 부여한다. 미발견 시 자기 위치 폴백.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split")
	FName CenterMarkerTag = TEXT("SplitCenter");

	//! \brief 분열/복귀 연출용 GameplayCue 태그(선택).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split|Cue")
	FGameplayTag SplitCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split|Cue")
	FGameplayTag ReturnCueTag;

private:
	UFUNCTION()
	void HandleSplitWindupElapsed();

	UFUNCTION()
	void HandleSplitDurationElapsed();

	UFUNCTION()
	void HandleCloneDied();

	UFUNCTION()
	void HandleMergeWindupElapsed();

	UFUNCTION()
	void HandleReturnStaggerElapsed();

	void Merge();
	void FinishSplit();
	ACPP_EnemyBase* SpawnClone(const FVector& SpawnLocation, const FRotator& SpawnRotation, float CloneHealth);
	FVector GetCenterLocation(FRotator& OutMarkerRotation) const;
	void DespawnRemainingClones();
	void SetOriginalVanished(ACPP_EnemyBase* EnemyAvatar, bool bVanished);
	void ExecuteCosmeticCue(const FGameplayTag& CueTag) const;
	ACPP_EnemyBase* GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const;

	mutable FGameplayTagContainer PatternCooldownTags;
	FGameplayAbilitySpecHandle ActiveSpecHandle;
	FGameplayAbilityActivationInfo ActiveActivationInfo;
	TArray<TWeakObjectPtr<ACPP_EnemyBase>> Clones;
	float OriginalMaxHealth = 0.0f;
	float PendingMergeHealth = 0.0f;
	bool bMerged = false;
	bool bOriginalVanished = false;
};
