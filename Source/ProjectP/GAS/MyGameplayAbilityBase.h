////////////////////////////
//! \page MyGameplayAbilityBase.h
//! \brief MyGAS GameplayAbility 기반 클래스 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "MyGameplayAbilityBase.generated.h"

////////////////////////////
//! \class UMyGameplayAbilityBase
//! \author HanUl
//! \brief MyGAS 어빌리티 공통 태그와 SetByCaller 편의 함수를 제공하는 GameplayAbility 기반 클래스다.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyGameplayAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UMyGameplayAbilityBase();

	//! \brief 어빌리티가 켜졌다는 사실을 스트리밍 시스템에 알린다.
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	//! \brief 스트리밍이 이 어빌리티를 가리킬 태그다. 파생 클래스가 다른 출처를 줄 수 있다.
	virtual FGameplayTag GetStreamingSkillTag() const;

	FGameplayTag GetAbilityTag() const;
	FGameplayTag GetInputTag() const;
	FGameplayTag GetCooldownTag() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Ability")
	bool HasAbilityTag() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Ability")
	bool HasInputTag() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Ability")
	bool HasCooldownTag() const;

	//! \brief 보스 패턴 선택 가중치에 사용되는 논리적 쿨다운 시간(초). 파생 클래스가 오버라이드한다. 기본 0.
	UFUNCTION(BlueprintPure, Category = "MyGAS|Cooldown")
	virtual float GetCooldownSeconds() const;

	UFUNCTION(BlueprintCallable, Category = "MyGAS|SetByCaller")
	bool AssignSetByCallerDamage(UPARAM(ref) FGameplayEffectSpecHandle& SpecHandle, float Damage) const;

	UFUNCTION(BlueprintCallable, Category = "MyGAS|SetByCaller")
	bool AssignSetByCallerCoefficient(UPARAM(ref) FGameplayEffectSpecHandle& SpecHandle, float Coefficient) const;

	UFUNCTION(BlueprintCallable, Category = "MyGAS|SetByCaller")
	bool AssignSetByCallerHeal(UPARAM(ref) FGameplayEffectSpecHandle& SpecHandle, float Heal) const;

	UFUNCTION(BlueprintCallable, Category = "MyGAS|SetByCaller")
	bool AssignSetByCallerShield(UPARAM(ref) FGameplayEffectSpecHandle& SpecHandle, float Shield) const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Tags")
	FGameplayTag AbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Tags")
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Tags")
	FGameplayTag CooldownTag;
};
