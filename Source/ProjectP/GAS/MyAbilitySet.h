////////////////////////////
//! \page MyAbilitySet.h
//! \brief MyGAS AbilitySet DataAsset 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "MyAbilitySet.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;

////////////////////////////
//! \struct FMyAbilitySetEntry
//! \author 장효제
//! \brief MyAbilitySet에서 ASC에 부여할 GameplayAbility 정보를 정의한다.
USTRUCT(BlueprintType)
struct FMyAbilitySetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Ability")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Ability", meta = (ClampMin = "1"))
	int32 AbilityLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Ability")
	FGameplayTag InputTag;
};

////////////////////////////
//! \struct FMyAbilitySetGameplayEffect
//! \author 장효제
//! \brief MyAbilitySet에서 ASC 자신에게 적용할 GameplayEffect 정보를 정의한다.
USTRUCT(BlueprintType)
struct FMyAbilitySetGameplayEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Gameplay Effect")
	TSubclassOf<UGameplayEffect> GameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Gameplay Effect", meta = (ClampMin = "0.0"))
	float EffectLevel = 1.0f;
};

////////////////////////////
//! \struct FMyAbilitySetGrantedHandles
//! \author 장효제
//! \brief MyAbilitySet이 ASC에 부여한 AbilitySpecHandle과 ActiveGameplayEffectHandle 목록을 저장하고 회수한다.
USTRUCT(BlueprintType)
struct FMyAbilitySetGrantedHandles
{
	GENERATED_BODY()

public:
	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);

	void TakeFromAbilitySystem(UAbilitySystemComponent* ASC);

private:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;
};

////////////////////////////
//! \class UMyAbilitySet
//! \author 장효제
//! \brief MyGAS에서 ASC에 부여할 GameplayAbility 목록을 보관하는 DataAsset이다.
UCLASS(BlueprintType, Const)
class PROJECTP_API UMyAbilitySet : public UDataAsset
{
	GENERATED_BODY()

public:

	void GiveToAbilitySystem(UAbilitySystemComponent* ASC, FMyAbilitySetGrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Ability Set", meta = (AllowPrivateAccess = "true"))
	TArray<FMyAbilitySetEntry> GrantedAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Ability Set", meta = (AllowPrivateAccess = "true"))
	TArray<FMyAbilitySetGameplayEffect> GrantedGameplayEffects;
};
