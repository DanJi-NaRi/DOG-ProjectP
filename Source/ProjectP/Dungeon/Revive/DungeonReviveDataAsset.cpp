#include "Dungeon/Revive/DungeonReviveDataAsset.h"

////////////////////////////
//! \author HanUl
//! \brief 부활 옵션의 서버 게임플레이 값이 실행 가능한 범위인지 검사한다.
//! \param OutError 잘못된 경우 원인을 받을 문자열
//! \return 비용, 체력 비율, 대기시간과 ID가 유효하면 true
bool FDungeonReviveOption::IsGameplayConfigurationValid(FString& OutError) const
{
	OutError.Reset();

	if (OptionId.IsNone())
	{
		OutError = TEXT("OptionId가 비어 있습니다.");
		return false;
	}

	if (MesoCost < 0)
	{
		OutError = FString::Printf(TEXT("'%s'의 MesoCost가 0보다 작습니다."), *OptionId.ToString());
		return false;
	}

	if (ReviveHealthPercent <= 0.0f || ReviveHealthPercent > 1.0f)
	{
		OutError = FString::Printf(TEXT("'%s'의 ReviveHealthPercent는 0 초과 1 이하여야 합니다."), *OptionId.ToString());
		return false;
	}

	if (ReviveDelaySeconds < 0.0f)
	{
		OutError = FString::Printf(TEXT("'%s'의 ReviveDelaySeconds가 0보다 작습니다."), *OptionId.ToString());
		return false;
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief ID가 일치하고 필요하면 활성화된 부활 옵션을 찾는다.
//! \param OptionId 찾을 안정적인 옵션 ID
//! \param bRequireEnabled true이면 비활성 옵션을 반환하지 않는다
//! \return 일치하는 옵션 포인터, 없으면 nullptr
const FDungeonReviveOption* UDungeonReviveDataAsset::FindOption(FName OptionId, bool bRequireEnabled) const
{
	if (OptionId.IsNone())
	{
		return nullptr;
	}

	return Options.FindByPredicate(
		[OptionId, bRequireEnabled](const FDungeonReviveOption& Option)
		{
			return Option.OptionId == OptionId && (!bRequireEnabled || Option.bEnabled);
		});
}

////////////////////////////
//! \author HanUl
//! \brief 활성화된 부활 옵션 ID를 SortOrder와 OptionId 순서로 반환한다.
//! \param OutOptionIds 정렬된 활성 옵션 ID 배열
//! \return 없음
void UDungeonReviveDataAsset::GetEnabledOptionIds(TArray<FName>& OutOptionIds) const
{
	TArray<const FDungeonReviveOption*> EnabledOptions;
	for (const FDungeonReviveOption& Option : Options)
	{
		if (Option.bEnabled)
		{
			EnabledOptions.Add(&Option);
		}
	}

	EnabledOptions.Sort([](const FDungeonReviveOption& Left, const FDungeonReviveOption& Right)
	{
		if (Left.SortOrder != Right.SortOrder)
		{
			return Left.SortOrder < Right.SortOrder;
		}
		return Left.OptionId.LexicalLess(Right.OptionId);
	});

	OutOptionIds.Reset(EnabledOptions.Num());
	for (const FDungeonReviveOption* Option : EnabledOptions)
	{
		OutOptionIds.Add(Option->OptionId);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 전체 데이터의 옵션 값, 중복 ID, 자동 선택 ID 계약을 검사한다.
//! \param OutError 첫 번째 검증 실패 원인을 받을 문자열
//! \return 서버에서 안전하게 사용할 수 있으면 true
bool UDungeonReviveDataAsset::ValidateData(FString& OutError) const
{
	OutError.Reset();
	TSet<FName> SeenOptionIds;

	for (const FDungeonReviveOption& Option : Options)
	{
		FString OptionError;
		if (!Option.IsGameplayConfigurationValid(OptionError))
		{
			OutError = MoveTemp(OptionError);
			return false;
		}

		if (SeenOptionIds.Contains(Option.OptionId))
		{
			OutError = FString::Printf(TEXT("중복된 부활 OptionId가 있습니다: '%s'"), *Option.OptionId.ToString());
			return false;
		}
		SeenOptionIds.Add(Option.OptionId);
	}

	if (!DefaultAutoSelectOptionId.IsNone())
	{
		const FDungeonReviveOption* DefaultOption = FindOption(DefaultAutoSelectOptionId, true);
		if (!DefaultOption || !DefaultOption->bCanAutoSelect)
		{
			OutError = FString::Printf(
				TEXT("DefaultAutoSelectOptionId '%s'가 없거나 자동 선택 불가 상태입니다."),
				*DefaultAutoSelectOptionId.ToString());
			return false;
		}
	}

	return true;
}
