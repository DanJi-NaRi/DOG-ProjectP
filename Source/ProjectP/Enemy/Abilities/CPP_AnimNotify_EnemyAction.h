// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "CPP_AnimNotify_EnemyAction.generated.h"

////////////////////////////
//! \enum EEnemyAnimAction
//! \brief 공용 적 노티파이가 몽타주 도달 시 발동할 적 공격 액션 종류. (기존 개별 노티파이 4종을 대체)
UENUM(BlueprintType)
enum class EEnemyAnimAction : uint8
{
	FireProjectile UMETA(DisplayName = "Fire Projectile"),
	TriggerArea    UMETA(DisplayName = "Trigger Area"),
	StartDash      UMETA(DisplayName = "Start Dash Attack"),
	Summon         UMETA(DisplayName = "Summon")
};

////////////////////////////
//! \class UCPP_AnimNotify_EnemyAction
//! \brief 적 공격 몽타주가 이 노티파이에 도달하면 Action에 지정된 활성 어빌리티 훅을 발동한다. (서버 전용)
//!        개별 노티파이(FireProjectile/SpawnArea/StartDashAttack/Summon) 4종을 하나로 통합한 공용 노티파이.
UCLASS()
class PROJECTP_API UCPP_AnimNotify_EnemyAction : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	//! \brief 에디터 노티파이 트랙에 선택된 Action 이름을 표시한다(예: "EnemyAction: Start Dash Attack").
	virtual FString GetNotifyName_Implementation() const override;

protected:
	//! \brief 이 노티파이가 발동할 적 공격 액션. 몽타주마다 알맞은 값으로 지정해야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Attack")
	EEnemyAnimAction Action = EEnemyAnimAction::FireProjectile;
};
