#include "DungeonGS.h"

#include "Net/UnrealNetwork.h"
#include "Dungeon/Revive/DungeonReviveDataAsset.h"
#include "Streaming/MyStreamingManagerComponent.h"

//////////////////////////////////////////////////////////////////////
//! \author 장효제
//! \brief DungeonGS의 생성자 - 현재는 스트리밍 매니저를 생성하고 소유함
ADungeonGS::ADungeonGS()
{
    StreamingManagerComponent = CreateDefaultSubobject<UMyStreamingManagerComponent>(TEXT("StreamingManagerComponent"));
}

////////////////////////////
//! \author 장효제
//! \brief 던전 GS가 소유한 Streaming Manager Component를 반환한다.
//! \return StreamingManagerComponent 포인터. 미생성 시 nullptr.
UMyStreamingManagerComponent* ADungeonGS::GetStreamingManager() const
{
    return StreamingManagerComponent;
}

////////////////////////////
//! \author HanUl
//! \brief 서버 부활 검증과 클라이언트 표시가 공유할 정적 부활 옵션 데이터를 반환한다.
//! \param 없음
//! \return BP_DungeonGS에 지정된 부활 데이터, 미지정이면 nullptr
UDungeonReviveDataAsset* ADungeonGS::GetReviveData() const
{
    return ReviveData;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 복제할 프로퍼티를 등록하는 함수
// OutLifetimeProps : 언리얼 네트워크 복제 시스템에 등록될 프로퍼티 목록
void ADungeonGS::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ADungeonGS, SurrenderVoteState);
    DOREPLIFETIME(ADungeonGS, MissionViews);
}

////////////////////////////
//! \author 장효제
//! \brief 클라이언트 UI에 공개된 Mission View 배열을 반환한다.
//! \return 종료 시각 순으로 정렬된 공개 Mission View 복사본이다.
TArray<FMyMissionPublicView> ADungeonGS::GetMissionViews() const
{
    return MissionViews;
}

////////////////////////////
//! \author 장효제
//! \brief 서버 Mission 원본이 만든 공개 View를 복제 상태에 반영한다.
//! \param NewMissionViews 새 공개 Mission View 배열이다.
void ADungeonGS::SetMissionViews(const TArray<FMyMissionPublicView>& NewMissionViews)
{
    if (!HasAuthority())
    {
        return;
    }

    MissionViews = NewMissionViews;
    OnMissionViewsChanged.Broadcast();
    ForceNetUpdate();
}

////////////////////////////
//! \author 장효제
//! \brief Mission View 복제 수신을 로컬 HUD 구독자에게 알린다.
void ADungeonGS::OnRep_MissionViews()
{
    OnMissionViewsChanged.Broadcast();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 항복 투표 상태를 반환하는 함수
// Return Value : 현재 항복 투표 상태 정보
const FDungeonSurrenderVoteState& ADungeonGS::GetSurrenderVoteState() const
{
    return SurrenderVoteState;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표 상태를 갱신하는 함수
// NewState : 새로 적용할 항복 투표 상태 정보
void ADungeonGS::SetSurrenderVoteState(const FDungeonSurrenderVoteState& NewState)
{
    SurrenderVoteState = NewState;

    OnSurrenderVoteStateChanged();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표 상태가 클라이언트에 복제되었을 때 호출되는 함수
void ADungeonGS::OnRep_SurrenderVoteState()
{
    OnSurrenderVoteStateChanged();
}
