////////////////////////////
//! \file CPP_AnimNotify_PlayNiagaraSelectiveFollow.cpp
//! \brief Socket Transform 항목별 상속을 선택할 수 있는 Niagara AnimNotify 구현 파일이다.

#include "CPP_AnimNotify_PlayNiagaraSelectiveFollow.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

////////////////////////////
//! \author HanUl
//! \brief Niagara를 Socket에 생성하고 위치, 회전, 스케일의 부모 상속 여부를 적용한다.
//! \param MeshComp Notify를 실행한 SkeletalMeshComponent
//! \param Animation Notify를 포함한 Animation Asset
//! \param EventReference Notify 실행 컨텍스트
//! \return 없음
void UCPP_AnimNotify_PlayNiagaraSelectiveFollow::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !NiagaraSystem)
	{
		return;
	}

	UWorld* World = MeshComp->GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		NiagaraSystem,
		MeshComp,
		SocketName,
		LocationOffset,
		bFollowRotation ? RotationOffset : FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		true,
		false
	);
	if (!NiagaraComponent)
	{
		return;
	}

	NiagaraComponent->SetRelativeScale3D(Scale);

	const FVector InitialWorldLocation = NiagaraComponent->GetComponentLocation();
	const FVector InitialWorldScale = NiagaraComponent->GetComponentScale();

	NiagaraComponent->SetAbsolute(
		!bFollowLocation,
		!bFollowRotation,
		!bFollowScale
	);

	if (!bFollowLocation)
	{
		NiagaraComponent->SetWorldLocation(InitialWorldLocation);
	}

	if (!bFollowRotation)
	{
		NiagaraComponent->SetWorldRotation(AbsoluteWorldRotation);
	}

	if (!bFollowScale)
	{
		NiagaraComponent->SetWorldScale3D(Scale);
	}

	NiagaraComponent->Activate(true);
}

////////////////////////////
//! \author HanUl
//! \brief Montage Notify 트랙에 Niagara System 이름이 포함된 표시명을 반환한다.
//! \param 없음
//! \return Notify 표시명
FString UCPP_AnimNotify_PlayNiagaraSelectiveFollow::GetNotifyName_Implementation() const
{
	return NiagaraSystem
		? FString::Printf(TEXT("Niagara Selective Follow: %s"), *NiagaraSystem->GetName())
		: TEXT("Niagara Selective Follow");
}
