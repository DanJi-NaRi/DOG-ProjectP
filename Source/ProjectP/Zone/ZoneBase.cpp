#include "ZoneBase.h"
#include "Streaming/MyStreamingPayloads.h"
#include "MyGameplayTags.h"

#include "Components/BoxComponent.h"
#include "Components/ClearComponent.h"
#include "Components/CPP_SpawnerComponent.h"
#include "Components/CPP_ZoneFallingRockComponent.h"
#include "GameFramework/PlayerState.h"
#include "Player/PlayerCharacterBase.h"
#include "TimerManager.h"
#include "ZoneSignalReceiver.h"

////////////////////////////
//! \author HanUl
//! \brief Zone의 기본 Bound Collider를 생성하고 에디터에서 조절 가능한 초기값을 설정한다.
//! \param 
//! \return 
AZoneBase::AZoneBase()
{
	PrimaryActorTick.bCanEverTick = false;

	ZoneBoundary = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBoundary"));
	SetRootComponent(ZoneBoundary);

	ZoneBoundary->SetBoxExtent(BoundaryExtent);
	ZoneBoundary->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ZoneBoundary->SetCollisionObjectType(ECC_WorldDynamic);
	ZoneBoundary->SetCollisionResponseToAllChannels(ECR_Ignore);
	ZoneBoundary->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ZoneBoundary->SetGenerateOverlapEvents(true);
	ZoneBoundary->SetHiddenInGame(true);
	ZoneBoundary->SetCanEverAffectNavigation(false);

	ZoneType = EZoneType::Battle;
	ZoneState = EZoneState::Locked;
}

////////////////////////////
//! \author HanUl
//! \brief Zone Bound의 Overlap 이벤트를 등록한다.
//! \param 
//! \return 
void AZoneBase::BeginPlay()
{
	Super::BeginPlay();

	if (ZoneBoundary)
	{
		ZoneBoundary->OnComponentBeginOverlap.AddUniqueDynamic(this, &AZoneBase::HandleZoneBoundaryBeginOverlap);
	}

	CollectClearConditions();
}

////////////////////////////
//! \author HanUl
//! \brief 배치된 Zone 인스턴스의 BoundaryExtent 변경을 Bound Collider에 반영한다.
//! \param Transform Actor의 현재 배치 Transform
//! \return 
void AZoneBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (ZoneBoundary)
	{
		ZoneBoundary->SetBoxExtent(BoundaryExtent);
	}
}

////////////////////////////
//! \author 준혁
//! \brief 월드 위치가 이 Zone의 경계(ZoneBoundary) 박스 안에 있는지 판정한다. 회전된 존도 정확히 검사하도록 로컬 좌표로 변환해 비교한다.
//! \param WorldLocation 판정할 월드 위치
//! \return 경계 안이면 true
bool AZoneBase::ContainsLocation(const FVector& WorldLocation) const
{
	if (!ZoneBoundary)
	{
		return false;
	}

	const FVector LocalLocation = ZoneBoundary->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = ZoneBoundary->GetUnscaledBoxExtent();

	return FMath::Abs(LocalLocation.X) <= Extent.X
		&& FMath::Abs(LocalLocation.Y) <= Extent.Y
		&& FMath::Abs(LocalLocation.Z) <= Extent.Z;
}

