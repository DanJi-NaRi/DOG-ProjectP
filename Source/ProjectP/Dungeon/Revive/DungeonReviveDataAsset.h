#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DungeonReviveDataAsset.generated.h"

class UGameplayEffect;
class UTexture2D;

////////////////////////////
//! \struct FDungeonReviveOption
//! \author HanUl
//! \brief 비용, 부활 체력, 대기시간과 표시 정보를 함께 정의하는 데이터 기반 부활 옵션이다.
USTRUCT(BlueprintType)
struct PROJECTP_API FDungeonReviveOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive")
	FName OptionId;

	//! 제안하는 신의 정체성 태그다. 지정하면 이름, 초상화, 대표색을 DT_GodPresentation에서 가져온다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive|Presentation", meta = (Categories = "God"))
	FGameplayTag GodTag;

	//! GodTag를 지정하지 않은 옵션에서 사용할 이름이다. GodTag가 있으면 DT_GodPresentation 값이 우선한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive|Presentation")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive|Presentation", meta = (MultiLine = "true"))
	FText Description;

	//! GodTag를 지정하지 않은 옵션에서 사용할 초상화다. GodTag가 있으면 DT_GodPresentation 값이 우선한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive|Presentation")
	TSoftObjectPtr<UTexture2D> Portrait;

	//! UI가 카드 테두리, 색상, 레이아웃 스타일을 찾을 때 사용할 논리 ID다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive|Presentation")
	FName StyleId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive|Gameplay", meta = (ClampMin = "0"))
	int32 MesoCost = 0;

	//! 0.45는 최대 체력의 45%를 의미한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive|Gameplay", meta = (ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0"))
	float ReviveHealthPercent = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive|Gameplay", meta = (ClampMin = "0.0"))
	float ReviveDelaySeconds = 8.0f;

	//! 부활 완료 후 선택적으로 적용할 추가 효과다. 무적, 보호막, 이동 버프 같은 옵션 확장에 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive|Gameplay")
	TSoftClassPtr<UGameplayEffect> PostReviveEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive")
	int32 SortOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive")
	bool bCanAutoSelect = true;

	bool IsGameplayConfigurationValid(FString& OutError) const;
};

////////////////////////////
//! \class UDungeonReviveDataAsset
//! \author HanUl
//! \brief 던전에서 사용할 가변 개수의 부활 옵션과 향후 UI 자동 선택 규칙을 보관한다.
UCLASS(BlueprintType)
class PROJECTP_API UDungeonReviveDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive|Selection", meta = (ClampMin = "0.0"))
	float SelectionTimeoutSeconds = 10.0f;

	//! 선택 제한 시간이 끝났을 때 사용할 옵션 ID다. UI 연결 단계에서 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive|Selection")
	FName DefaultAutoSelectOptionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revive")
	TArray<FDungeonReviveOption> Options;

	const FDungeonReviveOption* FindOption(FName OptionId, bool bRequireEnabled = true) const;
	void GetEnabledOptionIds(TArray<FName>& OutOptionIds) const;
	bool ValidateData(FString& OutError) const;
};
