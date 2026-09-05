////////////////////////////
//! \page MyGodPresentationTypes.cpp
//! \brief 신 대표색의 안전한 변환을 구현한다.
#include "God/MyGodPresentationTypes.h"

////////////////////////////
//! \author 장효제
//! \brief #RRGGBB sRGB 대표색을 Slate에서 사용할 선형 색상으로 변환한다.
//! \return 유효한 대표색이며 형식이 잘못되면 흰색이다.
FLinearColor FMyGodPresentationRow::GetGodLinearColor() const
{
	if (GodColor.Len() != 7 || GodColor[0] != TEXT('#'))
	{
		return FLinearColor::White;
	}

	for (int32 Index = 1; Index < GodColor.Len(); ++Index)
	{
		if (!FChar::IsHexDigit(GodColor[Index]))
		{
			return FLinearColor::White;
		}
	}

	return FLinearColor::FromSRGBColor(FColor::FromHex(GodColor));
}

namespace MyGodPresentation
{
	////////////////////////////
	//! \author 장효제
	//! \brief GodTag에 대응하는 신 표시 행을 찾는다. RowName이 아니라 태그로 찾는다.
	//! \param PresentationTable 신 표시 정보 DataTable이다.
	//! \param GodTag 찾을 신 정체성 태그다.
	//! \return 일치하는 행이며 없으면 nullptr이다.
	const FMyGodPresentationRow* FindByTag(const UDataTable* PresentationTable, FGameplayTag GodTag)
	{
		if (!PresentationTable || !GodTag.IsValid())
		{
			return nullptr;
		}

		TArray<FMyGodPresentationRow*> AllPresentations;
		PresentationTable->GetAllRows(TEXT("MyGodPresentation::FindByTag"), AllPresentations);
		for (const FMyGodPresentationRow* Presentation : AllPresentations)
		{
			if (Presentation && Presentation->GodTag.MatchesTagExact(GodTag))
			{
				return Presentation;
			}
		}
		return nullptr;
	}

	////////////////////////////
	//! \author 장효제
	//! \brief 신 표시 DataTable을 기본 경로에서 불러온다. WBP에 테이블을 지정하지 않은 경우의 대비책이다.
	//! \return 불러온 DataTable이며 실패하면 nullptr이다.
	UDataTable* LoadDefaultTable()
	{
		return LoadObject<UDataTable>(nullptr, DefaultTablePath, nullptr, LOAD_NoWarn);
	}
}
