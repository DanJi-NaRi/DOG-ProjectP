////////////////////////////
//! \page MyRichTextDecorators.h
//! \brief 공용 신 색상과 인라인 이미지 RichText Decorator를 선언한다.
#pragma once

#include "Components/RichTextBlockDecorator.h"
#include "Components/RichTextBlockImageDecorator.h"
#include "MyRichTextDecorators.generated.h"

class UDataTable;
class URichTextBlock;

////////////////////////////
//! \class UMyGodRichTextDecorator
//! \author 장효제
//! \brief god 태그의 GameplayTag를 신 대표색으로 변환하는 Decorator다.
UCLASS()
class PROJECTP_API UMyGodRichTextDecorator final : public URichTextBlockDecorator
{
	GENERATED_BODY()

public:
	UMyGodRichTextDecorator(const FObjectInitializer& ObjectInitializer);

	virtual TSharedPtr<ITextDecorator> CreateDecorator(URichTextBlock* InOwner) override;
	bool TryGetGodColor(FName GodTagName, FLinearColor& OutColor) const;

private:
	UPROPERTY()
	TObjectPtr<UDataTable> GodPresentationTable;
};

////////////////////////////
//! \class UMyRichTextImageDecorator
//! \author 장효제
//! \brief 공용 RichImage DataTable의 Texture와 UI Material을 img 태그로 표시한다.
UCLASS()
class PROJECTP_API UMyRichTextImageDecorator final : public URichTextBlockImageDecorator
{
	GENERATED_BODY()

public:
	UMyRichTextImageDecorator(const FObjectInitializer& ObjectInitializer);
};

namespace MyRichText
{
	PROJECTP_API void ConfigureDecorators(URichTextBlock* RichTextBlock);
}