////////////////////////////
//! \author HanSeul
//! \brief 현재 Zone의 방어 또는 돌파 저지 목표가 실패했는지 확인한다.
//! \return 목표 실패가 확정되었으면 true
bool AZoneBase::HasEncounterFailed() const
{
	const UCPP_SpawnerComponent* Spawner = FindComponentByClass<UCPP_SpawnerComponent>();
	return IsValid(Spawner) && Spawner->HasObjectiveFailed();
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 플레이어 캐릭터가 Zone Bound에 들어오면 ZoneManager에게 진입을 보고한다. 상태 전환 결정은 Manager가 담당.
//! \param OverlappedComponent Overlap이 발생한 컴포넌트
//! \param OtherActor Zone에 진입한 Actor
//! \param OtherComp Zone에 진입한 Actor의 컴포넌트
//! \param OtherBodyIndex Overlap Body Index
//! \param bFromSweep Sweep 기반 Overlap 여부
//! \param SweepResult Sweep 결과
//! \return
void AZoneBase::HandleZoneBoundaryBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (!HasAuthority())
	{
		return;
	}

	const APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(OtherActor);
	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	APlayerState* EnteredPlayer = PlayerCharacter->GetPlayerState();
	if (!IsValid(EnteredPlayer))
	{
		return;
	}

	OnZonePlayerEntered.Broadcast(this, EnteredPlayer);
}

////////////////////////////
//! \author HanUl
//! \brief 배치된 Zone의 상태를 변경하고 상태별 실행 함수를 내부에서 디스패치한다. 같은 상태 재요청은 무시(멱등).
//! \param NewState 변경할 Zone 상태
//! \return
void AZoneBase::ChangeState(EZoneState NewState)
{
	if (ZoneState == NewState)
	{
		return;
	}

	const EZoneState OldZoneState = ZoneState;
	ZoneState = NewState;

	// 스트리밍 상태 조건이 "전투가 이어지는 동안"을 잴 수 있게 켜짐·꺼짐을 알린다.
	// 싸우는 존만 전투다. 상점과 퍼즐이 Active인 것은 전투가 아니다.
	const bool bIsCombatZone =
		ZoneType == EZoneType::Battle || ZoneType == EZoneType::Boss;
	if (HasAuthority() && bIsCombatZone)
	{
		const bool bWasCombat = OldZoneState == EZoneState::Active;
		const bool bIsCombat = NewState == EZoneState::Active;
		if (bWasCombat != bIsCombat)
		{
			MyStreamingState::BroadcastState(
				this, MyGameplayTags::Streaming_State_Combat, bIsCombat);
		}
	}

	switch (NewState)
	{
	case EZoneState::Preparing:
		ExecutePreparingState();
		break;
	case EZoneState::Ready:
		ExecuteReadyState();
		break;
	case EZoneState::Entering:
		ExecuteEnteringState();
		break;
	case EZoneState::Active:
		ExecuteActiveState();
		break;
	case EZoneState::Clear:
		ExecuteClearState();
		break;
	default:
		break;
	}
}


////////////////////////////
//! \author HanUl
//! \brief Preparing 전환 시 실행. 설정된 프리스폰 타이밍이 Preparing이면 초기 배치 적을 스폰한다.
//! \return
void AZoneBase::ExecutePreparingState()
{
	if (HasAuthority())
	{
		if (UCPP_SpawnerComponent* Spawner = FindComponentByClass<UCPP_SpawnerComponent>())
		{
			if (Spawner->HasAnyEncounterObjective())
			{
				bClearFinalizationPending = false;
				if (ClearCondition)
				{
					ClearCondition->ResetClearCondition();
				}
				Spawner->ResetSpawner();
			}
		}
	}

	ActivatePreSpawners();
}


////////////////////////////
//! \author HanUl
//! \brief Ready 전환 시 실행. 프리스폰 타이밍에 도달했고 아직 미실행이면 초기 배치 적을 스폰한다.
//! \return
void AZoneBase::ExecuteReadyState()
{
	ActivatePreSpawners();
}


////////////////////////////
//! \author HanUl
//! \brief Active 시 실행.
//! \return 
void AZoneBase::ExecuteActiveState()
{
	// 순서 중요: 클리어 조건이 먼저 스포너 스폰을 구독해야 첫 웨이브를 놓치지 않는다.
	if (ClearCondition)
	{
		ClearCondition->ActivateClearCondition();
	}

	ActivateSpawners();

	CloseEntrance();

	if (UCPP_ZoneFallingRockComponent* FallingRockComponent = FindComponentByClass<UCPP_ZoneFallingRockComponent>())
	{
		FallingRockComponent->StartFallingRocks();
	}
}

