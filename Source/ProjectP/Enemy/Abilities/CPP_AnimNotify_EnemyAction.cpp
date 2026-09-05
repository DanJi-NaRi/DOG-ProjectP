// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_AnimNotify_EnemyAction.h"

#include "Enemy/Core/CPP_EnemyBase.h"

////////////////////////////
//! \author HanSeul
//! \brief 적 공격 몽타주가 이 노티파이에 도달하면 Action에 지정된 활성 어빌리티 훅을 발동한다. (서버 전용)
//! \param MeshComp Skeletal mesh component that received the notify.
//! \param Animation Animation asset that owns this notify.
//! \param EventReference Notify event context.
//! \return None
void UCPP_AnimNotify_EnemyAction::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	ACPP_EnemyBase* Enemy = Cast<ACPP_EnemyBase>(MeshComp->GetOwner());
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}

	switch (Action)
	{
	case EEnemyAnimAction::FireProjectile:
		Enemy->FirePrimaryProjectileFromAnimNotify();
		break;
	case EEnemyAnimAction::TriggerArea:
		Enemy->TriggerPrimaryAreaFromAnimNotify();
		break;
	case EEnemyAnimAction::StartDash:
		Enemy->StartDashAttackFromAnimNotify();
		break;
	case EEnemyAnimAction::Summon:
		Enemy->TriggerSummonFromAnimNotify();
		break;
	default:
		break;
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 에디터 노티파이 트랙에 선택된 Action 이름을 표시한다(예: "EnemyAction: Start Dash Attack").
//! \param None
//! \return 노티파이 트랙에 표시할 이름 문자열.
FString UCPP_AnimNotify_EnemyAction::GetNotifyName_Implementation() const
{
	FString ActionName;
	switch (Action)
	{
	case EEnemyAnimAction::FireProjectile:
		ActionName = TEXT("Fire Projectile");
		break;
	case EEnemyAnimAction::TriggerArea:
		ActionName = TEXT("Trigger Area");
		break;
	case EEnemyAnimAction::StartDash:
		ActionName = TEXT("Start Dash Attack");
		break;
	case EEnemyAnimAction::Summon:
		ActionName = TEXT("Summon");
		break;
	default:
		ActionName = TEXT("Unset");
		break;
	}

	return FString::Printf(TEXT("EnemyAction: %s"), *ActionName);
}
