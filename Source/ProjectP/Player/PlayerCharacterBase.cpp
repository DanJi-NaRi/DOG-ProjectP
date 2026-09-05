#include "PlayerCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CurveTable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

// GAS
#include "../GAS/MyAttributeSet.h"
#include "../GAS/MyPlayerState.h"
#include "../MyGameplayTags.h"
#include "../Streaming/MyStreamingPayloads.h"

// Other Components
#include "Components/MyBasicControlComponent.h"
#include "Components/MySkillControlComponent.h"
#include "Components/PlayerCameraComponent.h"
#include "Components/PlayerInteractionComponent.h"
#include "Components/PlayerMovementComponent.h"
#include "Components/WeightCarryComponent.h"

namespace
{
	const TCHAR* LexToStringNetMode(ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone:
			return TEXT("Standalone");
		case NM_DedicatedServer:
			return TEXT("DedicatedServer");
		case NM_ListenServer:
			return TEXT("ListenServer");
		case NM_Client:
			return TEXT("Client");
		default:
			return TEXT("Unknown");
		}
	}
}



////////////////////////////
//! \author HanUl
//! \brief 플레이어 Character 공통 Avatar에 필요한 카메라와 이동 관련 기본 컴포넌트를 생성한다.
//! \param 없음
//! \return 없음
APlayerCharacterBase::APlayerCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	bUseControllerRotationYaw = true;
	CharacterMaterialSlotName = TEXT("Halo");
	//DefaultCharacterTags.AddTag(MyGameplayTags::Character_Player_Nefer.GetTag());
	DefaultCharacterTags.AddTag(MyGameplayTags::Character_Player.GetTag());

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Character100MaterialFinder(TEXT("/Game/ToonShade/Nefer/Material/MI_White_Halo_V10_R.MI_White_Halo_V10_R"));
	if (Character100MaterialFinder.Succeeded())
	{
		Character100Material = Character100MaterialFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Character200MaterialFinder(TEXT("/Game/ToonShade/Nefer/Material/MI_White_Halo_V10_G.MI_White_Halo_V10_G"));
	if (Character200MaterialFinder.Succeeded())
	{
		Character200Material = Character200MaterialFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Character300MaterialFinder(TEXT("/Game/ToonShade/Nefer/Material/MI_White_Halo_V10.MI_White_Halo_V10"));
	if (Character300MaterialFinder.Succeeded())
	{
		Character300Material = Character300MaterialFinder.Object;
	}

	if (UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement())
	{
		CharacterMovementComponent->bOrientRotationToMovement = false;
		CharacterMovementComponent->bUseControllerDesiredRotation = false;
		CharacterMovementComponent->MaxAcceleration = 8192.0f;
		CharacterMovementComponent->JumpZVelocity = 1700.0f;
		CharacterMovementComponent->GravityScale = 4.3f;
		CharacterMovementComponent->AirControl = 0.2f;
		CharacterMovementComponent->FallingLateralFriction = 4.0f;
		CharacterMovementComponent->BrakingDecelerationFalling = 800.0f;
	}

	// Camera Set
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = DefaultArmLength;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->bDoCollisionTest = false;     

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = 70.0f;

	// Component Set
	PlayerMovementComponent = CreateDefaultSubobject<UPlayerMovementComponent>(TEXT("PlayerMovementComponent"));
	PlayerCameraComponent = CreateDefaultSubobject<UPlayerCameraComponent>(TEXT("PlayerCameraComponent"));

	MyBasicControlComponent = CreateDefaultSubobject<UMyBasicControlComponent>(TEXT("MyBasicControlComponent"));
	MySkillControlComponent = CreateDefaultSubobject<UMySkillControlComponent>(TEXT("MySkillControlComponent"));
	PlayerInteractionComponent = CreateDefaultSubobject<UPlayerInteractionComponent>(TEXT("PlayerInteractionComponent"));
	WeightCarryComponent = CreateDefaultSubobject<UWeightCarryComponent>(TEXT("WeightCarryComponent"));
}



////////////////////////////
//! \author 박준혁
//! \brief 복제할 프로퍼티를 등록한다.
//! \param OutLifetimeProps 언리얼 네트워크 복제 시스템에 등록될 프로퍼티 목록
void APlayerCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APlayerCharacterBase, bReconnectInactive);
	DOREPLIFETIME(APlayerCharacterBase, SelectedCharacterId);
}

////////////////////////////
//! \author 장효제
//! \brief 현재 Character가 Avatar로 사용하는 AbilitySystemComponent를 반환한다.
//! \return 캐시된 AbilitySystemComponent 포인터
UAbilitySystemComponent* APlayerCharacterBase::GetAbilitySystemComponent() const
{
	return CachedAbilitySystemComponent;
}

////////////////////////////
//! \author 장효제
//! \brief 현재 Character가 참조 중인 MyGAS AttributeSet을 반환한다.
//! \return 캐시된 MyAttributeSet 포인터
UMyAttributeSet* APlayerCharacterBase::GetMyAttributeSet() const
{
	return CachedAttributeSet;
}

////////////////////////////
//! \author 장효제
//! \brief 현재 플레이어 체력을 반환한다.
//! \return 캐시된 AttributeSet의 Health, 없으면 0
float APlayerCharacterBase::GetHealth() const
{
	return CachedAttributeSet ? CachedAttributeSet->GetHealth() : 0.0f;
}