////////////////////////////
//! \author HanUl
//! \brief Entering 전환 시 실행. 입구를 개방하고, 프리스폰 타이밍에 도달했고 아직 미실행이면 초기 배치 적을 스폰한다.
//! \return
void AZoneBase::ExecuteEnteringState()
{
	OpenEntrance();
	ActivatePreSpawners();
}


////////////////////////////
//! \author HanUl
//! \brief Clear 시 실행. 스포너를 정지(잔존 적 일괄 사망 처리)하고 출구를 개방한다.
//! \return
void AZoneBase::ExecuteClearState()
{
	if (UCPP_ZoneFallingRockComponent* FallingRockComponent = FindComponentByClass<UCPP_ZoneFallingRockComponent>())
	{
		FallingRockComponent->StopFallingRocks();
	}

	if (ClearCondition)
	{
		ClearCondition->DeactivateClearCondition();
	}

	if (HasAuthority())
	{
		if (UCPP_SpawnerComponent* Spawner = FindComponentByClass<UCPP_SpawnerComponent>())
		{
			Spawner->DeactivateSpawner();
		}
	}

	OpenExit();
}

////////////////////////////
//! \author HanUl
//! \brief Clear 조건 컴포넌트가 만족되었을 때 Zone을 Clear 상태로 전환한다.
//! \param SatisfiedClearCondition 만족된 Clear 조건 컴포넌트
//! \return 
void AZoneBase::NotifyClearConditionSatisfied(UClearComponent* SatisfiedClearCondition)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(SatisfiedClearCondition) || SatisfiedClearCondition != ClearCondition)
	{
		return;
	}

	if (ZoneState != EZoneState::Active)
	{
		return;
	}

	UCPP_SpawnerComponent* Spawner = FindComponentByClass<UCPP_SpawnerComponent>();
	if (Spawner && Spawner->HasAnyEncounterObjective())
	{
		if (Spawner->HasObjectiveFailed() || bClearFinalizationPending)
		{
			return;
		}

		bClearFinalizationPending = true;
		GetWorldTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(
				this,
				&AZoneBase::FinalizeClearConditionSatisfied,
				SatisfiedClearCondition));
		return;
	}

	FinalizeClearConditionSatisfied(SatisfiedClearCondition);
}

////////////////////////////
//! \author HanSeul
//! \brief 거점 실패 여부를 마지막으로 확인한 뒤 Zone Clear 전환을 확정한다.
//! \param SatisfiedClearCondition 만족을 통지한 Clear 조건 컴포넌트
void AZoneBase::FinalizeClearConditionSatisfied(UClearComponent* SatisfiedClearCondition)
{
	bClearFinalizationPending = false;

	if (!HasAuthority()
		|| !IsValid(SatisfiedClearCondition)
		|| SatisfiedClearCondition != ClearCondition
		|| ZoneState != EZoneState::Active)
	{
		return;
	}

	if (const UCPP_SpawnerComponent* Spawner = FindComponentByClass<UCPP_SpawnerComponent>())
	{
		if (Spawner->HasObjectiveFailed())
		{
			return;
		}
	}

	ChangeState(EZoneState::Clear);
	OnZoneCleared.Broadcast(this);
}

#if !UE_BUILD_SHIPPING
////////////////////////////
//! \author 장효제
//! \brief 개발 검증을 위해 Clear 조건을 우회하되 실제 상태 변경과 OnZoneCleared 경로를 실행한다.
//! \param
//! \return 서버 권한에서 아직 Clear되지 않은 Zone을 강제 Clear했으면 true다.
bool AZoneBase::CheatForceClear()
{
	if (!HasAuthority()
		|| ZoneState == EZoneState::Clear
		|| ZoneState == EZoneState::Used)
	{
		return false;
	}

	ChangeState(EZoneState::Clear);
	OnZoneCleared.Broadcast(this);
	return true;
}
#endif

