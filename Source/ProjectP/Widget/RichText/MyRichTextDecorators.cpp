////////////////////////////
//! \page MyRichTextDecorators.cpp
//! \brief 공용 신 색상과 인라인 이미지 RichText Decorator를 구현한다.
#include "Widget/RichText/MyRichTextDecorators.h"

#include "Components/RichTextBlock.h"
#include "Engine/DataTable.h"
#include "God/MyGodPresentationTypes.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	class FMyGodTextDecorator final : public FRichTextDecorator
	{
	public:
		FMyGodTextDecorator(URichTextBlock* InOwner, const UMyGodRichTextDecorator* InDecorator)
			: FRichTextDecorator(InOwner), Decorator(InDecorator)
		{
		}

		virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override
		{
			return RunParseResult.Name == TEXT("god")
				&& RunParseResult.MetaData.Contains(TEXT("id"));
		}

	protected:
		virtual void CreateDecoratorText(
			const FTextRunInfo& RunInfo,
			FTextBlockStyle& InOutTextStyle,
			FString& InOutString) const override
		{
			InOutString += RunInfo.Content.ToString();

			const FString* GodTagName = RunInfo.MetaData.Find(TEXT("id"));
			FLinearColor GodColor = FLinearColor::White;
			if (GodTagName)
			{
				Decorator->TryGetGodColor(**GodTagName, GodColor);
			}
			InOutTextStyle.SetColorAndOpacity(FSlateColor(GodColor));
		}

	private:
		const UMyGodRichTextDecorator* Decorator;
	};
}

UMyGodRichTextDecorator::UMyGodRichTextDecorator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UDataTable> GodPresentationTableAsset(
		MyGodPresentation::DefaultTablePath);
	if (GodPresentationTableAsset.Succeeded())
	{
		GodPresentationTable = GodPresentationTableAsset.Object;
	}
}

TSharedPtr<ITextDecorator> UMyGodRichTextDecorator::CreateDecorator(URichTextBlock* InOwner)
{
	return MakeShared<FMyGodTextDecorator>(InOwner, this);
}

////////////////////////////
//! \author 장효제
//! \brief GameplayTag 문자열에 대응하는 신 대표색을 찾는다.
//! \param GodTagName god 태그의 id 메타데이터다.
//! \param OutColor 찾은 선형 대표색이다.
//! \return 대응하는 God Presentation 행을 찾았으면 true다.
bool UMyGodRichTextDecorator::TryGetGodColor(FName GodTagName, FLinearColor& OutColor) const
{
	if (!GodPresentationTable)
	{
		return false;
	}

	TArray<FMyGodPresentationRow*> Presentations;
	GodPresentationTable->GetAllRows(TEXT("UMyGodRichTextDecorator::TryGetGodColor"), Presentations);
	for (const FMyGodPresentationRow* Presentation : Presentations)
	{
		if (Presentation && Presentation->GodTag.ToString().Equals(GodTagName.ToString(), ESearchCase::CaseSensitive))
		{
			OutColor = Presentation->GetGodLinearColor();
			return true;
		}
	}
	return false;
}

UMyRichTextImageDecorator::UMyRichTextImageDecorator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ImageSet = Cast<UDataTable>(StaticLoadObject(
		UDataTable::StaticClass(),
		nullptr,
		TEXT("/Game/LeDuat/Widget/DT_RichTextImages.DT_RichTextImages"),
		nullptr,
		LOAD_NoWarn | LOAD_Quiet));
}

////////////////////////////
//! \author 장효제
//! \brief 프로젝트 공용 god와 img Decorator를 RichTextBlock에 설정한다.
//! \param RichTextBlock 설정할 RichTextBlock이다.
void MyRichText::ConfigureDecorators(URichTextBlock* RichTextBlock)
{
	if (!RichTextBlock)
	{
		return;
	}

	RichTextBlock->SetDecorators({
		UMyGodRichTextDecorator::StaticClass(),
		UMyRichTextImageDecorator::StaticClass(),
	});
}
