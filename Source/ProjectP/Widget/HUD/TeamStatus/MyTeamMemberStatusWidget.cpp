#include "MyTeamMemberStatusWidget.h"

#include "AbilitySystemComponent.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerState.h"
#include "../../../GAS/MyAttributeSet.h"
#include "../../../GAS/MyPlayerState.h"
#include "../../../Player/PlayerCharacterBase.h"
#include "../PlayerStatus/MyExpBarWidget.h"
#include "../PlayerStatus/MyHPBarWidget.h"

//////////////////////////////////////////////////////////////////////
// - Codex -
// 디자이너 미리보기와 파티원 전용 바 스타일을 적용하는 함수
void UMyTeamMemberStatusWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    ApplyBarStyle();

    if (IsDesignTime())
    {
        ApplyPreviewStatus();
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 런타임 생성 시 파티원 UI의 숫자 텍스트를 숨기고 바 스타일을 적용하는 함수
void UMyTeamMemberStatusWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ApplyBarStyle();

    if (HPBar)
    {
        HPBar->SetValueTextVisible(false);
    }

    if (ExpBar)
    {
        ExpBar->SetValueTextVisible(false);
    }

    if (!BoundPlayerState)
    {
        bShowingRuntimePreview = true;
        ApplyPreviewStatus();
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 위젯 제거 시 PlayerState와 ASC에 연결된 델리게이트를 정리하는 함수
void UMyTeamMemberStatusWidget::NativeDestruct()
{
    UnbindFromPlayerState();
    Super::NativeDestruct();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 파티원 패널이 담당할 캐릭터 ID를 슬롯에 고정하고 연결 대기 상태로 초기화하는 함수
// InCharacterId : 슬롯에 고정할 캐릭터 ID
void UMyTeamMemberStatusWidget::InitializeCharacterSlot(int32 InCharacterId)
{
    if (InCharacterId != 100 &&
        InCharacterId != 200 &&
        InCharacterId != 300)
    {
        return;
    }

    if (CachedCharacterId == InCharacterId)
    {
        return;
    }

    UnbindFromPlayerState();

    bShowingRuntimePreview = false;
    CachedUserIndex = -1;
    CachedCharacterId = InCharacterId;
    CachedLevel = 1;
    CachedUsername.Reset();
    bDisconnected = true;

    RefreshPlayerInfo();
    RefreshLifePresentation();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 표시할 파티원의 PlayerState와 UI를 연결하는 함수
// InPlayerState : 표시 대상 파티원의 PlayerState
// Return Value : PlayerState 연결 성공 여부
bool UMyTeamMemberStatusWidget::BindToPlayerState(AMyPlayerState* InPlayerState)
{
    if (!IsValid(InPlayerState))
    {
        return false;
    }

    bShowingRuntimePreview = false;

    if (BoundPlayerState == InPlayerState)
    {
        bDisconnected = false;
        CacheIdentityFromPlayerState();
        BindToAbilitySystemComponent();
        RefreshAll(false);
        return true;
    }

    UnbindFromPlayerState();

    BoundPlayerState = InPlayerState;
    bDisconnected = false;
    CacheIdentityFromPlayerState();

    LevelDataChangedHandle = BoundPlayerState->OnLevelDataChanged.AddUObject(
        this,
        &ThisClass::HandleLevelDataChanged);
    LifeStateChangedHandle = BoundPlayerState->OnLifeStateChanged.AddUObject(
        this,
        &ThisClass::HandleLifeStateChanged);
    BoundPlayerState->OnPawnSet.AddUniqueDynamic(this, &ThisClass::HandlePawnSet);

    BindToAbilitySystemComponent();
    RefreshAll(false);
    return true;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 파티원 접속 종료 상태를 표시하고 마지막 신원 정보는 유지하는 함수
void UMyTeamMemberStatusWidget::MarkDisconnected()
{
    UnbindFromPlayerState();
    bShowingRuntimePreview = false;
    bDisconnected = true;
    RefreshPlayerInfo();
    RefreshLifePresentation();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 슬롯에 저장된 파티원의 유저 인덱스를 반환하는 함수
// Return Value : 파티원 UserIndex, 저장된 정보가 없으면 -1
int32 UMyTeamMemberStatusWidget::GetTeamMemberUserIndex() const
{
    return CachedUserIndex;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 슬롯에 저장된 파티원의 선택 캐릭터 ID를 반환하는 함수
// Return Value : 선택 캐릭터 ID, 저장된 정보가 없으면 -1
int32 UMyTeamMemberStatusWidget::GetTeamMemberCharacterId() const
{
    return CachedCharacterId;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 슬롯이 지정한 PlayerState에 연결되어 있는지 확인하는 함수
// InPlayerState : 비교할 PlayerState
// Return Value : 같은 PlayerState에 연결되어 있으면 true
bool UMyTeamMemberStatusWidget::IsBoundToPlayerState(const AMyPlayerState* InPlayerState) const
{
    return BoundPlayerState == InPlayerState;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 블루프린트 디자이너에서 사용할 파티원 상태 미리보기를 적용하는 함수
void UMyTeamMemberStatusWidget::ApplyPreviewStatus()
{
    CachedUsername = PreviewUsername;
    CachedLevel = FMath::Max(PreviewLevel, 1);
    CachedCharacterId = PreviewCharacterId;
    bDisconnected = bPreviewDown;

    if (HPBar)
    {
        HPBar->SetValueTextVisible(false);
        HPBar->SetHPInstant(PreviewCurrentHP, PreviewMaxHP);
        HPBar->SetShield(PreviewShield, false);
    }

    if (ExpBar)
    {
        ExpBar->SetValueTextVisible(false);
        ExpBar->SetExpInstant(PreviewCurrentExp, PreviewRequiredExp);
    }

    RefreshPlayerInfo();
    RefreshLifePresentation();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 파티원 전용 HP와 경험치 텍스처를 공용 바 위젯에 적용하는 함수
void UMyTeamMemberStatusWidget::ApplyBarStyle()
{
    if (HPBar)
    {
        HPBar->SetBarTextures(
            TeamNormalHPTexture,
            TeamDangerHPTexture,
            TeamHPBackgroundTexture,
            TeamShieldTexture);
    }

    if (ExpBar)
    {
        ExpBar->SetBarTextures(
            TeamExpFillTexture,
            TeamExpBackgroundTexture);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 연결된 PlayerState에서 슬롯 고정에 필요한 신원 정보를 캐시하는 함수
void UMyTeamMemberStatusWidget::CacheIdentityFromPlayerState()
{
    if (!BoundPlayerState)
    {
        return;
    }

    CachedUserIndex = BoundPlayerState->GetUserIndex();
    CachedCharacterId = BoundPlayerState->GetSelectedCharacterId();
    CachedLevel = FMath::Max(BoundPlayerState->GetCharacterLevel(), 1);
    CachedUsername = BoundPlayerState->GetUsername();

    if (CachedUsername.IsEmpty())
    {
        CachedUsername = BoundPlayerState->GetPlayerName();
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 연결된 PlayerState의 ASC에 HP, 최대 HP, 쉴드 변경 델리게이트를 연결하는 함수
void UMyTeamMemberStatusWidget::BindToAbilitySystemComponent()
{
    UAbilitySystemComponent* NewAbilitySystemComponent = BoundPlayerState
        ? BoundPlayerState->GetAbilitySystemComponent()
        : nullptr;
    if (BoundAbilitySystemComponent == NewAbilitySystemComponent)
    {
        return;
    }

    UnbindFromAbilitySystemComponent();
    BoundAbilitySystemComponent = NewAbilitySystemComponent;
    if (!BoundAbilitySystemComponent)
    {
        return;
    }

    HealthChangedHandle = BoundAbilitySystemComponent
        ->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetHealthAttribute())
        .AddUObject(this, &ThisClass::HandleHealthChanged);
    MaxHealthChangedHandle = BoundAbilitySystemComponent
        ->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetMaxHealthAttribute())
        .AddUObject(this, &ThisClass::HandleMaxHealthChanged);
    ShieldChangedHandle = BoundAbilitySystemComponent
        ->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetShieldAttribute())
        .AddUObject(this, &ThisClass::HandleShieldChanged);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// PlayerState의 레벨, 생명 상태, Pawn 변경 델리게이트 연결을 해제하는 함수
void UMyTeamMemberStatusWidget::UnbindFromPlayerState()
{
    UnbindFromAbilitySystemComponent();

    if (BoundPlayerState)
    {
        if (LevelDataChangedHandle.IsValid())
        {
            BoundPlayerState->OnLevelDataChanged.Remove(LevelDataChangedHandle);
        }

        if (LifeStateChangedHandle.IsValid())
        {
            BoundPlayerState->OnLifeStateChanged.Remove(LifeStateChangedHandle);
        }

        BoundPlayerState->OnPawnSet.RemoveDynamic(this, &ThisClass::HandlePawnSet);
    }

    LevelDataChangedHandle.Reset();
    LifeStateChangedHandle.Reset();
    BoundPlayerState = nullptr;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// ASC의 HP, 최대 HP, 쉴드 변경 델리게이트 연결을 해제하는 함수
void UMyTeamMemberStatusWidget::UnbindFromAbilitySystemComponent()
{
    if (BoundAbilitySystemComponent)
    {
        if (HealthChangedHandle.IsValid())
        {
            BoundAbilitySystemComponent
                ->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetHealthAttribute())
                .Remove(HealthChangedHandle);
        }

        if (MaxHealthChangedHandle.IsValid())
        {
            BoundAbilitySystemComponent
                ->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetMaxHealthAttribute())
                .Remove(MaxHealthChangedHandle);
        }

        if (ShieldChangedHandle.IsValid())
        {
            BoundAbilitySystemComponent
                ->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetShieldAttribute())
                .Remove(ShieldChangedHandle);
        }
    }

    HealthChangedHandle.Reset();
    MaxHealthChangedHandle.Reset();
    ShieldChangedHandle.Reset();
    BoundAbilitySystemComponent = nullptr;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 연결된 파티원의 텍스트, 전투 수치, 경험치, 생명 상태를 모두 갱신하는 함수
// bAnimate : HP와 경험치 게이지 애니메이션 재생 여부
void UMyTeamMemberStatusWidget::RefreshAll(bool bAnimate)
{
    CacheIdentityFromPlayerState();
    RefreshPlayerInfo();
    RefreshAttributes(bAnimate);
    RefreshLevelAndExp(bAnimate);
    RefreshLifePresentation();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 레벨을 Username보다 먼저 배치한 파티원 정보 텍스트를 갱신하는 함수
void UMyTeamMemberStatusWidget::RefreshPlayerInfo()
{
    if (!Text_PlayerInfo)
    {
        return;
    }

    if (CachedUsername.IsEmpty())
    {
        Text_PlayerInfo->SetText(FText::GetEmpty());
        return;
    }

    Text_PlayerInfo->SetText(FText::Format(
        NSLOCTEXT("ProjectP", "TeamMemberInfoFormat", "LV.{0} {1}"),
        FText::AsNumber(FMath::Max(CachedLevel, 1)),
        FText::FromString(CachedUsername)));
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// ASC에서 현재 HP, 최대 HP, 쉴드를 읽어 파티원 체력 바를 갱신하는 함수
// bAnimate : HP와 쉴드 게이지 애니메이션 재생 여부
void UMyTeamMemberStatusWidget::RefreshAttributes(bool bAnimate)
{
    if (!BoundAbilitySystemComponent || !HPBar)
    {
        return;
    }

    const float CurrentHP = BoundAbilitySystemComponent->GetNumericAttribute(
        UMyAttributeSet::GetHealthAttribute());
    const float MaxHP = BoundAbilitySystemComponent->GetNumericAttribute(
        UMyAttributeSet::GetMaxHealthAttribute());
    const float Shield = BoundAbilitySystemComponent->GetNumericAttribute(
        UMyAttributeSet::GetShieldAttribute());

    HPBar->SetHP(CurrentHP, MaxHP, bAnimate);
    HPBar->SetShield(Shield, bAnimate);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// PlayerState의 레벨과 경험치 및 Pawn의 다음 레벨 요구 경험치를 UI에 반영하는 함수
// bAnimate : 경험치 게이지 애니메이션 재생 여부
void UMyTeamMemberStatusWidget::RefreshLevelAndExp(bool bAnimate)
{
    if (!BoundPlayerState)
    {
        return;
    }

    CachedLevel = FMath::Max(BoundPlayerState->GetCharacterLevel(), 1);
    RefreshPlayerInfo();

    if (!ExpBar)
    {
        return;
    }

    const APlayerCharacterBase* PlayerCharacter = BoundPlayerState->GetPawn<APlayerCharacterBase>();
    if (!PlayerCharacter)
    {
        ExpBar->SetExpInstant(0.0f, 1.0f);
        return;
    }

    const int32 CurrentExp = BoundPlayerState->GetCharacterExp();
    const int32 RequiredExp = PlayerCharacter->GetExpRequiredForNextLevel();
    if (RequiredExp > 0)
    {
        ExpBar->SetExp(
            static_cast<float>(CurrentExp),
            static_cast<float>(RequiredExp),
            bAnimate);
    }
    else
    {
        ExpBar->SetExpInstant(1.0f, 1.0f);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 생존, 사망, 접속 종료 상태에 맞춰 프로필과 게이지 표시를 전환하는 함수
void UMyTeamMemberStatusWidget::RefreshLifePresentation()
{
    const bool bUsePreviewPresentation =
        IsDesignTime() || bShowingRuntimePreview;
    const bool bDown = bUsePreviewPresentation
        ? bPreviewDown
        : bDisconnected ||
            !BoundPlayerState ||
            BoundPlayerState->GetLifeState() == EPlayerLifeState::Dead;

    if (IMG_Profile)
    {
        if (UTexture2D* ProfileTexture = ResolveProfileTexture(bDown))
        {
            IMG_Profile->SetBrushResourceObject(ProfileTexture);
        }
    }

    if (HPBar)
    {
        HPBar->SetValueTextVisible(false);
        HPBar->SetGaugeVisible(!bDown);
    }

    if (ExpBar)
    {
        ExpBar->SetValueTextVisible(false);
        ExpBar->SetGaugeVisible(!bDown);
    }

    if (!bDown)
    {
        RefreshAttributes(false);
        RefreshLevelAndExp(false);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 선택 캐릭터 ID와 생존 상태에 맞는 프로필 텍스처를 반환하는 함수
// bDown : 사망 또는 접속 종료 상태 여부
// Return Value : 사용할 프로필 텍스처, 매핑이 없으면 nullptr
UTexture2D* UMyTeamMemberStatusWidget::ResolveProfileTexture(bool bDown) const
{
    const FTeamMemberProfileTextureSet* TextureSet =
        ProfileTexturesByCharacterId.Find(CachedCharacterId);
    if (!TextureSet)
    {
        return nullptr;
    }

    return bDown ? TextureSet->DownTexture : TextureSet->AliveTexture;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// HP 변경 알림을 받아 파티원 체력 바를 갱신하는 함수
// Data : HP 변경 전후 값
void UMyTeamMemberStatusWidget::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
    RefreshAttributes(true);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 최대 HP 변경 알림을 받아 파티원 체력 바를 갱신하는 함수
// Data : 최대 HP 변경 전후 값
void UMyTeamMemberStatusWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
    RefreshAttributes(true);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 쉴드 변경 알림을 받아 파티원 체력 바를 갱신하는 함수
// Data : 쉴드 변경 전후 값
void UMyTeamMemberStatusWidget::HandleShieldChanged(const FOnAttributeChangeData& Data)
{
    RefreshAttributes(true);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 레벨 또는 경험치 변경 알림을 받아 파티원 정보와 경험치 바를 갱신하는 함수
void UMyTeamMemberStatusWidget::HandleLevelDataChanged()
{
    RefreshLevelAndExp(true);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 생명 상태 변경 알림을 받아 정상 또는 Down 표시로 전환하는 함수
// OldLifeState : 변경 전 생명 상태
// NewLifeState : 변경 후 생명 상태
void UMyTeamMemberStatusWidget::HandleLifeStateChanged(
    EPlayerLifeState OldLifeState,
    EPlayerLifeState NewLifeState)
{
    RefreshLifePresentation();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// PlayerState의 Pawn이 교체될 때 경험치 요구량과 전투 수치를 다시 읽는 함수
// PlayerState : Pawn이 변경된 PlayerState
// NewPawn : 새로 연결된 Pawn
// OldPawn : 이전에 연결된 Pawn
void UMyTeamMemberStatusWidget::HandlePawnSet(
    APlayerState* PlayerState,
    APawn* NewPawn,
    APawn* OldPawn)
{
    if (PlayerState != BoundPlayerState)
    {
        return;
    }

    BindToAbilitySystemComponent();
    RefreshAll(false);
}
