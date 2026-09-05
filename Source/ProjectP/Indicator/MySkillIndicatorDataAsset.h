////////////////////////////
//! \page MySkillIndicatorDataAsset.h
//! \brief MyGAS 스킬 인디케이터 DataAsset 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MySkillIndicatorTypes.h"
#include "MySkillIndicatorDataAsset.generated.h"

class AMySkillIndicatorActorBase;

////////////////////////////
//! \class UMySkillIndicatorDataAsset
//! \brief designer가 Ability별 인디케이터 표시 설정을 지정하는 DataAsset이다.
UCLASS(BlueprintType, Const)
class PROJECTP_API UMySkillIndicatorDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	const FMySkillIndicatorSpec& GetIndicatorSpec() const;
	TSubclassOf<AMySkillIndicatorActorBase> GetIndicatorActorClass() const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Indicator", meta = (AllowPrivateAccess = "true"))
	FMySkillIndicatorSpec IndicatorSpec;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Indicator", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AMySkillIndicatorActorBase> IndicatorActorClass;
};
