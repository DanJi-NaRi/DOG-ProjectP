#include "CPP_PartyInviteUserRow.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UCPP_PartyInviteUserRow::NativeConstruct()
{
    Super::NativeConstruct();

    ValidateWidgetBindings();

    if (BTN_PartyInvite)
    {
        BTN_PartyInvite->OnClicked.RemoveDynamic(this, &UCPP_PartyInviteUserRow::HandlePartyInviteButtonClicked);
        BTN_PartyInvite->OnClicked.AddDynamic(this, &UCPP_PartyInviteUserRow::HandlePartyInviteButtonClicked);
    }
}

void UCPP_PartyInviteUserRow::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (CurrentRowMode != ELobbyPartyRowMode::PartyJoinTarget || PartyJoinCooldownRemainingSeconds <= 0.0f)
    {
        return;
    }

    PartyJoinCooldownRemainingSeconds = FMath::Max(0.0f, PartyJoinCooldownRemainingSeconds - InDeltaTime);
    UpdatePartyJoinButtonState();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 초대 대상 유저 정보를 Row 위젯에 적용하는 함수
// NewTargetPlayerState : 초대 대상 플레이어의 PlayerState
// NewDisplayName : Row에 표시할 유저 이름
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 멤버 Row에 표시할 멤버 정보를 적용하는 함수
// NewMemberRowData : Row에 표시할 파티 멤버 정보
void UCPP_PartyInviteUserRow::SetupPartyMember(const FLobbyPartyMemberRowData& NewMemberRowData)
{
    CurrentRowMode = ELobbyPartyRowMode::PartyMember;
    CachedMemberRowData = NewMemberRowData;
    TargetPlayerState = NewMemberRowData.PlayerState.Get();
    TargetPartyId = -1;
    bCanRequestJoin = false;
    PartyJoinCooldownRemainingSeconds = 0.0f;

    if (TXT_Username)
    {
        const bool bHasStateTextBlock = TXT_PartyState != nullptr;
        const FString DisplayName = NewMemberRowData.bIsPartyLeader
            ? FString::Printf(TEXT("(Master) %s"), *NewMemberRowData.DisplayName)
            : NewMemberRowData.DisplayName;
        const FString DisplayText = bHasStateTextBlock || NewMemberRowData.ConnectionStateText.IsEmpty()
            ? DisplayName
            : FString::Printf(TEXT("%s - %s"), *DisplayName, *NewMemberRowData.ConnectionStateText);
        TXT_Username->SetText(FText::FromString(DisplayText));
    }

    if (TXT_PartyState)
    {
        TXT_PartyState->SetText(FText::FromString(NewMemberRowData.ConnectionStateText));
    }

    if (BTN_PartyInvite)
    {
        BTN_PartyInvite->SetVisibility(ESlateVisibility::Collapsed);
        BTN_PartyInvite->SetIsEnabled(false);
    }

    BP_OnPartyMemberSetup(NewMemberRowData);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 초대 대상 유저 정보를 Row 위젯에 적용하는 함수
// NewTargetPlayerState : 초대 대상 플레이어의 PlayerState
// NewDisplayName : Row에 표시할 유저 이름
void UCPP_PartyInviteUserRow::SetupInviteTarget(APlayerState* NewTargetPlayerState, const FString& NewDisplayName)
{
    CurrentRowMode = ELobbyPartyRowMode::InviteTarget;
    CachedMemberRowData = FLobbyPartyMemberRowData();
    TargetPlayerState = NewTargetPlayerState;
    TargetPartyId = -1;
    bCanRequestJoin = false;
    PartyJoinCooldownRemainingSeconds = 0.0f;

    if (TXT_Username)
    {
        TXT_Username->SetText(FText::FromString(NewDisplayName));
    }

    if (TXT_PartyInvite)
    {
        TXT_PartyInvite->SetText(FText::FromString(TEXT("초대")));
    }

    if (BTN_PartyInvite)
    {
        BTN_PartyInvite->SetVisibility(ESlateVisibility::Visible);
        BTN_PartyInvite->SetIsEnabled(TargetPlayerState != nullptr);
    }

    if (TXT_PartyState)
    {
        TXT_PartyState->SetText(FText::GetEmpty());
    }

    BP_OnInviteTargetSetup(NewTargetPlayerState, NewDisplayName);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 가입 탭에서 표시할 파티 정보를 Row 위젯에 적용하는 함수
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 가입 탭에서 표시할 파티 정보를 Row 위젯에 적용하는 함수
// NewTargetPartyId : 가입 신청을 보낼 파티 ID
// NewDisplayName : Row에 표시할 파티 정보 텍스트
// bNewCanRequestJoin : 가입 신청 버튼 활성화 가능 여부
// RemainingCooldownSeconds : 동일 파티 가입 신청 쿨타임 남은 시간
void UCPP_PartyInviteUserRow::SetupPartyJoinTarget(int32 NewTargetPartyId, const FString& NewDisplayName, bool bNewCanRequestJoin, float RemainingCooldownSeconds)
{
    CurrentRowMode = ELobbyPartyRowMode::PartyJoinTarget;
    CachedMemberRowData = FLobbyPartyMemberRowData();
    TargetPlayerState = nullptr;
    TargetPartyId = NewTargetPartyId;
    bCanRequestJoin = bNewCanRequestJoin;
    PartyJoinCooldownRemainingSeconds = FMath::Max(0.0f, RemainingCooldownSeconds);

    if (TXT_Username)
    {
        TXT_Username->SetText(FText::FromString(NewDisplayName));
    }

    if (TXT_PartyState)
    {
        TXT_PartyState->SetText(FText::GetEmpty());
    }

    FLobbyJoinablePartyEntry PartyJoinTarget;
    PartyJoinTarget.PartyId = NewTargetPartyId;
    PartyJoinTarget.DisplayName = NewDisplayName;
    PartyJoinTarget.bCanRequestJoin = bNewCanRequestJoin;
    PartyJoinTarget.RemainingCooldownSeconds = RemainingCooldownSeconds;

    UpdatePartyJoinButtonState();
    BP_OnPartyJoinTargetSetup(PartyJoinTarget);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Row의 버튼이 클릭되었을 때 현재 모드에 맞는 대상 정보를 외부로 전달하는 함수
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 가입 대상 정보를 Row 위젯에 적용하는 함수
// NewPartyJoinTarget : Row에 표시할 파티 가입 대상 정보
void UCPP_PartyInviteUserRow::SetupPartyJoinTargetEntry(const FLobbyJoinablePartyEntry& NewPartyJoinTarget)
{
    CurrentRowMode = ELobbyPartyRowMode::PartyJoinTarget;
    CachedMemberRowData = FLobbyPartyMemberRowData();
    TargetPlayerState = nullptr;
    TargetPartyId = NewPartyJoinTarget.PartyId;
    bCanRequestJoin = NewPartyJoinTarget.bCanRequestJoin;
    PartyJoinCooldownRemainingSeconds = FMath::Max(0.0f, NewPartyJoinTarget.RemainingCooldownSeconds);

    if (TXT_Username)
    {
        TXT_Username->SetText(FText::FromString(NewPartyJoinTarget.DisplayName));
    }

    if (TXT_PartyState)
    {
        TXT_PartyState->SetText(FText::GetEmpty());
    }

    UpdatePartyJoinButtonState();
    BP_OnPartyJoinTargetSetup(NewPartyJoinTarget);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Row의 버튼이 클릭되었을 때 현재 모드에 맞는 대상 정보를 외부로 전달하는 함수
void UCPP_PartyInviteUserRow::HandlePartyInviteButtonClicked()
{
    if (CurrentRowMode == ELobbyPartyRowMode::PartyMember)
    {
        return;
    }

    if (CurrentRowMode == ELobbyPartyRowMode::PartyJoinTarget)
    {
        if (!bCanRequestJoin || TargetPartyId == -1 || PartyJoinCooldownRemainingSeconds > 0.0f)
        {
            return;
        }

        StartPartyJoinCooldown(5.0f);
        OnJoinPartyButtonClicked.Broadcast(TargetPartyId);
        return;
    }

    OnInviteButtonClicked.Broadcast(TargetPlayerState.Get());
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 Row Widget Blueprint의 기능 연결 이름이 맞는지 확인하는 함수
void UCPP_PartyInviteUserRow::ValidateWidgetBindings() const
{
    if (!TXT_Username)
    {
        UE_LOG(LogTemp, Warning, TEXT("PartyInviteUserRow binding missing. Display name text widget name should be TXT_Username."));
    }

    if (!BTN_PartyInvite)
    {
        UE_LOG(LogTemp, Warning, TEXT("PartyInviteUserRow binding missing. Action button name should be BTN_PartyInvite."));
    }

    if (!TXT_PartyInvite)
    {
        UE_LOG(LogTemp, Warning, TEXT("PartyInviteUserRow binding missing. Action button text widget name should be TXT_PartyInvite."));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 가입 신청 버튼의 클라이언트 쿨타임을 시작하는 함수
// CooldownSeconds : 가입 신청 버튼을 비활성화할 시간
void UCPP_PartyInviteUserRow::StartPartyJoinCooldown(float CooldownSeconds)
{
    PartyJoinCooldownRemainingSeconds = FMath::Max(0.0f, CooldownSeconds);
    UpdatePartyJoinButtonState();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 가입 신청 버튼의 활성화 상태와 표시 텍스트를 현재 쿨타임에 맞게 갱신하는 함수
void UCPP_PartyInviteUserRow::UpdatePartyJoinButtonState()
{
    if (!BTN_PartyInvite)
    {
        BP_OnPartyJoinButtonStateChanged(false, FMath::CeilToInt(PartyJoinCooldownRemainingSeconds));
        return;
    }

    const bool bIsCooldownActive = PartyJoinCooldownRemainingSeconds > 0.0f;
    BTN_PartyInvite->SetIsEnabled(bCanRequestJoin && !bIsCooldownActive && TargetPartyId != -1);
    BTN_PartyInvite->SetVisibility(ESlateVisibility::Visible);

    if (!TXT_PartyInvite)
    {
        BP_OnPartyJoinButtonStateChanged(BTN_PartyInvite->GetIsEnabled(), FMath::CeilToInt(PartyJoinCooldownRemainingSeconds));
        return;
    }

    if (bIsCooldownActive)
    {
        const int32 RemainingSeconds = FMath::Max(1, FMath::CeilToInt(PartyJoinCooldownRemainingSeconds));
        TXT_PartyInvite->SetText(FText::AsNumber(RemainingSeconds));
        BP_OnPartyJoinButtonStateChanged(false, RemainingSeconds);
        return;
    }

    TXT_PartyInvite->SetText(FText::FromString(TEXT("가입 신청")));
    BP_OnPartyJoinButtonStateChanged(BTN_PartyInvite->GetIsEnabled(), 0);
}
