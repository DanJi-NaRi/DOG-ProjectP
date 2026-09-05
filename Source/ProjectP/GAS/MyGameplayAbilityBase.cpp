////////////////////////////
//! \page MyGameplayAbilityBase.cpp
//! \brief MyGAS GameplayAbility 기반 클래스의 태그 조회와 SetByCaller 편의 함수를 구현한다.
#include "MyGameplayAbilityBase.h"

#include "MyAbilitySystemLibrary.h"
#include "Streaming/MyStreamingCombatMessageLibrary.h"

////////////////////////////
//! \author 장효제
//! \brief 어빌리티 인스턴싱 정책을 Actor별 인스턴스 방식으로 초기화한다.
UMyGameplayAbilityBase::UMyGameplayAbilityBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

////////////////////////////
//! \author 장효제
//! \brief 어빌리티가 켜지면 스킬 사용 사실을 서버에서 한 번 발행한다.
//! \param Handle 어빌리티 Spec 핸들이다.
//! \param ActorInfo 어빌리티 소유자 정보다.
//! \param ActivationInfo 활성화 정보다. 예측 실행과 서버 실행을 구분한다.
//! \param TriggerEventData 트리거 이벤트 데이터다.
//! \return 없음
void UMyGameplayAbilityBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 클라이언트 예측 실행에서도 이 함수는 돌아간다. 사실은 서버가 한 번만 확정한다.
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	// 가리킬 태그가 없는 어빌리티는 스트리밍이 셀 수단이 없어 건너뛴다.
	const FGameplayTag StreamingSkillTag = GetStreamingSkillTag();
	if (!StreamingSkillTag.IsValid())
	{
		return;
	}

	UMyStreamingCombatMessageLibrary::BroadcastSkillUsed(
		this,
		ActorInfo->AvatarActor.Get(),
		StreamingSkillTag);
}

////////////////////////////
//! \author 장효제
//! \brief 스트리밍이 이 어빌리티를 가리킬 태그를 반환한다.
//! \return 기본 구현은 어빌리티 자신의 AbilityTag다.
FGameplayTag UMyGameplayAbilityBase::GetStreamingSkillTag() const
{
	return AbilityTag;
}

FGameplayTag UMyGameplayAbilityBase::GetAbilityTag() const
{
	return AbilityTag;
}

FGameplayTag UMyGameplayAbilityBase::GetInputTag() const
{
	return InputTag;
}

FGameplayTag UMyGameplayAbilityBase::GetCooldownTag() const
{
	return CooldownTag;
}

////////////////////////////
//! \author 장효제
//! \brief 어빌리티 태그가 유효한지 확인한다.
//! \return AbilityTag가 유효하면 true
bool UMyGameplayAbilityBase::HasAbilityTag() const
{
	return AbilityTag.IsValid();
}

////////////////////////////
//! \author 장효제
//! \brief 입력 태그가 유효한지 확인한다.
//! \return InputTag가 유효하면 true
bool UMyGameplayAbilityBase::HasInputTag() const
{
	return InputTag.IsValid();
}

////////////////////////////
//! \author 장효제
//! \brief 쿨다운 태그가 유효한지 확인한다.
//! \return CooldownTag가 유효하면 true
bool UMyGameplayAbilityBase::HasCooldownTag() const
{
	return CooldownTag.IsValid();
}

////////////////////////////
//! \brief 보스 패턴 선택 가중치용 논리적 쿨다운 시간. 기본 구현은 0을 반환한다.
float UMyGameplayAbilityBase::GetCooldownSeconds() const
{
	return 0.0f;
}

////////////////////////////
//! \author 장효제
//! \brief GameplayEffectSpecHandle에 Data.Damage SetByCaller 크기를 할당한다.
//! \param SpecHandle 값을 적용할 GameplayEffectSpecHandle
//! \param Damage 할당할 피해량
//! \return 값 할당에 성공하면 true, SpecHandle이 유효하지 않으면 false
bool UMyGameplayAbilityBase::AssignSetByCallerDamage(FGameplayEffectSpecHandle& SpecHandle, float Damage) const
{
	return UMyAbilitySystemLibrary::AssignSetByCallerDamage(SpecHandle, Damage);
}

////////////////////////////
//! \author HanUl
//! \brief GameplayEffectSpecHandle에 Data.Coefficient SetByCaller 크기를 할당한다.
//! \param SpecHandle 값을 적용할 GameplayEffectSpecHandle
//! \param Coefficient 할당할 스킬 피해 계수
//! \return 값 할당에 성공하면 true, SpecHandle이 유효하지 않으면 false
bool UMyGameplayAbilityBase::AssignSetByCallerCoefficient(FGameplayEffectSpecHandle& SpecHandle, float Coefficient) const
{
	return UMyAbilitySystemLibrary::AssignSetByCallerCoefficient(SpecHandle, Coefficient);
}

////////////////////////////
//! \author 장효제
//! \brief GameplayEffectSpecHandle에 Data.Heal SetByCaller 크기를 할당한다.
//! \param SpecHandle 값을 적용할 GameplayEffectSpecHandle
//! \param Heal 할당할 회복량
//! \return 값 할당에 성공하면 true, SpecHandle이 유효하지 않으면 false
bool UMyGameplayAbilityBase::AssignSetByCallerHeal(FGameplayEffectSpecHandle& SpecHandle, float Heal) const
{
	return UMyAbilitySystemLibrary::AssignSetByCallerHeal(SpecHandle, Heal);
}

////////////////////////////
//! \author 장효제
//! \brief GameplayEffectSpecHandle에 Data.Shield SetByCaller 크기를 할당한다.
//! \param SpecHandle 값을 적용할 GameplayEffectSpecHandle
//! \param Shield 할당할 보호막 수치
//! \return 값 할당에 성공하면 true, SpecHandle이 유효하지 않으면 false
bool UMyGameplayAbilityBase::AssignSetByCallerShield(FGameplayEffectSpecHandle& SpecHandle, float Shield) const
{
	return UMyAbilitySystemLibrary::AssignSetByCallerShield(SpecHandle, Shield);
}
