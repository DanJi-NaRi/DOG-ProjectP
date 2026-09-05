////////////////////////////
//! \page MyAttributeSet.h
//! \brief Re Duat 전투 AttributeSet의 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "MyAttributeSet.generated.h"

struct FGameplayEffectModCallbackData;

#define ATTRIBUTE_ACCESSORS_LEDUAT(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)


////////////////////////////
//! \class UMyAttributeSet
//! \author 장효제
//! \brief Re Duat 전투에 필요한 체력, 보호막, 공격력, 방어력, 이동 속도, 치명타 확률, 공격 속도, 쿨타임 감소, 메타 어트리뷰트를 관리하는 AttributeSet이다.
//! \note IncomingDamage는 ExecutionCalculation에서 산출된 최종 피해량을 전달받아 Shield와 Health에 적용한다.
//! \note IncomingDamage, IncomingHeal, IncomingShield는 서버에서 즉시 소비되는 메타 어트리뷰트이므로 복제하지 않는다.
UCLASS()
class PROJECTP_API UMyAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMyAttributeSet();

	//! 치명타 확률 최종 상한 (기획: 레벨 스탯 + 아이템 버프 포함 50%)
	static constexpr float CritChanceCap = 0.5f;

	//! 저주 게이지 최대값
	static constexpr float CurseGaugeMax = 100.0f;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower);

	UFUNCTION()
	void OnRep_Defense(const FGameplayAttributeData& OldDefense);

	UFUNCTION()
	void OnRep_DamageTakenMultiplier(const FGameplayAttributeData& OldDamageTakenMultiplier);

	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed);

	UFUNCTION()
	void OnRep_Shield(const FGameplayAttributeData& OldShield);

	UFUNCTION()
	void OnRep_CritChance(const FGameplayAttributeData& OldCritChance);

	UFUNCTION()
	void OnRep_CritDamage(const FGameplayAttributeData& OldCritDamage);

	UFUNCTION()
	void OnRep_CooldownReduction(const FGameplayAttributeData& OldCooldownReduction);

	UFUNCTION()
	void OnRep_AttackSpeed(const FGameplayAttributeData& OldAttackSpeed);

	UFUNCTION()
	void OnRep_MaxMoveCharge(const FGameplayAttributeData& OldMaxMoveCharge);

	UFUNCTION()
	void OnRep_MoveCharge(const FGameplayAttributeData& OldMoveCharge);

	UFUNCTION()
	void OnRep_CurseGauge(const FGameplayAttributeData& OldCurseGauge);

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ReDuat|Attributes", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, Health);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ReDuat|Attributes", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, MaxHealth);


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ReDuat|Attributes", ReplicatedUsing = OnRep_AttackPower)
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, AttackPower);


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ReDuat|Attributes", ReplicatedUsing = OnRep_Defense)
	FGameplayAttributeData Defense;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, Defense);

	//! 받는 피해에 곱해지는 배율. 1.0은 기본 피해, 1.2는 받는 피해 20% 증가다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ReDuat|Attributes", ReplicatedUsing = OnRep_DamageTakenMultiplier)
	FGameplayAttributeData DamageTakenMultiplier;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, DamageTakenMultiplier);


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ReDuat|Attributes", ReplicatedUsing = OnRep_MoveSpeed)
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, MoveSpeed);


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ReDuat|Attributes", ReplicatedUsing = OnRep_Shield)
	FGameplayAttributeData Shield;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, Shield);


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ReDuat|Attributes", ReplicatedUsing = OnRep_CritChance)
	FGameplayAttributeData CritChance;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, CritChance);

	//! 크리티컬 발동 시 기본 피해량에 곱해지는 배율(1.0 이상). 2.0 = 200%.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ReDuat|Attributes", ReplicatedUsing = OnRep_CritDamage)
	FGameplayAttributeData CritDamage;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, CritDamage);


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ReDuat|Attributes", ReplicatedUsing = OnRep_AttackSpeed)
	FGameplayAttributeData AttackSpeed;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, AttackSpeed);


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ReDuat|Attributes", ReplicatedUsing = OnRep_CooldownReduction)
	FGameplayAttributeData CooldownReduction;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, CooldownReduction);


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ReDuat|Attributes", ReplicatedUsing = OnRep_MaxMoveCharge)
	FGameplayAttributeData MaxMoveCharge;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, MaxMoveCharge);


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ReDuat|Attributes", ReplicatedUsing = OnRep_MoveCharge)
	FGameplayAttributeData MoveCharge;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, MoveCharge);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ReDuat|Attributes", ReplicatedUsing = OnRep_CurseGauge)
	FGameplayAttributeData CurseGauge;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, CurseGauge);



	//================
	// 아래의 변수들은 GE에서 보낼 때 받을 메타(임시) 변수임

	UPROPERTY(Transient, BlueprintReadOnly, Category = "ReDuat|Attributes|Meta")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, IncomingDamage);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "ReDuat|Attributes|Meta")
	FGameplayAttributeData IncomingCurseGauge;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, IncomingCurseGauge);

	//! \author 장효제
	//! \brief 피해 Payload에 치명타 여부를 전달하기 위한 서버 소비용 메타 Attribute다.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ReDuat|Attributes|Meta")
	FGameplayAttributeData IncomingCriticalHit;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, IncomingCriticalHit);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "ReDuat|Attributes|Meta")
	FGameplayAttributeData IncomingHeal;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, IncomingHeal);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "ReDuat|Attributes|Meta")
	FGameplayAttributeData IncomingShield;
	ATTRIBUTE_ACCESSORS_LEDUAT(UMyAttributeSet, IncomingShield);
};
