#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Boss/Core/CPP_BossTypes.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "CPP_BossPatternSetData.generated.h"

USTRUCT(BlueprintType)
struct PROJECTP_API FBossPatternEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern")
	TSubclassOf<UMyGameplayAbilityBase> AbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern")
	bool bIsGimmick = false;
};

UCLASS(BlueprintType)
class PROJECTP_API UCPP_BossPatternSetData : public UDataAsset
{
	GENERATED_BODY()

public:
	const TArray<FBossPatternEntry>& GetPatternsForPhase(EBossPhase Phase) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Pattern", meta = (AllowPrivateAccess = "true"))
	TArray<FBossPatternEntry> Phase1Patterns;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Pattern", meta = (AllowPrivateAccess = "true"))
	TArray<FBossPatternEntry> Phase2Patterns;
};
