////////////////////////////
//! \file CPP_Interactable.h
//! \brief 기믹/월드 오브젝트용 범용 상호작용 인터페이스 선언 파일이다.
//! \editor 준혁 - ICPP_Activatable 제거(Zone의 IZoneSignalReceiver로 통일, 문 신호 중복 해소)
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CPP_Interactable.generated.h"

////////////////////////////
//! \brief 플레이어가 능동적으로 조작하는 오브젝트(반사판 회전, 단지 집기 등)가 구현한다.
UINTERFACE(MinimalAPI, Blueprintable)
class UCPP_Interactable : public UInterface
{
	GENERATED_BODY()
};

class PROJECTP_API ICPP_Interactable
{
	GENERATED_BODY()

public:
	//! 지금 이 Interactor가 상호작용할 수 있는가?
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Gimmick|Interact")
	bool CanInteract(AActor* Interactor) const;

	//! 상호작용 시작.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Gimmick|Interact")
	void Interact(AActor* Interactor);

	//! 상호작용 종료(손을 뗌).
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Gimmick|Interact")
	void EndInteract(AActor* Interactor);
};
