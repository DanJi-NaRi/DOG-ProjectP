////////////////////////////
//! \file CPP_PressurePlateElement.cpp
//! \brief 압력 발판 요소 구현 파일이다.
//! \editor 준혁 - 발판 진입/이탈/점유변경 검증용 임시 로그 추가
//! \editor 준혁 - 점유 복제를 폰 포인터에서 bool로 변경(릴레번시 대응), 리셋/시작 시 오버랩 재스캔 추가
//! \editor 준혁 - 클리어 고정(SolvedLock) 시 점유 연출 동결 구현
#include "CPP_PressurePlateElement.h"
#include "Streaming/MyStreamingPayloads.h"
#include "MyGameplayTags.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ACPP_PressurePlateElement::ACPP_PressurePlateElement()
{
	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	SetRootComponent(TriggerSphere);
	TriggerSphere->InitSphereRadius(100.0f);
	TriggerSphere->SetCollisionProfileName(TEXT("Trigger"));
	TriggerSphere->SetGenerateOverlapEvents(true);

	PlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlateMesh"));
	PlateMesh->SetupAttachment(TriggerSphere);
	PlateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACPP_PressurePlateElement::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACPP_PressurePlateElement, bIsOccupied);
}

void ACPP_PressurePlateElement::BeginPlay()
{
	Super::BeginPlay();

	// [임시] 코드 로드/넷롤 확인용 - 이게 안 뜨면 옛 DLL이거나 이 클래스가 아님
	UE_LOG(LogTemp, Warning, TEXT("[Gimmick] Plate BeginPlay - %s (Role=%d, Auth=%d)"),
		*GetName(), (int32)GetLocalRole(), HasAuthority() ? 1 : 0);

	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &ACPP_PressurePlateElement::OnPlateBeginOverlap);
	TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &ACPP_PressurePlateElement::OnPlateEndOverlap);

	// 초기 오버랩 이벤트는 위 바인딩 전(Super::BeginPlay)에 디스패치되므로, 이미 겹쳐 있는 폰은 재스캔으로 반영한다.
	if (HasAuthority())
	{
		RescanOverlappingPawns();
	}
}

AActor* ACPP_PressurePlateElement::GetActivator() const
{
	return Occupant;
}

void ACPP_PressurePlateElement::OnPlateBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	// [임시] 오버랩 도달 확인용 (authority 가드보다 위 - 클라/서버 모두 찍힘)
	UE_LOG(LogTemp, Warning, TEXT("[Gimmick] Overlap 도달 - Plate: %s vs %s (Auth=%d)"),
		*GetName(), *GetNameSafe(OtherActor), HasAuthority() ? 1 : 0);

	if (!HasAuthority())
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	OverlappingPawns.AddUnique(Pawn);

	// [임시 주석] 테스트 이후 삭제 예정
	UE_LOG(LogTemp, Log, TEXT("[Gimmick] 발판 진입 - Plate: %s, Pawn: %s"), *GetName(), *Pawn->GetName());

	RefreshOccupant();
}

void ACPP_PressurePlateElement::OnPlateEndOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (!HasAuthority())
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	OverlappingPawns.Remove(Pawn);

	// [임시 주석] 테스트 이후 삭제 예정
	UE_LOG(LogTemp, Log, TEXT("[Gimmick] 발판 이탈 - Plate: %s, Pawn: %s"), *GetName(), *Pawn->GetName());

	RefreshOccupant();
}

void ACPP_PressurePlateElement::RefreshOccupant()
{
	// 소멸/무효 항목 정리
	OverlappingPawns.RemoveAll([](const TObjectPtr<APawn>& P) { return P == nullptr; });

	APawn* NewOccupant = OverlappingPawns.Num() > 0 ? OverlappingPawns[0].Get() : nullptr;
	const bool bOccupantChanged = (NewOccupant != Occupant);
	if (bOccupantChanged)
	{
		const bool bWasPressed = Occupant != nullptr;
		Occupant = NewOccupant;

		// 스트리밍 상태 조건이 "발판을 누르고 있는 동안"을 잴 수 있게 알린다.
		const bool bIsPressed = Occupant != nullptr;
		if (bWasPressed != bIsPressed)
		{
			MyStreamingState::BroadcastState(
				this, MyGameplayTags::Streaming_State_Plate_Pressed, bIsPressed);
		}

		// [임시 주석] 테스트 이후 삭제 예정 - 조건 판정에 실제로 반영되는 대표 점유자 변화
		UE_LOG(LogTemp, Log, TEXT("[Gimmick] 발판 점유 변경 - Plate: %s, 점유: %s, Occupant: %s"),
			*GetName(),
			Occupant != nullptr ? TEXT("ON") : TEXT("OFF"),
			*GetNameSafe(Occupant));
	}

	// 클리어 고정 중에는 연출 상태(bIsOccupied)를 동결한다. 해제 직후 재스캔이 이 블록에서 실제 상태로 재동기화한다.
	const bool bNowOccupied = (Occupant != nullptr);
	if (!IsSolvedLocked() && bIsOccupied != bNowOccupied)
	{
		bIsOccupied = bNowOccupied;

		// 서버/리슨 호스트 연출 (원격 클라이언트는 OnRep_Occupied가 처리)
		OnOccupantChanged(bIsOccupied);
	}

	if (bOccupantChanged)
	{
		MarkStateDirty();
	}
}

void ACPP_PressurePlateElement::OnSolvedLockChanged()
{
	if (IsSolvedLocked())
	{
		// 클리어 연출(눌림/점등)을 켠 채 동결한다. 이미 점유 중이면 변화 없음.
		if (!bIsOccupied)
		{
			bIsOccupied = true;
			OnOccupantChanged(true);
		}
	}
	else
	{
		// 해제: 물리적 실제 상태로 재동기화(RefreshOccupant가 bIsOccupied를 되돌린다).
		RescanOverlappingPawns();
	}
}

void ACPP_PressurePlateElement::RescanOverlappingPawns()
{
	OverlappingPawns.Reset();

	TArray<AActor*> Overlapping;
	TriggerSphere->GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (Pawn && Pawn->IsPlayerControlled())
		{
			OverlappingPawns.AddUnique(Pawn);
		}
	}

	RefreshOccupant();
}

void ACPP_PressurePlateElement::ResetElement()
{
	if (!HasAuthority())
	{
		return;
	}

	Super::ResetElement();

	// 발판은 움직이는 부품이 없으므로 리셋 = 물리적 실제 상태로 재동기화.
	// (리셋 시점에 여전히 밟고 있는 플레이어를 놓치지 않는다)
	RescanOverlappingPawns();
}

void ACPP_PressurePlateElement::OnRep_Occupied()
{
	UE_LOG(LogTemp, Warning, TEXT("[Gimmick][ClientRep] Occupied replicated - Plate: %s, bIsOccupied=%d, Auth=%d"),
		*GetName(), bIsOccupied ? 1 : 0, HasAuthority() ? 1 : 0);

	OnOccupantChanged(bIsOccupied);
}
