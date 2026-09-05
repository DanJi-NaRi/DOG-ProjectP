////////////////////////////
//! \file MySkillDefinitionDataAsset.h
//! \brief MyGAS 단일 스킬 정의 DataAsset 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "../MyGameplayAbility_SkillBase.h"
#include "MySkillDefinitionFragment.h"
#include "MySkillSetDataAsset.h"
#include "MySkillDefinitionDataAsset.generated.h"

class UAnimMontage;
class UTexture2D;

////////////////////////////
//! \struct FMySkillAnimationSpec
//! \author HanUl
//! \brief 스킬 애니메이션과 발동 프레임 동기화에 필요한 설정을 정의한다.
USTRUCT(BlueprintType)
struct FMySkillAnimationSpec
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Animation")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Animation")
	FName StartSectionName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Animation")
	FName FireNotifyName = TEXT("Fire");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Animation")
	FName EndNotifyName = TEXT("End");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Animation")
	FGameplayTag FireEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Animation")
	FGameplayTag EndEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Animation", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Animation")
	bool bStopWhenAbilityEnds = true;
};


////////////////////////////
//! \class UMySkillDefinitionDataAsset
//! \author HanUl
//! \brief 한 스킬의 AbilityClass, 입력/쿨다운 태그, 타이밍, 인디케이터, 효과 데이터를 묶어 보관하는 DataAsset이다.
UCLASS(BlueprintType, Const)
class PROJECTP_API UMySkillDefinitionDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "MyGAS|Skill Definition")
	TSubclassOf<UMyGameplayAbility_SkillBase> GetAbilityClass() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Skill Definition")
	int32 GetAbilityLevel() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Skill Definition")
	FName GetSkillId() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Skill Definition")
	FText GetDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Skill Definition")
	FGameplayTag GetInputTag() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Skill Definition")
	FGameplayTag GetAbilityTag() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Skill Definition")
	FGameplayTag GetCooldownTag() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Skill Definition")
	float GetCooldownDuration() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Skill Definition")
	bool IsValidDefinition() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Skill Definition")
	bool HasIndicator() const;

	const FMySkillAnimationSpec& GetAnimation() const;
	const FMySkillTimingSpec& GetTiming() const;
	const FMySkillTargetingSpec& GetTargeting() const;
	const FMySkillInputSpec& GetInput() const;
	const FMySkillIndicatorVisualSpec& GetIndicatorVisual() const;
	const FMySkillComboSpec& GetCombo() const;
	const FMySkillAimAssistSpec& GetAimAssist() const;
	const FMySkillProjectileSpec& GetProjectile() const;
	const FMySkillAreaSpec& GetArea() const;
	const FMySkillEffectSpec& GetEffects() const;
	const FMySkillMovementSpec& GetMovement() const;
	const TArray<TObjectPtr<UMySkillDefinitionFragment>>& GetFragments() const;

	template<typename FragmentType>
	const FragmentType* FindFragment() const
	{
		for (const TObjectPtr<UMySkillDefinitionFragment>& Fragment : Fragments)
		{
			if (const FragmentType* TypedFragment = Cast<FragmentType>(Fragment.Get()))
			{
				return TypedFragment;
			}
		}

		return nullptr;
	}

	bool BuildSkillDataEntry(FMySkillDataEntry& OutSkillData) const;
	bool BuildIndicatorSpec(
		FMySkillIndicatorSpec& OutIndicatorSpec,
		TSubclassOf<AMySkillIndicatorActorBase>& OutIndicatorActorClass
	) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Ability", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UMyGameplayAbility_SkillBase> AbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Ability", meta = (ClampMin = "1", AllowPrivateAccess = "true"))
	int32 AbilityLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Identity", meta = (AllowPrivateAccess = "true"))
	FName SkillId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Identity", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Identity", meta = (AllowPrivateAccess = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Identity", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Tags", meta = (AllowPrivateAccess = "true"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Tags", meta = (AllowPrivateAccess = "true"))
	FGameplayTag AbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Tags", meta = (AllowPrivateAccess = "true"))
	FGameplayTag CooldownTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Tags", meta = (AllowPrivateAccess = "true"))
	FGameplayTag SkillGroupTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Tags", meta = (AllowPrivateAccess = "true"))
	FGameplayTag SkillCategoryTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Cooldown", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float CooldownDuration = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Animation", meta = (AllowPrivateAccess = "true"))
	FMySkillAnimationSpec Animation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Timing", meta = (AllowPrivateAccess = "true"))
	FMySkillTimingSpec Timing;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Targeting", meta = (AllowPrivateAccess = "true"))
	FMySkillTargetingSpec Targeting;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Input", meta = (AllowPrivateAccess = "true"))
	FMySkillInputSpec Input;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Indicator", meta = (AllowPrivateAccess = "true"))
	FMySkillIndicatorVisualSpec IndicatorVisual;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Combo", meta = (AllowPrivateAccess = "true"))
	FMySkillComboSpec Combo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Aim Assist", meta = (AllowPrivateAccess = "true"))
	FMySkillAimAssistSpec AimAssist;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Projectile", meta = (AllowPrivateAccess = "true"))
	FMySkillProjectileSpec Projectile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Area", meta = (AllowPrivateAccess = "true"))
	FMySkillAreaSpec Area;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Effects", meta = (AllowPrivateAccess = "true"))
	FMySkillEffectSpec Effects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Movement", meta = (AllowPrivateAccess = "true"))
	FMySkillMovementSpec Movement;

	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "MyGAS|Skill Definition|Fragments", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UMySkillDefinitionFragment>> Fragments;
};