////////////////////////////
//! \author HanUl
//! \brief 신호를 받을 수 있는 Actor에게 Open Blueprint 이벤트를 전달한다.
//! \param TargetActor Open 신호를 받을 대상 Actor
//! \return
void AZoneBase::SendOpenSignal(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	if (!TargetActor->GetClass()->ImplementsInterface(UZoneSignalReceiver::StaticClass()))
	{
		return;
	}

	IZoneSignalReceiver::Execute_OnZoneOpen(TargetActor, this);
}

////////////////////////////
//! \author HanUl
//! \brief 신호를 받을 수 있는 Actor에게 Close Blueprint 이벤트를 전달한다.
//! \param TargetActor Close 신호를 받을 대상 Actor
//! \return
void AZoneBase::SendCloseSignal(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	if (!TargetActor->GetClass()->ImplementsInterface(UZoneSignalReceiver::StaticClass()))
	{
		return;
	}

	IZoneSignalReceiver::Execute_OnZoneClose(TargetActor, this);
}

////////////////////////////
//! \author HanUl
//! \brief BP에 등록된 종료 조건 컴포넌트 수집.
//! \return 
void AZoneBase::CollectClearConditions()
{
	ClearCondition = FindComponentByClass<UClearComponent>();

	if (!ClearCondition)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no ClearComponent."), *GetName());
		return;
	}

	ClearCondition->InitializeClearComponent(this);
	ClearCondition->OnClearConditionSatisfied.AddUniqueDynamic(this, &AZoneBase::HandleClearConditionSatisfied);
}

////////////////////////////
//! \author HanUl
//! \brief Clear 조건 컴포넌트의 만족 이벤트를 Zone 처리 함수로 전달한다.
//! \param SatisfiedClearCondition 만족된 Clear 조건 컴포넌트
//! \return 
void AZoneBase::HandleClearConditionSatisfied(UClearComponent* SatisfiedClearCondition)
{
	NotifyClearConditionSatisfied(SatisfiedClearCondition);
}

////////////////////////////
//! \author HanUl
//! \brief 입구 문 Actor에 Open 신호를 전달한다. (Entering 상태)
//! \return
void AZoneBase::OpenEntrance()
{
	SendOpenSignal(Entrance);
}

////////////////////////////
//! \author HanUl
//! \brief 입구 문 Actor에 Close 신호를 전달한다. (Active 상태)
//! \return
void AZoneBase::CloseEntrance()
{
	SendCloseSignal(Entrance);
}

////////////////////////////
//! \author HanUl
//! \brief 출구 문 Actor에 Open 신호를 전달한다. (Clear 상태)
//! \return
void AZoneBase::OpenExit()
{
	SendOpenSignal(Exit);
}

////////////////////////////
//! \author HanUl
//! \brief 출구 문 Actor에 Close 신호를 전달한다.
//! \return
void AZoneBase::CloseExit()
{
	SendCloseSignal(Exit);
}



////////////////////////////
//! \author HanUl
//! \brief 부착된 스포너에 현재 Zone 상태를 전달해 프리스폰을 시도한다. 스포너가 설정된 타이밍 이후 첫 상태에서 1회만 스폰한다. (서버 전용)
//! \return
void AZoneBase::ActivatePreSpawners()
{
	if (!HasAuthority())
	{
		return;
	}

	if (UCPP_SpawnerComponent* Spawner = FindComponentByClass<UCPP_SpawnerComponent>())
	{
		Spawner->TryPreSpawn(ZoneState);
	}
}

////////////////////////////
//! \author HanUl
//! \brief Active 상태에서 부착된 스포너 컴포넌트를 기동해 웨이브 스폰을 시작한다. (서버 전용)
//! \return
void AZoneBase::ActivateSpawners()
{
	if (!HasAuthority())
	{
		return;
	}

	if (UCPP_SpawnerComponent* Spawner = FindComponentByClass<UCPP_SpawnerComponent>())
	{
		Spawner->ActivateSpawner();
	}
}
