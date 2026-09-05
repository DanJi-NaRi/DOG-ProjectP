#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MyDialogueDataAsset.generated.h"

class UTexture2D;

//! \enum EMyDialogueScope 대화 시작 대상 범위
UENUM(BlueprintType)
enum class EMyDialogueScope : uint8
{
    Personal UMETA(DisplayName = "개인 (상호작용자만)"),
    Party    UMETA(DisplayName = "파티 전원"),
};

//! \struct FMyDialogueChoice 대화 선택지 하나. 선택 시 분기 이동과 기믹 트리거 통지를 담당한다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyDialogueChoice
{
    GENERATED_BODY()

    //! 선택지 버튼에 표시할 텍스트
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    FText Text;

    //! true면 이 선택지를 고를 때 서버에 통지되어 출처 오벨리스크의 기믹 트리거가 발동된다.
    //! (오벨리스크의 GimmickTriggerTiming이 OnDialogueChoice일 때만 서버가 수락한다)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    bool bTriggersGimmick = false;

    //! true면 이 선택지를 고를 때 서버에 통지되어 현재 Zone 기믹 초기화 투표를 요청한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    bool bStartsGimmickResetVote = false;

    //! 선택 후 이동할 줄 인덱스. -1이면 다음 줄로 진행. 유효 범위를 벗어나면 대화가 닫힌다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (ClampMin = "-1"))
    int32 NextLineIndex = -1;
};

//! \struct FMyDialogueLine 대화 한 줄. 화자 이미지 + 텍스트 + (선택) 선택지 목록.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyDialogueLine
{
    GENERATED_BODY()

    //! 화자가 신이면 이 태그를 지정한다. 이름과 초상화를 DT_GodPresentation에서 자동으로 가져온다.
    //! 태그 vs 밑의 텍스트와 이미지 = 태그가 우선한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (Categories = "God"))
    FGameplayTag SpeakerGodTag;

    //! 신이 아닌 화자의 이름. SpeakerGodTag를 지정하면 무시되고 표의 이름이 쓰인다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    FText SpeakerName;

    //! 신이 아닌 화자의 이미지. SpeakerGodTag를 지정하면 무시되고 표의 초상화가 쓰인다.
    //! 비워두면 그 줄에서는 이미지가 숨겨진다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    TSoftObjectPtr<UTexture2D> SpeakerImage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (MultiLine = "true"))
    FText Text;

    //! 이 줄에서 표시할 선택지들. 비어있지 않으면 좌클릭 진행이 막히고 선택지 버튼으로만 진행된다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (TitleProperty = "Text"))
    TArray<FMyDialogueChoice> Choices;
};

////////////////////////////
//! \class UMyDialogueDataAsset
//! \brief 대화 하나(줄 목록)를 담는 데이터에셋. 오벨리스크 등 대화 트리거 액터에 지정해 사용한다.
//!        진행(줄 넘기기)은 각 클라이언트 로컬에서만 이루어지며 동기화되지 않는다.
UCLASS(BlueprintType)
class PROJECTP_API UMyDialogueDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (TitleProperty = "Text"))
    TArray<FMyDialogueLine> Lines;
};
