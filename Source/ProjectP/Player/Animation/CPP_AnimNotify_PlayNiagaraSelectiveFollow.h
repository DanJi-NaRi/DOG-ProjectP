////////////////////////////
//! \file CPP_AnimNotify_PlayNiagaraSelectiveFollow.h
//! \brief Socket 위치, 회전, 스케일 상속을 각각 선택할 수 있는 Niagara AnimNotify 선언 파일이다.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "CPP_AnimNotify_PlayNiagaraSelectiveFollow.generated.h"

class UNiagaraSystem;

////////////////////////////
//! \class UCPP_AnimNotify_PlayNiagaraSelectiveFollow
//! \brief Niagara를 SkeletalMesh Socket에 생성하고 Transform 항목별 부모 상속 여부를 설정한다.
UCLASS(meta = (DisplayName = "Play Niagara Selective Follow"))
class PROJECTP_API UCPP_AnimNotify_PlayNiagaraSelectiveFollow : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	TObjectPtr<UNiagaraSystem> NiagaraSystem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	FName SocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Niagara",
		meta = (EditCondition = "bFollowRotation", EditConditionHides)
	)
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Niagara",
		meta = (EditCondition = "!bFollowRotation", EditConditionHides)
	)
	FRotator AbsoluteWorldRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	FVector Scale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform Follow")
	bool bFollowLocation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform Follow")
	bool bFollowRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform Follow")
	bool bFollowScale = true;
};
