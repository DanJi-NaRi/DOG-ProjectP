// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_EnemySummonAbility.h"

#include "Enemy/Abilities/CPP_EnemyAttackPatternData.h"
#include "Enemy/Core/CPP_EnemyBase.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "MyGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

////////////////////////////
//! \author HanUl
//! \brief 소환 발동: 소환 몽타주를 재생하고, 소환 노티파이 타이밍에 실제 소환한다(TriggerSummonFromNotify).
//!        몽타주가 없으면 즉시 소환 폴백. 게이트는 패턴 선택 단계에서 이미 통과.
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
//! \param TriggerEventData Optional trigger payload.
//! \return
void UCPP_EnemySummonAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(ActorInfo);
	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar ? EnemyAvatar->GetPrimaryAttackPattern() : nullptr;
	if (!EnemyAvatar || !EnemyAvatar->HasAuthority() || !AttackPattern || AttackPattern->SummonEnemyClasses.Num() == 0)
	{
		if (EnemyAvatar)
		{
			EnemyAvatar->FinishPrimaryAttackFromAbility();
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveSpecHandle = Handle;
	ActiveActivationInfo = ActivationInfo;
	bHasSummoned = false;
	EnemyAvatar->SetActiveSummonAbility(this);

	// 소환 몽타주 재생 → 소환 노티파이에서 실제 소환. 몽타주가 없으면 즉시 소환 폴백 후 종료.
	if (EnemyAvatar->PlayPrimaryAttackMontageFromAbility(EnemyAvatar->GetCurrentTargetActor()))
	{
		return;
	}

	DoSummon(EnemyAvatar);
	EnemyAvatar->ClearActiveSummonAbility(this);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	EnemyAvatar->FinishPrimaryAttackFromAbility();
}

////////////////////////////
//! \author HanUl
//! \brief 소환 노티파이 도달: 쿨다운을 커밋하고 실제 소환을 수행한다. 몽타주 종료 시 공통 경로가 어빌리티를 닫는다.
//! \param Summoner 소환자
//! \return 소환을 수행했으면 true
bool UCPP_EnemySummonAbility::TriggerSummonFromNotify(ACPP_EnemyBase* Summoner)
{
	if (bHasSummoned || !Summoner)
	{
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !CommitAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo))
	{
		return false;
	}

	return DoSummon(Summoner);
}