////////////////////////////
//! \author 장효제
//! \brief 현재 플레이어 최대 체력을 반환한다.
//! \return 캐시된 AttributeSet의 MaxHealth, 없으면 0
float APlayerCharacterBase::GetMaxHealth() const
{
	return CachedAttributeSet ? CachedAttributeSet->GetMaxHealth() : 0.0f;
}

////////////////////////////
//! \author 장효제
//! \brief UI ProgressBar에 사용할 현재 체력 비율을 반환한다.
//! \return Health / MaxHealth, MaxHealth가 0 이하이면 0
float APlayerCharacterBase::GetHealthPercent() const
{
	const float MaxHealth = GetMaxHealth();
	if (MaxHealth <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(GetHealth() / MaxHealth, 0.0f, 1.0f);
}

////////////////////////////
//! \author HanUl
//! \brief PlayerState가 소유한 현재 생명 상태를 반환한다.
//! \param 없음
//! \return PlayerState가 없으면 Alive, 있으면 복제된 생명 상태
EPlayerLifeState APlayerCharacterBase::GetLifeState() const
{
	const AMyPlayerState* MyPlayerState = GetPlayerState<AMyPlayerState>();
	return MyPlayerState ? MyPlayerState->GetLifeState() : EPlayerLifeState::Alive;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 캐릭터의 PlayerState가 Alive 상태인지 확인한다.
//! \param 없음
//! \return Alive 상태이면 true
bool APlayerCharacterBase::IsAlive() const
{
	return GetLifeState() == EPlayerLifeState::Alive;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 캐릭터의 PlayerState가 Dead 상태인지 확인한다. AnimBP 사망 전환 조건으로 사용할 수 있다.
//! \param 없음
//! \return Dead 상태이면 true
bool APlayerCharacterBase::IsDead() const
{
	return GetLifeState() == EPlayerLifeState::Dead;
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 Health를 0으로 설정하여 기존 PlayerState 사망 상태 전환 경로를 실행한다.
//! \param 없음
//! \return 없음
void APlayerCharacterBase::ForceKill()
{
	if (!HasAuthority() || IsDead() || !CachedAbilitySystemComponent)
	{
		return;
	}

	CachedAbilitySystemComponent->SetNumericAttributeBase(
		UMyAttributeSet::GetHealthAttribute(),
		0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 사망 순간 저장 위치로 동일 Pawn을 안전하게 이동시키고 Health 복구 후 Alive로 전환한다.
//! \param ReviveHealthPercent 최대 체력 대비 부활 체력 비율(0 초과 1 이하)
//! \return 서버에서 위치와 체력, 생명 상태 복구를 모두 완료하면 true
bool APlayerCharacterBase::ReviveAtLastDeathLocation(float ReviveHealthPercent)
{
	AMyPlayerState* MyPlayerState = GetPlayerState<AMyPlayerState>();
	if (!HasAuthority()
		|| !IsDead()
		|| !MyPlayerState
		|| !CachedAbilitySystemComponent
		|| !CachedAttributeSet
		|| !FMath::IsFinite(ReviveHealthPercent)
		|| ReviveHealthPercent <= 0.0f
		|| ReviveHealthPercent > 1.0f)
	{
		return false;
	}

	const FTransform ReviveTransform = bHasLastDeathTransform
		? LastDeathTransform
		: GetActorTransform();
	if (!TeleportTo(
		ReviveTransform.GetLocation(),
		ReviveTransform.Rotator(),
		/*bIsATest=*/false,
		/*bNoCheck=*/false))
	{
		return false;
	}

	if (UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement())
	{
		CharacterMovementComponent->StopMovementImmediately();
		CharacterMovementComponent->SetDefaultMovementMode();
	}

	const float MaxHealth = CachedAttributeSet->GetMaxHealth();
	if (MaxHealth <= 0.0f)
	{
		return false;
	}

	CachedAbilitySystemComponent->SetNumericAttributeBase(
		UMyAttributeSet::GetHealthAttribute(),
		MaxHealth * ReviveHealthPercent);
	MyPlayerState->SetLifeState(EPlayerLifeState::Alive);
	bHasLastDeathTransform = false;
	ForceNetUpdate();
	return true;
}

////////////////////////////
//! \author 박준혁
//! \brief 재접속 대기 중 인게임 영향에서 제외되는 비활성 상태를 설정한다.
//! \param bNewInactive true이면 재접속 대기 비활성 상태, false이면 일반 상태
void APlayerCharacterBase::SetReconnectInactive(bool bNewInactive)
{
	bReconnectInactive = bNewInactive;
}

////////////////////////////
//! \author 박준혁
//! \brief 재접속 대기 비활성 상태인지 확인한다.
//! \return 재접속 대기 비활성 상태 여부
bool APlayerCharacterBase::IsReconnectInactive() const
{
	return bReconnectInactive;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 캐릭터 선택 ID를 설정하고 해당 ID에 맞는 임시 머티리얼을 적용하는 함수
// NewSelectedCharacterId : 새로 선택된 캐릭터 ID
void APlayerCharacterBase::SetSelectedCharacterId(int32 NewSelectedCharacterId)
{
	if (!HasAuthority())
	{
		return;
	}

	if (NewSelectedCharacterId != -1 &&
		NewSelectedCharacterId != 100 &&
		NewSelectedCharacterId != 200 &&
		NewSelectedCharacterId != 300)
	{
		return;
	}

	if (SelectedCharacterId == NewSelectedCharacterId)
	{
		return;
	}

	SelectedCharacterId = NewSelectedCharacterId;
	ApplySelectedCharacterMaterial();
	ForceNetUpdate();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 캐릭터 선택 프리뷰를 위해 선택 캐릭터 ID에 맞는 임시 머티리얼을 즉시 적용하는 함수
// PreviewSelectedCharacterId : 프리뷰로 적용할 캐릭터 ID
void APlayerCharacterBase::PreviewSelectedCharacterMaterial(int32 PreviewSelectedCharacterId)
{
    if (PreviewSelectedCharacterId != 100 &&
        PreviewSelectedCharacterId != 200 &&
        PreviewSelectedCharacterId != 300)
    {
        return;
    }

    UMaterialInterface* SelectedMaterial = ResolveSelectedCharacterMaterial(PreviewSelectedCharacterId);
    if (!SelectedMaterial || !GetMesh() || CharacterMaterialSlotName.IsNone())
    {
        return;
    }

    const int32 MaterialIndex = GetMesh()->GetMaterialIndex(CharacterMaterialSlotName);
    if (MaterialIndex == INDEX_NONE)
    {
        return;
    }

    GetMesh()->SetMaterial(MaterialIndex, SelectedMaterial);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 캐릭터에 적용된 선택 캐릭터 ID를 반환하는 함수
// 반환값 : 현재 선택 캐릭터 ID, 선택되지 않았으면 -1
int32 APlayerCharacterBase::GetSelectedCharacterId() const
{
	return SelectedCharacterId;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 선택 캐릭터 ID가 복제되었을 때 외형용 임시 머티리얼을 갱신하는 함수
void APlayerCharacterBase::OnRep_SelectedCharacterId()
{
	ApplySelectedCharacterMaterial();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 선택 캐릭터 ID에 맞는 임시 머티리얼을 현재 Mesh의 지정 슬롯에 적용하는 함수
void APlayerCharacterBase::ApplySelectedCharacterMaterial()
{
	PreviewSelectedCharacterMaterial(SelectedCharacterId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 선택 캐릭터 ID에 대응하는 임시 머티리얼을 반환하는 함수
// InSelectedCharacterId : 머티리얼을 찾을 선택 캐릭터 ID
// 반환값 : 선택 캐릭터 ID에 대응하는 머티리얼, 없으면 nullptr
UMaterialInterface* APlayerCharacterBase::ResolveSelectedCharacterMaterial(int32 InSelectedCharacterId) const
{
	switch (InSelectedCharacterId)
	{
	case 100:
		return Character100Material;
	case 200:
		return Character200Material;
	case 300:
		return Character300Material;
	default:
		return nullptr;
	}
}

////////////////////////////
//! \author HanUl
//! \brief 카메라 기본값과 이동/카메라 컴포넌트 참조를 초기화한다.
//! \param 없음
//! \return 없음
void APlayerCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (CameraBoom)
	{
		CameraBoom->bUsePawnControlRotation = false;
		CameraBoom->SetUsingAbsoluteRotation(true);
		CameraBoom->SetWorldRotation(FRotator(-30.0f, -90.0f, 0.0f));
	}

	if (FollowCamera)
	{
		FollowCamera->bUsePawnControlRotation = false;
	}

	if (PlayerMovementComponent)
	{
		PlayerMovementComponent->InitializeMovement(this, FollowCamera, CameraBoom);
	}

	if (PlayerCameraComponent)
	{
		PlayerCameraComponent->InitializeCameraBoom(CameraBoom);
		PlayerCameraComponent->InitializeFollowCamera(FollowCamera);
	}

	if (MyBasicControlComponent)
	{
		MyBasicControlComponent->InitializeBasicControl(this, PlayerMovementComponent, PlayerCameraComponent, CameraBoom);
	}

	if (MySkillControlComponent)
	{
		MySkillControlComponent->InitializeSkillControl(CachedAbilitySystemComponent);
	}
}

////////////////////////////
//! \author HanUl
//! \brief Character 종료 시 PlayerState 생명 상태 델리게이트 구독을 해제한다.
//! \param EndPlayReason Character가 종료되는 이유
//! \return 없음
void APlayerCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && bCurseStateApplied)
	{
		ResetCurseStateOnServer();
	}
	else
	{
		ApplyCurseState(false);
	}

	UnbindCurseStateDelegate();
	UnbindLifeStateDelegate();
	Super::EndPlay(EndPlayReason);
}

////////////////////////////
//! \author 장효제
//! \brief 서버에서 Controller가 Possess된 뒤 PlayerState-owned ASC를 Avatar에 연결한다.
//! \param NewController 새로 소유한 Controller
//! \return 없음
void APlayerCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitAbilityActorInfo();
}

////////////////////////////
//! \author 장효제
//! \brief 클라이언트에서 PlayerState가 복제된 뒤 PlayerState-owned ASC를 Avatar에 연결한다.
void APlayerCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitAbilityActorInfo();
}

////////////////////////////
//! \author HanUl
//! \brief 기본 조작 입력 바인딩을 BasicControl 컴포넌트에 위임한다.
//! \param PlayerInputComponent 입력 바인딩 대상 컴포넌트
//! \return 없음
void APlayerCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (MyBasicControlComponent)
	{
		MyBasicControlComponent->BindBasicInput(PlayerInputComponent);
	}
	
	if (MySkillControlComponent) 
	{
		MySkillControlComponent->BindSkillInput(PlayerInputComponent);
	}
}

////////////////////////////
//! \author 장효제
//! \brief PlayerState-owned ASC를 현재 Character에 연결하고 서버에서 정체성 태그를 안전하게 부여한다.
//! \details 장효제: 스트리밍 시스템을 위한 Character 태그 부여 로직 추가.
//! \editor 준혁 (MoveSpeed 어트리뷰트 → CharacterMovement 연결 바인딩 추가)
//! \param 없음
//! \return 없음
void APlayerCharacterBase::InitAbilityActorInfo()
{
	AMyPlayerState* MyPlayerState = GetPlayerState<AMyPlayerState>();
	if (!MyPlayerState)
	{
		return;
	}

	UAbilitySystemComponent* ASC = MyPlayerState->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	ASC->InitAbilityActorInfo(MyPlayerState, this);
	ApplyDefaultCharacterTagsToAbilitySystem(ASC);

	CachedAbilitySystemComponent = ASC;
	CachedAttributeSet = MyPlayerState->GetMyAttributeSet();

	BindMoveSpeedToMovement(ASC);

	if (HasAuthority())
	{
		ApplyLevelStatsFromTable(MyPlayerState->GetCharacterLevel());
	}

	if (MySkillControlComponent)
	{
		MySkillControlComponent->InitializeSkillControl(ASC);
	}

	BindLifeStateDelegate(MyPlayerState);
	BindCurseStateDelegate(MyPlayerState);
}

////////////////////////////
//! \author HanUl
//! \brief 현재 PlayerState의 생명 상태 변경을 구독하고 이미 복제된 상태도 즉시 Character에 적용한다.
//! \param MyPlayerState 구독할 PlayerState
//! \return 없음
void APlayerCharacterBase::BindLifeStateDelegate(AMyPlayerState* MyPlayerState)
{
	if (!MyPlayerState)
	{
		return;
	}

	UnbindLifeStateDelegate();
	BoundLifeStatePlayerState = MyPlayerState;
	LifeStateChangedDelegateHandle = MyPlayerState->OnLifeStateChanged.AddUObject(
		this,
		&APlayerCharacterBase::HandleLifeStateChanged);

	ApplyLifeState(MyPlayerState->GetLifeState());
}

////////////////////////////
//! \author HanUl
//! \brief 현재 PlayerState에 등록된 생명 상태 변경 구독을 해제한다.
//! \param 없음
//! \return 없음
void APlayerCharacterBase::UnbindLifeStateDelegate()
{
	if (BoundLifeStatePlayerState.IsValid() && LifeStateChangedDelegateHandle.IsValid())
	{
		BoundLifeStatePlayerState->OnLifeStateChanged.Remove(LifeStateChangedDelegateHandle);
	}

	BoundLifeStatePlayerState.Reset();
	LifeStateChangedDelegateHandle.Reset();
}

////////////////////////////
//! \author HanSeul
//! \brief 현재 PlayerState의 저주 상태 변경을 구독하고 이미 복제된 상태도 즉시 Character에 적용한다.
//! \param MyPlayerState 구독할 PlayerState
//! \return 없음
void APlayerCharacterBase::BindCurseStateDelegate(AMyPlayerState* MyPlayerState)
{
	if (!MyPlayerState)
	{
		return;
	}

	UnbindCurseStateDelegate();
	BoundCurseStatePlayerState = MyPlayerState;
	CurseStateChangedDelegateHandle = MyPlayerState->OnCurseStateChanged.AddUObject(
		this,
		&APlayerCharacterBase::HandleCurseStateChanged);

	ApplyCurseState(MyPlayerState->IsCursed());
}

////////////////////////////
//! \author HanSeul
//! \brief 현재 PlayerState에 등록된 저주 상태 변경 구독을 해제한다.
//! \param 없음
//! \return 없음
void APlayerCharacterBase::UnbindCurseStateDelegate()
{
	if (BoundCurseStatePlayerState.IsValid() && CurseStateChangedDelegateHandle.IsValid())
	{
		BoundCurseStatePlayerState->OnCurseStateChanged.Remove(CurseStateChangedDelegateHandle);
	}

	BoundCurseStatePlayerState.Reset();
	CurseStateChangedDelegateHandle.Reset();
}

////////////////////////////
//! \author HanSeul
//! \brief PlayerState 저주 상태 변경을 현재 Character의 임시 테스트 행동에 반영한다.
//! \param bWasCursed 변경 전 저주 상태
//! \param bIsCursed 변경 후 저주 상태
//! \return 없음
void APlayerCharacterBase::HandleCurseStateChanged(bool bWasCursed, bool bIsCursed)
{
	(void)bWasCursed;
	ApplyCurseState(bIsCursed);
}

////////////////////////////
//! \author HanSeul
//! \brief 저주 상태 동안 진행 중 행동을 정리하고 기존 이동·스킬 입력 차단 태그를 임시 적용한다.
//! \param bIsCursed true이면 임시 정지를 시작하고 false이면 종료한다.
//! \return 없음
void APlayerCharacterBase::ApplyCurseState(bool bIsCursed)
{
	if (bIsCursed)
	{
		if (bCurseStateApplied)
		{
			return;
		}

		bCurseStateApplied = true;

		if (CachedAbilitySystemComponent && (HasAuthority() || IsLocallyControlled()))
		{
			CachedAbilitySystemComponent->CancelAllAbilities();
		}

		StopJumping();
		ConsumeMovementInputVector();

		if (PlayerMovementComponent)
		{
			PlayerMovementComponent->CancelAllFacingRequests();
		}

		if (HasAuthority() && CachedAbilitySystemComponent)
		{
			CachedAbilitySystemComponent->AddLooseGameplayTag(
				MyGameplayTags::State_Skill_BlockMoveInput,
				1,
				EGameplayTagReplicationState::TagOnly);
			CachedAbilitySystemComponent->AddLooseGameplayTag(
				MyGameplayTags::State_Skill_BlockSkillInput,
				1,
				EGameplayTagReplicationState::TagOnly);

			GetWorldTimerManager().SetTimer(
				TemporaryCurseTimerHandle,
				this,
				&APlayerCharacterBase::HandleTemporaryCurseExpired,
				FMath::Max(CurseTestDuration, 0.1f),
				false);
		}
		return;
	}

	if (!bCurseStateApplied)
	{
		return;
	}

	bCurseStateApplied = false;
	GetWorldTimerManager().ClearTimer(TemporaryCurseTimerHandle);

	if (HasAuthority() && CachedAbilitySystemComponent)
	{
		CachedAbilitySystemComponent->RemoveLooseGameplayTag(
			MyGameplayTags::State_Skill_BlockMoveInput,
			1,
			EGameplayTagReplicationState::TagOnly);
		CachedAbilitySystemComponent->RemoveLooseGameplayTag(
			MyGameplayTags::State_Skill_BlockSkillInput,
			1,
			EGameplayTagReplicationState::TagOnly);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 임시 저주 테스트 시간이 끝나면 서버 저주 상태와 게이지를 초기화한다.
//! \param 없음
//! \return 없음
void APlayerCharacterBase::HandleTemporaryCurseExpired()
{
	ResetCurseStateOnServer();
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 저주 게이지를 0으로 만들고 PlayerState의 저주 상태를 종료한다.
//! \param 없음
//! \return 없음
void APlayerCharacterBase::ResetCurseStateOnServer()
{
	if (!HasAuthority())
	{
		return;
	}

	if (CachedAbilitySystemComponent)
	{
		CachedAbilitySystemComponent->SetNumericAttributeBase(
			UMyAttributeSet::GetCurseGaugeAttribute(),
			0.0f);
	}

	if (AMyPlayerState* MyPlayerState = GetPlayerState<AMyPlayerState>())
	{
		MyPlayerState->SetCurseState(false);
	}
}

////////////////////////////
//! \author HanUl
//! \brief PlayerState 생명 상태 변경을 현재 Character의 게임플레이 상태에 반영한다.
//! \param OldLifeState 변경 전 생명 상태
//! \param NewLifeState 변경 후 생명 상태
//! \return 없음
void APlayerCharacterBase::HandleLifeStateChanged(EPlayerLifeState OldLifeState, EPlayerLifeState NewLifeState)
{
	(void)OldLifeState;
	ApplyLifeState(NewLifeState);
}

////////////////////////////
//! \author HanUl
//! \brief Dead 상태에서는 진행 중 Ability와 조작 입력, 상호작용을 정리하되 중력·낙하·충돌 물리는 유지한다.
//! \param NewLifeState 적용할 생명 상태
//! \return 없음
void APlayerCharacterBase::ApplyLifeState(EPlayerLifeState NewLifeState)
{
	const bool bShouldBeDead = NewLifeState == EPlayerLifeState::Dead;
	if (bShouldBeDead && HasAuthority() && !bDeathStateApplied)
	{
		LastDeathTransform = GetActorTransform();
		bHasLastDeathTransform = true;
	}

	if (PlayerInteractionComponent)
	{
		PlayerInteractionComponent->HandleOwnerLifeStateChanged(bShouldBeDead);
	}

	if (WeightCarryComponent)
	{
		WeightCarryComponent->HandleOwnerLifeStateChanged(bShouldBeDead);
	}

	if (MyBasicControlComponent)
	{
		MyBasicControlComponent->HandleOwnerLifeStateChanged(bShouldBeDead);
	}

	if (PlayerCameraComponent)
	{
		PlayerCameraComponent->HandleOwnerLifeStateChanged(bShouldBeDead);
	}

	if (bShouldBeDead)
	{
		if (bDeathStateApplied)
		{
			return;
		}

		bDeathStateApplied = true;

		if (HasAuthority())
		{
			ResetCurseStateOnServer();
			// 스트리밍 조건이 셀 수 있도록 사망 사실을 남긴다. bDeathStateApplied가
			// 한 번만 통과시키므로 부활 후 재사망도 정확히 한 번씩 센다.
			MyStreamingCountEvent::BroadcastCountEvent(
				this,
				MyGameplayTags::Streaming_Event_World_PlayerDied,
				FGameplayTag());
		}

		if (CachedAbilitySystemComponent && (HasAuthority() || IsLocallyControlled()))
		{
			CachedAbilitySystemComponent->CancelAllAbilities();
		}

		StopJumping();
		ConsumeMovementInputVector();

		if (PlayerMovementComponent)
		{
			PlayerMovementComponent->CancelAllFacingRequests();
		}
		return;
	}

	if (!bDeathStateApplied)
	{
		return;
	}

	bDeathStateApplied = false;
}

////////////////////////////
//! \author 장효제
//! \brief 서버 권한 ASC에 DefaultCharacterTags를 Loose Tag로 중복 없이 부여한다.
//! \details 장효제: Faction 태그와 분리된 스트리밍용 Character 정체성 태그를 관리한다.
//! \param ASC 태그를 부여할 AbilitySystemComponent
//! \return 없음
void APlayerCharacterBase::ApplyDefaultCharacterTagsToAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (!HasAuthority() || !ASC)
	{
		return;
	}

	TArray<FGameplayTag> CharacterTags;
	DefaultCharacterTags.GetGameplayTagArray(CharacterTags);

	for (const FGameplayTag& CharacterTag : CharacterTags)
	{
		if (!CharacterTag.IsValid() || ASC->HasMatchingGameplayTag(CharacterTag))
		{
			continue;
		}

		ASC->AddLooseGameplayTag(CharacterTag, 1, EGameplayTagReplicationState::TagOnly);
	}
}

////////////////////////////
//! \author 준혁
//! \brief MoveSpeed 어트리뷰트를 CharacterMovement의 MaxWalkSpeed에 연결한다.
//!        어트리뷰트가 모든 클라이언트로 복제되므로 서버/오너/시뮬 프록시가 같은 값으로 갱신되어
//!        무브먼트 예측 불일치(러버밴딩)가 생기지 않는다. 바인딩 시점의 현재 값도 즉시 반영한다.
//! \param InASC 바인딩할 어빌리티 시스템 컴포넌트
//! \return 없음
void APlayerCharacterBase::BindMoveSpeedToMovement(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}

	// 리스폰/재초기화로 다시 들어와도 같은 ASC에 중복 바인딩되지 않게 기존 핸들을 정리한다
	if (MoveSpeedChangedDelegateHandle.IsValid())
	{
		InASC->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetMoveSpeedAttribute()).Remove(MoveSpeedChangedDelegateHandle);
		MoveSpeedChangedDelegateHandle.Reset();
	}

	MoveSpeedChangedDelegateHandle = InASC->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetMoveSpeedAttribute())
		.AddUObject(this, &APlayerCharacterBase::OnMoveSpeedAttributeChanged);

	ApplyMoveSpeedToMovement(InASC->GetNumericAttribute(UMyAttributeSet::GetMoveSpeedAttribute()));
}

////////////////////////////
//! \author 준혁
//! \brief MoveSpeed 어트리뷰트 변경 콜백. 새 값을 이동 컴포넌트에 반영한다.
//! \param Data 어트리뷰트 변경 데이터(NewValue 사용)
//! \return 없음
void APlayerCharacterBase::OnMoveSpeedAttributeChanged(const FOnAttributeChangeData& Data)
{
	ApplyMoveSpeedToMovement(Data.NewValue);
}

////////////////////////////
//! \author 준혁
//! \brief CharacterMovement의 MaxWalkSpeed를 설정한다. 0 이하 값은 무시한다(어트리뷰트 미초기화 보호).
//! \param NewMoveSpeed 적용할 이동 속도
//! \return 없음
void APlayerCharacterBase::ApplyMoveSpeedToMovement(float NewMoveSpeed)
{
	if (NewMoveSpeed <= 0.0f)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = NewMoveSpeed;
	}
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 캐릭터 레벨을 설정하고 레벨 스탯 CurveTable 값을 어트리뷰트에 적용한다.
//! \param NewLevel 새 캐릭터 레벨(1~MaxCharacterLevel로 보정)
//! \return 없음
void APlayerCharacterBase::SetCharacterLevel(int32 NewLevel)
{
	if (!HasAuthority())
	{
		return;
	}

	AMyPlayerState* MyPlayerState = GetPlayerState<AMyPlayerState>();
	if (!MyPlayerState)
	{
		return;
	}

	const int32 PreviousLevel = MyPlayerState->GetCharacterLevel();
	const int32 ClampedLevel = FMath::Clamp(NewLevel, 1, MaxCharacterLevel);
	MyPlayerState->SetCharacterLevel(ClampedLevel);
	ApplyLevelStatsFromTable(ClampedLevel);

	// HanUl: 레벨 상승분(양수)만큼 스킬포인트를 지급하되, 누적 지급 총량은 MaxTotalSkillPoints(강화 총 횟수)까지로 제한한다.
	//        XP 레벨업·치트(/level)·직접 설정 등 모든 경로가 이 함수를 거치므로 지급을 여기서 일원화한다. 레벨 하락 시엔 지급/회수하지 않는다.
	const int32 GainedLevels = ClampedLevel - PreviousLevel;
	if (GainedLevels > 0 && SkillPointsPerLevelUp > 0)
	{
		const int32 RequestedPoints = GainedLevels * SkillPointsPerLevelUp;
		const int32 RemainingGrantable = FMath::Max(0, MaxTotalSkillPoints - MyPlayerState->GetTotalSkillPointsGranted());
		const int32 PointsToGrant = FMath::Min(RequestedPoints, RemainingGrantable);
		if (PointsToGrant > 0)
		{
			MyPlayerState->AddSkillPoints(PointsToGrant);
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief PlayerState에 저장된 현재 캐릭터 레벨을 반환한다.
//! \param 없음
//! \return 캐릭터 레벨, PlayerState가 없으면 1
int32 APlayerCharacterBase::GetCharacterLevel() const
{
	const AMyPlayerState* MyPlayerState = GetPlayerState<AMyPlayerState>();
	return MyPlayerState ? MyPlayerState->GetCharacterLevel() : 1;
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 경험치를 추가하고, Exp 커브의 요구량을 채우면 레벨업한다. 잔여 경험치는 이월된다.
//! \param Amount 추가할 경험치(0 이하 무시)
//! \return 없음
void APlayerCharacterBase::AddExperience(int32 Amount)
{
	if (!HasAuthority() || Amount <= 0)
	{
		return;
	}

	AMyPlayerState* MyPlayerState = GetPlayerState<AMyPlayerState>();
	if (!MyPlayerState)
	{
		return;
	}

	int32 CurrentLevel = MyPlayerState->GetCharacterLevel();
	if (CurrentLevel >= MaxCharacterLevel)
	{
		return;
	}

	const FRealCurve* ExpCurve = FindLevelCurveBySuffix(TEXT("Exp"));
	if (!ExpCurve)
	{
		UE_LOG(LogTemp, Warning, TEXT("Player add experience skipped - Exp curve is missing. Character: %s, Table: %s"),
			*GetNameSafe(this),
			*GetNameSafe(LevelStatTable));
		return;
	}

	int32 CurrentExp = MyPlayerState->GetCharacterExp() + Amount;
	int32 RequiredExp = FMath::Max(FMath::RoundToInt(ExpCurve->Eval(static_cast<float>(CurrentLevel))), 1);

	const int32 LevelBefore = CurrentLevel;
	while (CurrentLevel < MaxCharacterLevel && CurrentExp >= RequiredExp)
	{
		CurrentExp -= RequiredExp;
		++CurrentLevel;
		RequiredExp = FMath::Max(FMath::RoundToInt(ExpCurve->Eval(static_cast<float>(CurrentLevel))), 1);
	}

	// 최대 레벨 도달 시 잔여 경험치는 버린다.
	if (CurrentLevel >= MaxCharacterLevel)
	{
		CurrentExp = 0;
	}

	MyPlayerState->SetCharacterExp(CurrentExp);

	if (CurrentLevel != LevelBefore)
	{
		// 스킬포인트 지급은 SetCharacterLevel로 일원화되어 있다(레벨 상승분 기준).
		SetCharacterLevel(CurrentLevel);
	}

	UE_LOG(LogTemp, Log, TEXT("Player experience added - Character: %s, Amount: %d, Level: %d -> %d, Exp: %d/%d"),
		*GetNameSafe(this),
		Amount,
		LevelBefore,
		CurrentLevel,
		CurrentExp,
		CurrentLevel < MaxCharacterLevel ? RequiredExp : 0);
}

////////////////////////////
//! \author HanUl
//! \brief 현재 레벨에서 다음 레벨까지 필요한 경험치 요구량을 반환한다.
//! \param 없음
//! \return 요구 경험치. 최대 레벨이거나 Exp 커브가 없으면 0
int32 APlayerCharacterBase::GetExpRequiredForNextLevel() const
{
	const int32 CurrentLevel = GetCharacterLevel();
	if (CurrentLevel >= MaxCharacterLevel)
	{
		return 0;
	}

	const FRealCurve* ExpCurve = FindLevelCurveBySuffix(TEXT("Exp"));
	return ExpCurve ? FMath::Max(FMath::RoundToInt(ExpCurve->Eval(static_cast<float>(CurrentLevel))), 0) : 0;
}

////////////////////////////
//! \author HanUl
//! \brief 레벨 스탯 CurveTable에서 행 이름이 접미사로 끝나는 커브를 찾는다.
//! \param Suffix 행 이름 접미사(예: Exp, MaxHealth)
//! \return 커브 포인터, 테이블이 없거나 행이 없으면 nullptr
const FRealCurve* APlayerCharacterBase::FindLevelCurveBySuffix(const TCHAR* Suffix) const
{
	if (!LevelStatTable)
	{
		return nullptr;
	}

	for (const TPair<FName, FRealCurve*>& Row : LevelStatTable->GetRowMap())
	{
		if (Row.Key.ToString().EndsWith(Suffix))
		{
			return Row.Value;
		}
	}

	return nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 레벨 스탯 CurveTable에서 MaxHealth/Attack/Defense 값을 읽어 어트리뷰트 Base에 적용한다.
//!        MaxHealth 변경 시 현재 체력의 비율을 유지한다.
//! \editor 준혁 (치명타 확률 클램프 상한을 CritChanceCap(50%)으로 통일)
//! \param Level 적용할 캐릭터 레벨(1~MaxCharacterLevel로 보정)
//! \return 없음
void APlayerCharacterBase::ApplyLevelStatsFromTable(int32 Level)
{
	if (!HasAuthority() || !CachedAbilitySystemComponent || !CachedAttributeSet)
	{
		return;
	}

	const UCurveTable* StatTable = LevelStatTable;
	if (!StatTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("Player level stats skipped - LevelStatTable is not set. Character: %s, Level: %d"),
			*GetNameSafe(this),
			Level);
		return;
	}

	const FRealCurve* MaxHealthCurve = nullptr;
	const FRealCurve* AttackCurve = nullptr;
	const FRealCurve* DefenseCurve = nullptr;
	for (const TPair<FName, FRealCurve*>& Row : StatTable->GetRowMap())
	{
		const FString RowName = Row.Key.ToString();
		if (RowName.EndsWith(TEXT("MaxHealth")))
		{
			MaxHealthCurve = Row.Value;
		}
		else if (RowName.EndsWith(TEXT("Attack")))
		{
			AttackCurve = Row.Value;
		}
		else if (RowName.EndsWith(TEXT("Defense")))
		{
			DefenseCurve = Row.Value;
		}
	}

	if (!MaxHealthCurve || !AttackCurve || !DefenseCurve)
	{
		UE_LOG(LogTemp, Warning, TEXT("Player level stats row missing - Table: %s, MaxHealth: %s, Attack: %s, Defense: %s"),
			*GetNameSafe(StatTable),
			MaxHealthCurve ? TEXT("found") : TEXT("missing"),
			AttackCurve ? TEXT("found") : TEXT("missing"),
			DefenseCurve ? TEXT("found") : TEXT("missing"));
	}

	const float EvalLevel = static_cast<float>(FMath::Clamp(Level, 1, MaxCharacterLevel));

	// 레벨업으로 MaxHealth가 변해도 현재 체력의 비율을 유지한다.
	const float OldMaxHealth = CachedAttributeSet->GetMaxHealth();
	const float HealthRatio = OldMaxHealth > 0.0f
		? FMath::Clamp(CachedAttributeSet->GetHealth() / OldMaxHealth, 0.0f, 1.0f)
		: 1.0f;

	if (MaxHealthCurve)
	{
		const float NewMaxHealth = MaxHealthCurve->Eval(EvalLevel);
		CachedAbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetMaxHealthAttribute(), NewMaxHealth);
		CachedAbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetHealthAttribute(), NewMaxHealth * HealthRatio);
	}

	if (AttackCurve)
	{
		CachedAbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetAttackPowerAttribute(), AttackCurve->Eval(EvalLevel));
	}

	if (DefenseCurve)
	{
		CachedAbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetDefenseAttribute(), DefenseCurve->Eval(EvalLevel));
	}

	// 선택 행: Critical(확률 %, 15 = 15%) / CriticalDamage(배율 %, 200 = 2.0배). 행이 없으면 기본값을 유지한다.
	if (const FRealCurve* CriticalCurve = FindLevelCurveBySuffix(TEXT("Critical")))
	{
		const float CritChance = FMath::Clamp(CriticalCurve->Eval(EvalLevel) / 100.0f, 0.0f, UMyAttributeSet::CritChanceCap);
		CachedAbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetCritChanceAttribute(), CritChance);
	}

	if (const FRealCurve* CriticalDamageCurve = FindLevelCurveBySuffix(TEXT("CriticalDamage")))
	{
		const float CritDamage = FMath::Max(CriticalDamageCurve->Eval(EvalLevel) / 100.0f, 1.0f);
		CachedAbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetCritDamageAttribute(), CritDamage);
	}

	// 선택 행: Speed(이동 속도, cm/s). MoveSpeed 어트리뷰트 복제로 전 클라이언트의 MaxWalkSpeed에 반영된다.
	if (const FRealCurve* SpeedCurve = FindLevelCurveBySuffix(TEXT("Speed")))
	{
		const float NewMoveSpeed = SpeedCurve->Eval(EvalLevel);
		if (NewMoveSpeed > 0.0f)
		{
			CachedAbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetMoveSpeedAttribute(), NewMoveSpeed);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Player level stats Speed row ignored - non-positive value. Character: %s, Level: %.0f, Value: %.1f"),
				*GetNameSafe(this),
				EvalLevel,
				NewMoveSpeed);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Player level stats applied - Character: %s, Level: %.0f, Table: %s, MaxHealth: %.1f, Health: %.1f, AttackPower: %.1f, Defense: %.1f, CritChance: %.2f, CritDamage: x%.2f, MoveSpeed: %.1f"),
		*GetNameSafe(this),
		EvalLevel,
		*GetNameSafe(StatTable),
		CachedAttributeSet->GetMaxHealth(),
		CachedAttributeSet->GetHealth(),
		CachedAttributeSet->GetAttackPower(),
		CachedAttributeSet->GetDefense(),
		CachedAttributeSet->GetCritChance(),
		CachedAttributeSet->GetCritDamage(),
		CachedAttributeSet->GetMoveSpeed());
}

