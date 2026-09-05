////////////////////////////
//! \page MyGodPresentationTypes.h
//! \brief 신의 공용 표시 정보에 사용하는 DataTable 타입 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MyGodPresentationTypes.generated.h"

class UTexture2D;

////////////////////////////
//! \enum EMyGodPortraitStage
//! \author 장효제
//! \brief 호감도 공개 단계에 따라 사용할 신 초상화 종류다.
UENUM(BlueprintType)
enum class EMyGodPortraitStage : uint8
{
	Silhouette UMETA(DisplayName = "Silhouette"),
	Emote UMETA(DisplayName = "Emote"),
	Full UMETA(DisplayName = "Full")
};

////////////////////////////
//! \struct FMyGodPresentationRow
//! \author 장효제
//! \brief 신 태그에 대응하는 표시 이름과 아이콘을 제공하는 공용 DataTable 행 구조체다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyGodPresentationRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "God|Presentation", meta = (Categories = "God"))
	FGameplayTag GodTag;

	//! 신 페이지 목록의 표시 순서다. 값이 같으면 DataTable RowName 오름차순으로 정렬한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "God|Presentation")
	int32 DisplayOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "God|Presentation")
	FText DisplayName;

	//! 신 이름 표시에 사용하는 #RRGGBB sRGB 색상이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "God|Presentation")
	FString GodColor = TEXT("#FFFFFF");

	FLinearColor GetGodLinearColor() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "God|Presentation")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "God|Presentation", meta = (MultiLine = "true"))
	FText Introduction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "God|Presentation")
	TSoftObjectPtr<UTexture2D> SilhouettePortrait;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "God|Presentation")
	TSoftObjectPtr<UTexture2D> EmotePortrait;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "God|Presentation")
	TSoftObjectPtr<UTexture2D> FullPortrait;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "God|Presentation")
	TArray<FName> BlessingIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "God|Presentation")
	TArray<FText> FavoriteActions;
};

namespace MyGodPresentation
{
	//! 신 표시 DataTable의 기본 에셋 경로다. 에셋을 옮기면 이 상수 한 곳만 고친다.
	//! 생성자의 ConstructorHelpers::FObjectFinder에서도 그대로 쓸 수 있다.
	inline constexpr const TCHAR* DefaultTablePath = TEXT("/Game/LeDuat/Systems/God/DT_GodPresentation.DT_GodPresentation");

	PROJECTP_API const FMyGodPresentationRow* FindByTag(const UDataTable* PresentationTable, FGameplayTag GodTag);

	//! 신 표시 DataTable을 기본 경로에서 불러온다. 반환값을 캐시할 책임은 호출자에게 있다.
	//! (UPROPERTY에 보관하지 않으면 GC 대상이 된다)
	PROJECTP_API UDataTable* LoadDefaultTable();
}