////////////////////////////
//! \author HanUl
//! \brief 생존 플레이어 전원 주변에 소환을 실행한다(1회). 연출 큐 재생 포함. 쿨다운 커밋은 호출측 책임.
//! \param Summoner 소환자
//! \return true
bool UCPP_EnemySummonAbility::DoSummon(ACPP_EnemyBase* Summoner)
{
	bHasSummoned = true;

	if (SummonCueTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->ExecuteGameplayCue(SummonCueTag, ASC->MakeEffectContext());
		}
	}

	TArray<AActor*> LivingPlayers;
	UMyAbilitySystemLibrary::GetLivingPlayerPawns(Summoner, LivingPlayers);
	for (AActor* PlayerActor : LivingPlayers)
	{
		SpawnMinionsAroundPlayer(Summoner, PlayerActor);
	}
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 한 플레이어 주변 반경 내 무작위 위치에 SummonsPerPlayer마리를 소환한다. 무작위 클래스 + 공격력 스케일 +
//!        파생 적 플래그 + 소환자 등록. 위치는 내비메시에 투영해 벽 너머/절벽을 막는다.
//! \param Summoner 소환자
//! \param PlayerActor 기준 플레이어
//! \return
void UCPP_EnemySummonAbility::SpawnMinionsAroundPlayer(ACPP_EnemyBase* Summoner, AActor* PlayerActor)
{
	const UCPP_EnemyAttackPatternData* AttackPattern = Summoner->GetPrimaryAttackPattern();
	UWorld* World = Summoner->GetWorld();
	if (!IsValid(PlayerActor) || !AttackPattern || !World || AttackPattern->SummonEnemyClasses.Num() == 0)
	{
		return;
	}

	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const FVector PlayerLocation = PlayerActor->GetActorLocation();

	for (int32 i = 0; i < AttackPattern->SummonsPerPlayer; ++i)
	{
		const int32 ClassIndex = FMath::RandRange(0, AttackPattern->SummonEnemyClasses.Num() - 1);
		TSubclassOf<ACPP_EnemyBase> SummonClass = AttackPattern->SummonEnemyClasses[ClassIndex];
		if (!SummonClass)
		{
			continue;
		}

		// 플레이어 주변 반경 내 무작위 지점 → 내비 투영.
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Distance = FMath::FRandRange(0.0f, AttackPattern->SummonRadius);
		FVector SpawnLocation = PlayerLocation + FVector(Distance * FMath::Cos(Angle), Distance * FMath::Sin(Angle), 0.0f);
		if (NavSys)
		{
			FNavLocation Projected;
			if (NavSys->ProjectPointToNavigation(SpawnLocation, Projected, FVector(400.0f, 400.0f, 250.0f)))
			{
				SpawnLocation = Projected.Location;
				if (const ACPP_EnemyBase* SummonCDO = SummonClass.GetDefaultObject())
				{
					if (const UCapsuleComponent* Capsule = SummonCDO->GetCapsuleComponent())
					{
						SpawnLocation.Z += Capsule->GetScaledCapsuleHalfHeight();
					}
				}
			}
		}

		const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);
		ACPP_EnemyBase* Minion = World->SpawnActorDeferred<ACPP_EnemyBase>(
			SummonClass, SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Minion)
		{
			continue;
		}

		Minion->SetEnemyLevel(Summoner->GetEnemyLevel());
		Minion->ConfigureSpawnTarget(
			Summoner->GetSpawnTargetPolicy(),
			Summoner->GetAssignedObjectiveTarget());
		Minion->SetAbilitySpawnedMinion(true); // BeginPlay 전 세팅 → 보상 제외/재소환 금지 보장
		UGameplayStatics::FinishSpawningActor(Minion, SpawnTransform);

		Minion->SetAttackPowerScale(AttackPattern->SummonAttackPowerScale);
		Summoner->RegisterSummonedMinion(Minion);
	}
}

const FGameplayTagContainer* UCPP_EnemySummonAbility::GetCooldownTags() const
{
	PatternCooldownTags.Reset();

	if (const ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo()))
	{
		if (const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar->GetPrimaryAttackPattern())
		{
			if (AttackPattern->CooldownTag.IsValid())
			{
				PatternCooldownTags.AddTag(AttackPattern->CooldownTag);
			}
		}
	}

	return &PatternCooldownTags;
}

////////////////////////////
//! \author HanUl
//! \brief 활성 공격 패턴이 정의한 쿨다운 태그/시간을 적용한다. (다른 적 공격 어빌리티와 동일 규칙)
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
//! \return
void UCPP_EnemySummonAbility::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo
) const
{
	const UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(ActorInfo);
	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar ? EnemyAvatar->GetPrimaryAttackPattern() : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!CooldownGE || !AttackPattern || !AttackPattern->CooldownTag.IsValid() || AttackPattern->CooldownDuration <= 0.0f || !ASC)
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CooldownGE->GetClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->DynamicGrantedTags.AddTag(AttackPattern->CooldownTag);
	SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Cooldown, AttackPattern->CooldownDuration);
	SpecHandle.Data->SetDuration(AttackPattern->CooldownDuration, true);
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

////////////////////////////
//! \author HanUl
//! \brief 몽타주 종료(정상) 또는 외부 강제 종료(사망 등) 시 어빌리티를 닫는다. 이미 소환된 적은 독립적으로 유지된다.
//!        (정상 종료 시 공격 종료 통지는 호출측 FinishAttackMontage가 담당)
//! \param EnemyAvatar Enemy that owns this active ability.
//! \return
void UCPP_EnemySummonAbility::FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar)
{
	if (EnemyAvatar)
	{
		EnemyAvatar->ClearActiveSummonAbility(this);
	}

	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		EndAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo, true, false);
	}
}

ACPP_EnemyBase* UCPP_EnemySummonAbility::GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}

	return Cast<ACPP_EnemyBase>(ActorInfo->AvatarActor.Get());
}
