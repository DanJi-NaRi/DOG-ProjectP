#include "CPP_CheatCommandSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "../../../MyPlayerController.h"
#include "../../../Dungeon/CPP_DungeonGM.h"
#include "../../../Dungeon/DungeonGS.h"
#include "../../../Dungeon/Revive/DungeonReviveDataAsset.h"
#include "../../../Zone/ZoneBase.h"
#include "../../../Zone/ManagingSystems/ZoneManager.h"
#include "../../../Zone/Components/CPP_SpawnerComponent.h"
#include "../../../GAS/MyAttributeSet.h"
#include "../../../GAS/MyPlayerState.h"
#include "../../../Item/MyInventoryComponent.h"
#include "../../../MyGameplayTags.h"
#include "../NetSub/ServerConfigSubsystem.h"
#include "../../../Lobby/CPP_LobbyGMB.h"
#include "../../../Lobby/CPP_LobbyGSB.h"
#include "../../../Player/PlayerCharacterBase.h"
#include "../../../Widget/Debug/MyStatDebugWidget.h"
#include "Blueprint/UserWidget.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 게임/PIE 월드에서만 서브시스템을 생성하는 함수
// Outer : 서브시스템이 속할 월드
// Return Value : 생성 대상 월드이면 true
bool UCPP_CheatCommandSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (!Super::ShouldCreateSubsystem(Outer))
    {
        return false;
    }

    const UWorld* World = Cast<UWorld>(Outer);
    return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서브시스템 초기화 시 기본 제공 치트 명령을 등록하는 함수
// 서버/클라 양쪽 월드에서 동일하게 등록되므로 클라 자동완성 목록도 여기서 나온다.
void UCPP_CheatCommandSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    RegisterBuiltInCommands();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 치트 명령을 등록하는 함수. 같은 이름이 이미 있으면 덮어쓴다.
// CommandInfo : 명령 이름/사용법/설명/범위 정보
// CommandHandler : 실행 시 호출할 핸들러
void UCPP_CheatCommandSubsystem::RegisterCommand(const FCheatCommandInfo& CommandInfo, FCheatCommandHandler CommandHandler)
{
    if (CommandInfo.Name.IsEmpty())
    {
        return;
    }

    for (FRegisteredCheatCommand& Command : Commands)
    {
        if (Command.Info.Name.Equals(CommandInfo.Name, ESearchCase::IgnoreCase))
        {
            Command.Info = CommandInfo;
            Command.Handler = MoveTemp(CommandHandler);
            return;
        }
    }

    FRegisteredCheatCommand& NewCommand = Commands.AddDefaulted_GetRef();
    NewCommand.Info = CommandInfo;
    NewCommand.Handler = MoveTemp(CommandHandler);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 명령의 인자 자동완성 제공자를 등록하는 함수. 같은 명령에 다시 등록하면 덮어쓴다.
// CommandName : 대상 명령 이름 ('/' 제외)
// Provider : 인자 후보를 채워줄 델리게이트
void UCPP_CheatCommandSubsystem::RegisterArgumentSuggestionProvider(const FString& CommandName, FCheatArgumentSuggestionProvider Provider)
{
    if (CommandName.IsEmpty() || !Provider.IsBound())
    {
        return;
    }

    ArgumentSuggestionProviders.Add(CommandName.ToLower(), MoveTemp(Provider));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 명령의 인자 후보 목록을 반환하는 함수. (클라 자동완성 UI에서 호출)
// CommandName : 명령 이름 ('/' 제외)
// ArgIndex : 입력 중인 인자 인덱스 (0부터)
// Prefix : 입력 중인 인자 접두어 (비어 있으면 전체 후보)
// OutSuggestions : 후보 문자열 목록(출력)
void UCPP_CheatCommandSubsystem::GetArgumentSuggestions(const FString& CommandName, int32 ArgIndex, const FString& Prefix, TArray<FString>& OutSuggestions) const
{
    OutSuggestions.Reset();

    // 현재 월드에서 사용 가능한 명령인지 먼저 확인한다
    FCheatCommandInfo CommandInfo;
    if (!FindCommandInfo(CommandName, CommandInfo))
    {
        return;
    }

    const FCheatArgumentSuggestionProvider* Provider = ArgumentSuggestionProviders.Find(CommandName.ToLower());
    if (Provider && Provider->IsBound())
    {
        Provider->Execute(ArgIndex, Prefix, OutSuggestions);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 월드에서 사용 가능한 명령 목록을 반환하는 함수
// OutCommands : 사용 가능한 명령 정보 배열(출력)
void UCPP_CheatCommandSubsystem::GetAvailableCommands(TArray<FCheatCommandInfo>& OutCommands) const
{
    OutCommands.Reset();

    for (const FRegisteredCheatCommand& Command : Commands)
    {
        if (IsCommandAvailableInCurrentWorld(Command.Info))
        {
            OutCommands.Add(Command.Info);
        }
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 입력 접두어로 시작하는 사용 가능 명령 목록을 반환하는 함수
// NamePrefix : 명령 이름 접두어 ('/' 제외). 비어 있으면 전체 반환
// OutCommands : 매칭된 명령 정보 배열(출력)
void UCPP_CheatCommandSubsystem::GetMatchingCommands(const FString& NamePrefix, TArray<FCheatCommandInfo>& OutCommands) const
{
    OutCommands.Reset();

    for (const FRegisteredCheatCommand& Command : Commands)
    {
        if (!IsCommandAvailableInCurrentWorld(Command.Info))
        {
            continue;
        }

        if (NamePrefix.IsEmpty() || Command.Info.Name.StartsWith(NamePrefix, ESearchCase::IgnoreCase))
        {
            OutCommands.Add(Command.Info);
        }
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 명령 이름으로 등록 정보를 찾는 함수
// CommandName : 찾을 명령 이름 ('/' 제외, 대소문자 무시)
// OutCommandInfo : 찾은 명령 정보(출력)
// Return Value : 찾았으면 true
bool UCPP_CheatCommandSubsystem::FindCommandInfo(const FString& CommandName, FCheatCommandInfo& OutCommandInfo) const
{
    for (const FRegisteredCheatCommand& Command : Commands)
    {
        if (Command.Info.Name.Equals(CommandName, ESearchCase::IgnoreCase))
        {
            OutCommandInfo = Command.Info;
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 이 월드에서 채팅 치트 실행이 허용되는지 확인하는 함수
// Standalone(로컬 테스트)은 항상 허용하고, 그 외에는 서버 설정 bAllowChatCheats를 따른다.
// Return Value : 치트 실행이 허용되면 true
bool UCPP_CheatCommandSubsystem::AreChatCheatsAllowed() const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    if (World->GetNetMode() == NM_Standalone)
    {
        return true;
    }

    const UServerConfigSubsystem* ServerConfigSubsystem = World->GetGameInstance()
        ? World->GetGameInstance()->GetSubsystem<UServerConfigSubsystem>()
        : nullptr;
    return ServerConfigSubsystem && ServerConfigSubsystem->IsChatCheatAllowed();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 명령 문자열을 파싱해 실행하고 채팅창에 표시할 결과 문자열을 반환하는 함수
// bRunOnClient 명령은 로컬 클라이언트에서, 나머지는 서버에서 호출된다.
// Requester : 명령을 요청한 플레이어 컨트롤러
// CommandLine : "/heal 50" 또는 "heal 50" 형태의 명령 문자열
// Return Value : 실행 결과 문자열
FString UCPP_CheatCommandSubsystem::ExecuteCommand(AMyPlayerController* Requester, const FString& CommandLine)
{
    FString Trimmed = CommandLine.TrimStartAndEnd();
    if (Trimmed.StartsWith(TEXT("/")))
    {
        Trimmed.RightChopInline(1);
    }

    TArray<FString> Tokens;
    Trimmed.ParseIntoArrayWS(Tokens);
    if (Tokens.IsEmpty())
    {
        return TEXT("명령을 입력하세요. (/help 로 목록 확인)");
    }

    const FString CommandName = Tokens[0];
    TArray<FString> Args = Tokens;
    Args.RemoveAt(0);

    for (FRegisteredCheatCommand& Command : Commands)
    {
        if (!Command.Info.Name.Equals(CommandName, ESearchCase::IgnoreCase))
        {
            continue;
        }

        if (!IsCommandAvailableInCurrentWorld(Command.Info))
        {
            return FString::Printf(TEXT("'%s' 명령은 이 맵에서 사용할 수 없습니다."), *Command.Info.Name);
        }

        if (!Command.Handler.IsBound())
        {
            return FString::Printf(TEXT("'%s' 명령의 핸들러가 없습니다."), *Command.Info.Name);
        }

        return Command.Handler.Execute(Requester, Args);
    }

    return FString::Printf(TEXT("알 수 없는 명령: %s (/help 로 목록 확인)"), *CommandName);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 월드가 로비인지 던전인지 판별하는 함수
// 로비 GameState(ACPP_LobbyGSB)가 있으면 로비, 아니면 던전으로 취급한다.
// Return Value : 현재 월드의 치트 범위
ECheatCommandScope UCPP_CheatCommandSubsystem::GetCurrentWorldScope() const
{
    const UWorld* World = GetWorld();
    if (World && World->GetGameState<ACPP_LobbyGSB>())
    {
        return ECheatCommandScope::Lobby;
    }

    return ECheatCommandScope::Dungeon;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 명령이 현재 월드 범위에서 사용 가능한지 확인하는 함수
// CommandInfo : 확인할 명령 정보
// Return Value : 사용 가능하면 true
bool UCPP_CheatCommandSubsystem::IsCommandAvailableInCurrentWorld(const FCheatCommandInfo& CommandInfo) const
{
    return CommandInfo.Scope == ECheatCommandScope::All || CommandInfo.Scope == GetCurrentWorldScope();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 기본 제공 치트 명령들을 등록하는 함수
// 새 치트를 추가하려면 여기(또는 각 시스템의 서버 코드)에서 RegisterCommand를 호출하면 된다.
void UCPP_CheatCommandSubsystem::RegisterBuiltInCommands()
{
    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("help");
        Info.Usage = TEXT("/help");
        Info.Description = TEXT("사용 가능한 치트 목록을 표시한다");
        Info.Scope = ECheatCommandScope::All;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleHelpCommand));
    }

    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("char");
        Info.Usage = TEXT("/char <100|200|300>");
        Info.Description = TEXT("로비 캐릭터를 변경한다");
        Info.Scope = ECheatCommandScope::Lobby;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleSelectCharacterCommand));
    }

    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("heal");
        Info.Usage = TEXT("/heal [증감량]");
        Info.Description = TEXT("체력을 변경한다 (양수=회복, 음수=감소, 생략=전체 회복)");
        Info.Scope = ECheatCommandScope::Dungeon;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleHealCommand));
    }

    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("kill");
        Info.Usage = TEXT("/kill");
        Info.Description = TEXT("체력을 0으로 만들어 즉시 사망한다 (테스트용)");
        Info.Scope = ECheatCommandScope::Dungeon;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleKillCommand));
    }

    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("revive");
        Info.Usage = TEXT("/revive <OptionId>");
        Info.Description = TEXT("사망한 본인에게 데이터 기반 비용과 지연시간을 적용해 개인 부활을 예약한다");
        Info.Scope = ECheatCommandScope::Dungeon;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleReviveCommand));
        RegisterArgumentSuggestionProvider(Info.Name, FCheatArgumentSuggestionProvider::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleReviveArgumentSuggestions));
    }

    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("fps");
        Info.Usage = TEXT("/fps <60|120>");
        Info.Description = TEXT("클라이언트 최대 프레임을 설정한다 (0=무제한)");
        Info.Scope = ECheatCommandScope::All;
        Info.bRunOnClient = true;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleFpsCommand));
    }

    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("stats");
        Info.Usage = TEXT("/stats");
        Info.Description = TEXT("화면 좌측에 현재 스탯 디버그 창을 토글한다");
        Info.Scope = ECheatCommandScope::Dungeon;
        Info.bRunOnClient = true;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleStatsCommand));
    }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("debugline");
        Info.Usage = TEXT("/debugline <on|off>");
        Info.Description = TEXT("본인 화면의 스킬 판정 DebugLine 표시를 설정한다");
        Info.Scope = ECheatCommandScope::Dungeon;
        Info.bRunOnClient = true;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleDebugLineCommand));
        RegisterArgumentSuggestionProvider(Info.Name, FCheatArgumentSuggestionProvider::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleDebugLineArgumentSuggestions));
    }
#endif

    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("additem");
        Info.Usage = TEXT("/additem <아이템ID|All> [개수]");
        Info.Description = TEXT("아이템을 지급한다 (ID는 데이터테이블 Row Name, 개수 생략 시 1 / All은 전체 아이템을 최대치로 지급)");
        Info.Scope = ECheatCommandScope::Dungeon;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleAddItemCommand));
        RegisterArgumentSuggestionProvider(Info.Name, FCheatArgumentSuggestionProvider::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleAddItemArgumentSuggestions));
    }

    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("addmeso");
        Info.Usage = TEXT("/addmeso <양>");
        Info.Description = TEXT("메소를 지급한다");
        Info.Scope = ECheatCommandScope::Dungeon;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleAddMesoCommand));
    }

    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("level");
        Info.Usage = TEXT("/level <레벨>");
        Info.Description = TEXT("캐릭터 레벨을 설정하고 레벨 스탯 테이블 값을 적용한다");
        Info.Scope = ECheatCommandScope::Dungeon;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleSetLevelCommand));
    }

    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("addexp");
        Info.Usage = TEXT("/addexp <양>");
        Info.Description = TEXT("경험치를 추가한다 (요구량을 채우면 레벨업)");
        Info.Scope = ECheatCommandScope::Dungeon;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleAddExpCommand));
    }

    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("speed");
        Info.Usage = TEXT("/speed [값|+증감|-증감]");
        Info.Description = TEXT("이동 속도를 설정한다 (+/-는 현재 값에서 증감, 생략 시 현재 값 표시)");
        Info.Scope = ECheatCommandScope::Dungeon;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleSetSpeedCommand));
    }

    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("spawn");
        Info.Usage = TEXT("/spawn <존번호|here> <마릿수>");
        Info.Description = TEXT("지정 존의 스폰 지점에 몬스터를 스폰한다 (here=내가 서 있는 존, 존번호는 0부터, 몬스터는 존 WaveData 구성 순환)");
        Info.Scope = ECheatCommandScope::Dungeon;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleSpawnMonsterCommand));
        RegisterArgumentSuggestionProvider(Info.Name, FCheatArgumentSuggestionProvider::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleSpawnMonsterArgumentSuggestions));
    }

#if !UE_BUILD_SHIPPING
    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("clearzone");
        Info.Usage = TEXT("/clearzone <존번호>");
        Info.Description = TEXT("다음 진행 존을 강제로 Clear한다 (비Shipping 개발 검증 전용, 존번호는 0부터)");
        Info.Scope = ECheatCommandScope::Dungeon;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleClearZoneCommand));
        RegisterArgumentSuggestionProvider(Info.Name, FCheatArgumentSuggestionProvider::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleClearZoneArgumentSuggestions));
    }

    {
        FCheatCommandInfo Info;
        Info.Name = TEXT("makebubbles");
        Info.Usage = TEXT("/makeBubbles");
        Info.Description = TEXT("로컬 신 채팅과 일반 메신저에 빈 테스트 블록을 20개씩 추가한다");
        Info.Scope = ECheatCommandScope::Dungeon;
        Info.bRunOnClient = true;
        RegisterCommand(Info, FCheatCommandHandler::CreateUObject(this, &UCPP_CheatCommandSubsystem::HandleMakeBubblesCommand));
    }
#endif
}

#if !UE_BUILD_SHIPPING
////////////////////////////
//! \author 장효제
//! \brief 요청 클라이언트의 신 채팅과 일반 메신저에 빈 테스트 블록을 추가한다.
//! \param Requester 명령을 실행한 로컬 PlayerController
//! \param Args 사용하지 않는 추가 인자
//! \return 메신저에 별도 결과 문구를 추가하지 않도록 빈 문자열
FString UCPP_CheatCommandSubsystem::HandleMakeBubblesCommand(
    AMyPlayerController* Requester,
    const TArray<FString>& Args)
{
    if (Requester)
    {
        Requester->MakeTestBubbles(20);
    }
    return FString();
}
#endif

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// /help : 현재 월드에서 사용 가능한 치트 목록을 여러 줄 문자열로 반환하는 함수
// Requester : 명령을 요청한 플레이어 컨트롤러 (사용 안 함)
// Args : 추가 인자 (사용 안 함)
// Return Value : 명령 목록 문자열
FString UCPP_CheatCommandSubsystem::HandleHelpCommand(AMyPlayerController* Requester, const TArray<FString>& Args)
{
    TArray<FCheatCommandInfo> AvailableCommands;
    GetAvailableCommands(AvailableCommands);

    TArray<FString> Lines;
    Lines.Add(TEXT("=== 사용 가능한 치트 ==="));
    for (const FCheatCommandInfo& Info : AvailableCommands)
    {
        Lines.Add(FString::Printf(TEXT("%s : %s"), *Info.Usage, *Info.Description));
    }

    return FString::Join(Lines, TEXT("\n"));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// /char : 로비 캐릭터 선택을 변경하는 함수
// Requester : 명령을 요청한 플레이어 컨트롤러
// Args : [0]=캐릭터 ID (100/200/300)
// Return Value : 실행 결과 문자열
FString UCPP_CheatCommandSubsystem::HandleSelectCharacterCommand(AMyPlayerController* Requester, const TArray<FString>& Args)
{
    if (Args.IsEmpty())
    {
        return TEXT("사용법: /char <100|200|300>");
    }

    const int32 CharacterId = FCString::Atoi(*Args[0]);

    ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr;
    if (!LobbyGMB)
    {
        return TEXT("로비 GameMode를 찾을 수 없습니다.");
    }

    if (!LobbyGMB->SelectLobbyCharacter(Requester, CharacterId))
    {
        return FString::Printf(TEXT("캐릭터 변경 실패: %d (100/200/300만 가능)"), CharacterId);
    }

    return FString::Printf(TEXT("캐릭터를 %d(으)로 변경했습니다."), CharacterId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// /heal : 요청자 캐릭터의 체력을 증감하는 함수
// Requester : 명령을 요청한 플레이어 컨트롤러
// Args : [0]=체력 증감량 (양수는 회복, 음수는 감소, 생략 시 전체 회복)
// Return Value : 실행 결과 문자열
FString UCPP_CheatCommandSubsystem::HandleHealCommand(AMyPlayerController* Requester, const TArray<FString>& Args)
{
    APlayerCharacterBase* PlayerCharacter = Requester ? Cast<APlayerCharacterBase>(Requester->GetPawn()) : nullptr;
    UMyAttributeSet* AttributeSet = PlayerCharacter ? PlayerCharacter->GetMyAttributeSet() : nullptr;
    if (!AttributeSet)
    {
        return TEXT("회복할 캐릭터를 찾을 수 없습니다.");
    }

    const float MaxHealth = AttributeSet->GetMaxHealth();
    const float OldHealth = AttributeSet->GetHealth();

    float NewHealth = MaxHealth;
    if (!Args.IsEmpty())
    {
        const float HealthDelta = FCString::Atof(*Args[0]);
        if (FMath::IsNearlyZero(HealthDelta))
        {
            return TEXT("사용법: /heal [증감량] (양수=회복, 음수=감소, 0은 사용할 수 없음)");
        }
        NewHealth = FMath::Clamp(OldHealth + HealthDelta, 0.0f, MaxHealth);
    }

    AttributeSet->SetHealth(NewHealth);
    return FString::Printf(TEXT("체력 변경: %.0f -> %.0f (최대 %.0f)"), OldHealth, NewHealth, MaxHealth);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// /kill : 요청자 캐릭터의 체력을 0으로 만들어 사망시키는 함수 (서버에서 실행됨)
// SetHealth(0)이 서버 ASC의 Health 변경 델리게이트를 발화시키고, AMyPlayerState::HandleHealthChanged가
// 이를 감지해 LifeState를 Dead로 전환한다. (일반 피격 사망과 동일한 처리 경로)
// Requester : 명령을 요청한 플레이어 컨트롤러
// Args : 사용 안 함
// Return Value : 실행 결과 문자열
FString UCPP_CheatCommandSubsystem::HandleKillCommand(AMyPlayerController* Requester, const TArray<FString>& Args)
{
    APlayerCharacterBase* PlayerCharacter = Requester ? Cast<APlayerCharacterBase>(Requester->GetPawn()) : nullptr;
    UMyAttributeSet* AttributeSet = PlayerCharacter ? PlayerCharacter->GetMyAttributeSet() : nullptr;
    if (!AttributeSet)
    {
        return TEXT("사망시킬 캐릭터를 찾을 수 없습니다.");
    }

    const float OldHealth = AttributeSet->GetHealth();
    if (OldHealth <= 0.0f)
    {
        return TEXT("이미 사망 상태입니다.");
    }

    AttributeSet->SetHealth(0.0f);
    return FString::Printf(TEXT("체력을 0으로 만들었습니다. (%.0f -> 0)"), OldHealth);
}

////////////////////////////
//! \author HanUl
//! \brief /revive로 서버 DungeonGM의 데이터 기반 개인 부활 예약을 실행하거나 옵션 목록을 출력한다.
//! \param Requester 부활할 플레이어의 컨트롤러
//! \param Args [0]=ReviveDataAsset의 OptionId, 생략하면 활성 옵션 목록 출력
//! \return 예약 성공 또는 검증 실패 설명
FString UCPP_CheatCommandSubsystem::HandleReviveCommand(
    AMyPlayerController* Requester,
    const TArray<FString>& Args)
{
    ACPP_DungeonGM* DungeonGM = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_DungeonGM>() : nullptr;
    if (!DungeonGM)
    {
        return TEXT("던전 GameMode를 찾을 수 없습니다.");
    }

    if (Args.IsEmpty())
    {
        const ADungeonGS* DungeonGS = GetWorld() ? GetWorld()->GetGameState<ADungeonGS>() : nullptr;
        const UDungeonReviveDataAsset* ReviveData = DungeonGS ? DungeonGS->GetReviveData() : nullptr;
        if (!ReviveData)
        {
            return TEXT("사용법: /revive <OptionId> (DungeonGS의 ReviveDataAsset 미지정)");
        }

        TArray<FName> OptionIds;
        ReviveData->GetEnabledOptionIds(OptionIds);
        if (OptionIds.IsEmpty())
        {
            return TEXT("활성화된 부활 옵션이 없습니다.");
        }

        TArray<FString> OptionLines;
        OptionLines.Add(TEXT("=== 부활 옵션 ==="));
        for (const FName OptionId : OptionIds)
        {
            const FDungeonReviveOption* Option = ReviveData->FindOption(OptionId, true);
            if (Option)
            {
                OptionLines.Add(FString::Printf(
                    TEXT("%s : 비용 %d, 체력 %.0f%%, 대기 %.1f초"),
                    *Option->OptionId.ToString(),
                    Option->MesoCost,
                    Option->ReviveHealthPercent * 100.0f,
                    Option->ReviveDelaySeconds));
            }
        }
        return FString::Join(OptionLines, TEXT("\n"));
    }

    if (Args.Num() != 1)
    {
        return TEXT("사용법: /revive <OptionId>");
    }

    FString ResultMessage;
    DungeonGM->StartPlayerRevive(Requester, FName(*Args[0]), ResultMessage);
    return ResultMessage;
}

////////////////////////////
//! \author HanUl
//! \brief /level : 요청자 캐릭터의 레벨을 설정하고 레벨 스탯 CurveTable 값을 적용한다.
//! \param Requester 명령을 요청한 플레이어 컨트롤러
//! \param Args [0]=설정할 레벨
//! \return 실행 결과 문자열
FString UCPP_CheatCommandSubsystem::HandleSetLevelCommand(AMyPlayerController* Requester, const TArray<FString>& Args)
{
    APlayerCharacterBase* PlayerCharacter = Requester ? Cast<APlayerCharacterBase>(Requester->GetPawn()) : nullptr;
    if (!PlayerCharacter)
    {
        return TEXT("레벨을 설정할 캐릭터를 찾을 수 없습니다.");
    }

    if (Args.IsEmpty())
    {
        return TEXT("사용법: /level <레벨>");
    }

    const int32 NewLevel = FCString::Atoi(*Args[0]);
    if (NewLevel < 1)
    {
        return TEXT("사용법: /level <레벨> (1 이상)");
    }

    PlayerCharacter->SetCharacterLevel(NewLevel);

    const UMyAttributeSet* AttributeSet = PlayerCharacter->GetMyAttributeSet();
    if (!AttributeSet)
    {
        return FString::Printf(TEXT("레벨 %d 설정 (AttributeSet 없음)"), PlayerCharacter->GetCharacterLevel());
    }

    return FString::Printf(TEXT("레벨 %d 적용: HP %.1f/%.1f, 공격력 %.1f, 방어력 %.1f"),
        PlayerCharacter->GetCharacterLevel(),
        AttributeSet->GetHealth(),
        AttributeSet->GetMaxHealth(),
        AttributeSet->GetAttackPower(),
        AttributeSet->GetDefense());
}

////////////////////////////
//! \author HanUl
//! \brief /addexp : 요청자 캐릭터에 경험치를 추가한다. 요구량을 채우면 레벨업된다.
//! \param Requester 명령을 요청한 플레이어 컨트롤러
//! \param Args [0]=추가할 경험치
//! \return 실행 결과 문자열
FString UCPP_CheatCommandSubsystem::HandleAddExpCommand(AMyPlayerController* Requester, const TArray<FString>& Args)
{
    APlayerCharacterBase* PlayerCharacter = Requester ? Cast<APlayerCharacterBase>(Requester->GetPawn()) : nullptr;
    AMyPlayerState* MyPlayerState = Requester ? Requester->GetPlayerState<AMyPlayerState>() : nullptr;
    if (!PlayerCharacter || !MyPlayerState)
    {
        return TEXT("경험치를 추가할 캐릭터를 찾을 수 없습니다.");
    }

    if (Args.IsEmpty())
    {
        return TEXT("사용법: /addexp <양>");
    }

    const int32 ExpAmount = FCString::Atoi(*Args[0]);
    if (ExpAmount <= 0)
    {
        return TEXT("사용법: /addexp <양> (1 이상)");
    }

    const int32 LevelBefore = PlayerCharacter->GetCharacterLevel();
    PlayerCharacter->AddExperience(ExpAmount);

    return FString::Printf(TEXT("경험치 +%d: 레벨 %d -> %d, 경험치 %d/%d"),
        ExpAmount,
        LevelBefore,
        PlayerCharacter->GetCharacterLevel(),
        MyPlayerState->GetCharacterExp(),
        PlayerCharacter->GetExpRequiredForNextLevel());
}

////////////////////////////
//! \author HanUl
//! \brief /speed : 요청자 캐릭터의 이동 속도(MoveSpeed 어트리뷰트)를 설정하거나 증감한다. (서버에서 실행됨)
//!        어트리뷰트 복제로 모든 클라이언트의 MaxWalkSpeed에 반영된다. /level 실행 시 테이블 값으로 되돌아간다.
//! \param Requester 명령을 요청한 플레이어 컨트롤러
//! \param Args [0]=설정 값 또는 +/-로 시작하는 증감 값 (생략 시 현재 값 표시)
//! \return 실행 결과 문자열
FString UCPP_CheatCommandSubsystem::HandleSetSpeedCommand(AMyPlayerController* Requester, const TArray<FString>& Args)
{
    APlayerCharacterBase* PlayerCharacter = Requester ? Cast<APlayerCharacterBase>(Requester->GetPawn()) : nullptr;
    UMyAttributeSet* AttributeSet = PlayerCharacter ? PlayerCharacter->GetMyAttributeSet() : nullptr;
    if (!AttributeSet)
    {
        return TEXT("이동 속도를 설정할 캐릭터를 찾을 수 없습니다.");
    }

    const float OldMoveSpeed = AttributeSet->GetMoveSpeed();
    if (Args.IsEmpty())
    {
        return FString::Printf(TEXT("현재 이동 속도: %.1f (사용법: /speed <값> 또는 /speed +100)"), OldMoveSpeed);
    }

    const FString& ValueString = Args[0];
    const bool bRelative = ValueString.StartsWith(TEXT("+")) || ValueString.StartsWith(TEXT("-"));
    const float InputValue = FCString::Atof(*ValueString);
    if (!bRelative && InputValue <= 0.0f)
    {
        return TEXT("사용법: /speed <값> (0보다 커야 함) 또는 /speed +100, /speed -100");
    }

    const float NewMoveSpeed = bRelative ? OldMoveSpeed + InputValue : InputValue;
    if (NewMoveSpeed <= 0.0f)
    {
        return FString::Printf(TEXT("결과 이동 속도가 0 이하입니다. (현재 %.1f, 증감 %.1f)"), OldMoveSpeed, InputValue);
    }

    AttributeSet->SetMoveSpeed(NewMoveSpeed);
    return FString::Printf(TEXT("이동 속도: %.1f -> %.1f"), OldMoveSpeed, NewMoveSpeed);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// /fps : 클라이언트 최대 프레임을 설정하는 함수 (bRunOnClient라 요청한 클라에서 즉시 실행됨)
// GameUserSettings에 저장되므로 재실행 후에도 유지된다.
// Requester : 명령을 요청한 플레이어 컨트롤러 (사용 안 함)
// Args : [0]=최대 프레임 (60/120 권장, 0이면 무제한)
// Return Value : 실행 결과 문자열
FString UCPP_CheatCommandSubsystem::HandleFpsCommand(AMyPlayerController* Requester, const TArray<FString>& Args)
{
    if (IsRunningDedicatedServer())
    {
        return TEXT("fps는 클라이언트 전용 명령입니다.");
    }

    if (Args.IsEmpty())
    {
        return TEXT("사용법: /fps <60|120> (0=무제한)");
    }

    const float MaxFps = FCString::Atof(*Args[0]);
    if (MaxFps != 0.0f && (MaxFps < 5.0f || MaxFps > 240.0f))
    {
        return TEXT("5~240 사이 값이나 0(무제한)만 설정할 수 있습니다.");
    }

    UGameUserSettings* GameUserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (!GameUserSettings)
    {
        return TEXT("GameUserSettings를 찾을 수 없습니다.");
    }

    GameUserSettings->SetFrameRateLimit(MaxFps);
    GameUserSettings->ApplySettings(false);

    if (MaxFps == 0.0f)
    {
        return TEXT("최대 프레임 제한을 해제했습니다.");
    }

    return FString::Printf(TEXT("최대 프레임을 %.0f로 설정했습니다."), MaxFps);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// /stats : 화면 좌측 스탯 디버그 위젯을 토글하는 함수 (bRunOnClient라 요청한 클라에서 즉시 실행됨)
// 값은 복제된 PlayerState/ASC에서 읽기만 하므로 서버 동작에 영향 없음.
// Requester : 명령을 요청한 플레이어 컨트롤러 (없으면 로컬 첫 컨트롤러 사용)
// Args : 사용 안 함
// Return Value : 실행 결과 문자열
FString UCPP_CheatCommandSubsystem::HandleStatsCommand(AMyPlayerController* Requester, const TArray<FString>& Args)
{
    if (IsRunningDedicatedServer())
    {
        return TEXT("stats는 클라이언트 전용 명령입니다.");
    }

    if (StatDebugWidget)
    {
        StatDebugWidget->RemoveFromParent();
        StatDebugWidget = nullptr;
        return TEXT("스탯 디버그 창을 껐습니다.");
    }

    APlayerController* OwningPC = Requester;
    if (!OwningPC || !OwningPC->IsLocalPlayerController())
    {
        OwningPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    }

    if (!OwningPC || !OwningPC->IsLocalPlayerController())
    {
        return TEXT("로컬 플레이어 컨트롤러를 찾을 수 없습니다.");
    }

    StatDebugWidget = CreateWidget<UMyStatDebugWidget>(OwningPC, UMyStatDebugWidget::StaticClass());
    if (!StatDebugWidget)
    {
        return TEXT("스탯 디버그 위젯 생성에 실패했습니다.");
    }

    // 디버그 오버레이는 다른 UI 위에 보여야 하므로 높은 ZOrder로 뷰포트에 직접 올린다
    StatDebugWidget->AddToViewport(1000);
    return TEXT("스탯 디버그 창을 켰습니다. (/stats 로 닫기)");
}

////////////////////////////
//! \author HanUl
//! \brief 요청한 로컬 플레이어의 스킬 판정 DebugLine 표시 상태를 설정한다.
//! \param Requester 명령을 실행한 로컬 PlayerController
//! \param Args 첫 번째 인자에 on 또는 off
//! \return 명령 실행 결과 문자열
FString UCPP_CheatCommandSubsystem::HandleDebugLineCommand(
    AMyPlayerController* Requester,
    const TArray<FString>& Args)
{
    if (!Requester || !Requester->IsLocalPlayerController())
    {
        return TEXT("로컬 플레이어 컨트롤러를 찾을 수 없습니다.");
    }

    if (Args.Num() != 1)
    {
        return TEXT("사용법: /debugline <on|off>");
    }

    const bool bEnable = Args[0].Equals(TEXT("on"), ESearchCase::IgnoreCase);
    const bool bDisable = Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase);
    if (!bEnable && !bDisable)
    {
        return TEXT("사용법: /debugline <on|off>");
    }

    Requester->SetSkillDebugLineEnabled(bEnable);
    return bEnable
        ? TEXT("스킬 판정 DebugLine을 켰습니다.")
        : TEXT("스킬 판정 DebugLine을 껐습니다.");
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// /additem : 요청자의 인벤토리에 아이템을 지급하는 함수 (서버에서 실행됨)
// Requester : 명령을 요청한 플레이어 컨트롤러
// Args : [0]=아이템 ID (데이터테이블 Row Name) 또는 "All", [1]=개수 (생략 시 1, All은 무시하고 최대치)
// Return Value : 실행 결과 문자열
FString UCPP_CheatCommandSubsystem::HandleAddItemCommand(AMyPlayerController* Requester, const TArray<FString>& Args)
{
    if (Args.IsEmpty())
    {
        return TEXT("사용법: /additem <아이템ID|All> [개수]");
    }

    const AMyPlayerState* MyPlayerState = Requester ? Requester->GetPlayerState<AMyPlayerState>() : nullptr;
    UMyInventoryComponent* Inventory = MyPlayerState ? MyPlayerState->GetInventoryComponent() : nullptr;
    if (!Inventory)
    {
        return TEXT("인벤토리 컴포넌트를 찾을 수 없습니다. (PlayerState 확인)");
    }

    // "All" : 데이터테이블의 모든 아이템을 앞에서부터(Row 순서) 각각 최대치(MaxStackCount)로 지급한다.
    // 새 종류마다 한 칸을 쓰므로 인벤토리 최대 슬롯(MaxSlots)까지만 채워진다. (개수 인자는 무시)
    if (Args[0].Equals(TEXT("All"), ESearchCase::IgnoreCase))
    {
        TArray<FName> ItemIds;
        Inventory->GetAllItemIds(ItemIds);
        if (ItemIds.IsEmpty())
        {
            return TEXT("지급 실패: 데이터테이블이 비어 있거나 미지정입니다.");
        }

        int32 GrantedTypes = 0;
        for (const FName& CurrentItemId : ItemIds)
        {
            FMyItemData ItemData;
            if (!Inventory->FindItemData(CurrentItemId, ItemData))
            {
                continue;
            }

            if (Inventory->AddItem(CurrentItemId, ItemData.MaxStackCount))
            {
                ++GrantedTypes;
            }
        }

        FString Result = FString::Printf(TEXT("전체 아이템 %d종을 최대치로 지급했습니다. (데이터테이블 %d개, 인벤토리 %d/%d칸)"),
            GrantedTypes, ItemIds.Num(), Inventory->GetUsedSlots(), Inventory->GetMaxSlots());
        if (ItemIds.Num() > GrantedTypes)
        {
            Result += FString::Printf(TEXT(" - %d개는 슬롯 부족으로 미지급"), ItemIds.Num() - GrantedTypes);
        }
        return Result;
    }

    const FName ItemId(*Args[0]);
    const int32 Count = Args.Num() >= 2 ? FCString::Atoi(*Args[1]) : 1;
    if (Count <= 0)
    {
        return TEXT("개수는 1 이상이어야 합니다.");
    }

    if (!Inventory->AddItem(ItemId, Count))
    {
        return FString::Printf(TEXT("지급 실패: '%s' - 데이터테이블에 없는 아이템 ID이거나 테이블 미지정."), *ItemId.ToString());
    }

    return FString::Printf(TEXT("'%s' %d개 지급 (보유 %d개)"), *ItemId.ToString(), Count, Inventory->GetItemCount(ItemId));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// /addmeso : 요청자의 인벤토리에 메소를 지급하는 함수 (서버에서 실행됨)
// Requester : 명령을 요청한 플레이어 컨트롤러
// Args : [0]=지급량
// Return Value : 실행 결과 문자열
FString UCPP_CheatCommandSubsystem::HandleAddMesoCommand(AMyPlayerController* Requester, const TArray<FString>& Args)
{
    if (Args.IsEmpty())
    {
        return TEXT("사용법: /addmeso <양>");
    }

    const int32 Amount = FCString::Atoi(*Args[0]);
    if (Amount <= 0)
    {
        return TEXT("양은 1 이상이어야 합니다.");
    }

    const AMyPlayerState* MyPlayerState = Requester ? Requester->GetPlayerState<AMyPlayerState>() : nullptr;
    UMyInventoryComponent* Inventory = MyPlayerState ? MyPlayerState->GetInventoryComponent() : nullptr;
    if (!Inventory)
    {
        return TEXT("인벤토리 컴포넌트를 찾을 수 없습니다. (PlayerState 확인)");
    }

	Inventory->AddMeso(Amount, MyGameplayTags::Meso_Source_Cheat);
    return FString::Printf(TEXT("메소 %d 지급 (보유 %d)"), Amount, Inventory->GetMeso());
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// /additem 인자 자동완성 : 첫 번째 인자(아이템 ID) 입력 시 데이터테이블의 아이템 ID 후보를 채우는 함수
// 클라 월드에서 실행되며, 로컬 플레이어 PlayerState의 인벤토리 컴포넌트에서 테이블을 읽는다.
// ArgIndex : 입력 중인 인자 인덱스
// Prefix : 입력 중인 접두어 (대소문자 무시 매칭)
// OutSuggestions : 후보 아이템 ID 목록(출력)
void UCPP_CheatCommandSubsystem::HandleAddItemArgumentSuggestions(int32 ArgIndex, const FString& Prefix, TArray<FString>& OutSuggestions)
{
    // 첫 번째 인자(아이템 ID)만 후보를 제공한다. 두 번째 인자는 개수라 후보가 없다.
    if (ArgIndex != 0)
    {
        return;
    }

    // "All"(전체 아이템 최대치 지급) 특수 인자를 후보 맨 앞에 노출한다. (테이블 미지정이어도 표시)
    if (Prefix.IsEmpty() || FString(TEXT("All")).StartsWith(Prefix, ESearchCase::IgnoreCase))
    {
        OutSuggestions.Add(TEXT("All"));
    }

    const APlayerController* LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    const APlayerState* LocalPlayerState = LocalPC ? LocalPC->PlayerState.Get() : nullptr;
    const UMyInventoryComponent* Inventory = LocalPlayerState ? LocalPlayerState->FindComponentByClass<UMyInventoryComponent>() : nullptr;
    if (!Inventory)
    {
        return;
    }

    TArray<FName> ItemIds;
    Inventory->GetAllItemIds(ItemIds);

    for (const FName& ItemId : ItemIds)
    {
        const FString ItemIdString = ItemId.ToString();
        if (Prefix.IsEmpty() || ItemIdString.StartsWith(Prefix, ESearchCase::IgnoreCase))
        {
            OutSuggestions.Add(ItemIdString);
        }
    }
}

////////////////////////////
//! \author HanUl
//! \brief /revive 첫 번째 인자에 DungeonGS ReviveDataAsset의 활성 OptionId를 제안한다.
//! \param ArgIndex 입력 중인 인자 인덱스
//! \param Prefix 입력 중인 옵션 ID 접두어
//! \param OutSuggestions 자동완성 후보 목록
//! \return 없음
void UCPP_CheatCommandSubsystem::HandleReviveArgumentSuggestions(
    int32 ArgIndex,
    const FString& Prefix,
    TArray<FString>& OutSuggestions)
{
    if (ArgIndex != 0)
    {
        return;
    }

    const ADungeonGS* DungeonGS = GetWorld() ? GetWorld()->GetGameState<ADungeonGS>() : nullptr;
    const UDungeonReviveDataAsset* ReviveData = DungeonGS ? DungeonGS->GetReviveData() : nullptr;
    if (!ReviveData)
    {
        return;
    }

    TArray<FName> OptionIds;
    ReviveData->GetEnabledOptionIds(OptionIds);
    for (const FName OptionId : OptionIds)
    {
        const FString OptionIdString = OptionId.ToString();
        if (Prefix.IsEmpty() || OptionIdString.StartsWith(Prefix, ESearchCase::IgnoreCase))
        {
            OutSuggestions.Add(OptionIdString);
        }
    }
}

////////////////////////////
//! \author HanUl
//! \brief /debugline 첫 번째 인자에 on과 off 자동완성 후보를 제공한다.
//! \param ArgIndex 입력 중인 인자 인덱스
//! \param Prefix 입력 중인 접두어
//! \param OutSuggestions 자동완성 후보 목록
//! \return 없음
void UCPP_CheatCommandSubsystem::HandleDebugLineArgumentSuggestions(
    int32 ArgIndex,
    const FString& Prefix,
    TArray<FString>& OutSuggestions)
{
    if (ArgIndex != 0)
    {
        return;
    }

    static const TCHAR* DebugLineArguments[] = { TEXT("on"), TEXT("off") };
    for (const TCHAR* Argument : DebugLineArguments)
    {
        const FString Candidate(Argument);
        if (Prefix.IsEmpty() || Candidate.StartsWith(Prefix, ESearchCase::IgnoreCase))
        {
            OutSuggestions.Add(Candidate);
        }
    }
}

namespace
{
    ////////////////////////////
    //! \author 준혁
    //! \brief 월드에 배치된 ZoneManager를 찾는다. (레벨당 1개 가정)
    //! \param World 검색할 월드
    //! \return 첫 번째 ZoneManager, 없으면 nullptr
    AZoneManager* FindZoneManager(UWorld* World)
    {
        if (!World)
        {
            return nullptr;
        }

        for (TActorIterator<AZoneManager> It(World); It; ++It)
        {
            return *It;
        }

        return nullptr;
    }
}

////////////////////////////
//! \author 준혁
//! \brief /spawn : 지정 존의 스폰 지점에 몬스터를 원하는 마릿수만큼 스폰하는 함수 (서버에서 실행됨)
//!        몬스터 클래스는 해당 존 WaveData의 적 구성을 순환 사용하고, 없으면 GM의 StressTestEnemyClass를 사용한다.
//!        스폰된 몬스터는 일반 스폰과 동일하게 등록되어, Active 존이면 전멸 클리어 집계에 포함된다.
//! \param Requester 명령을 요청한 플레이어 컨트롤러
//! \param Args [0]=존번호(0부터) 또는 "here"(요청자가 서 있는 존), [1]=마릿수
//! \return 실행 결과 문자열
FString UCPP_CheatCommandSubsystem::HandleSpawnMonsterCommand(AMyPlayerController* Requester, const TArray<FString>& Args)
{
    if (Args.Num() < 2)
    {
        return TEXT("사용법: /spawn <존번호|here> <마릿수>");
    }

    AZoneManager* ZoneManager = FindZoneManager(GetWorld());
    if (!ZoneManager)
    {
        return TEXT("ZoneManager를 찾을 수 없습니다. (존이 배치되지 않은 맵)");
    }

    const int32 ZoneCount = ZoneManager->GetZoneCount();

    // 대상 존 결정: "here"면 요청자가 서 있는 존, 아니면 존번호(OrderedZones 배열 인덱스, 0부터).
    AZoneBase* TargetZone = nullptr;
    int32 TargetZoneIndex = INDEX_NONE;
    if (Args[0].Equals(TEXT("here"), ESearchCase::IgnoreCase))
    {
        const APawn* RequesterPawn = Requester ? Requester->GetPawn() : nullptr;
        if (!RequesterPawn)
        {
            return TEXT("요청자 캐릭터를 찾을 수 없습니다.");
        }

        const FVector PawnLocation = RequesterPawn->GetActorLocation();
        for (int32 i = 0; i < ZoneCount; ++i)
        {
            AZoneBase* Zone = ZoneManager->GetZoneAt(i);
            if (Zone && Zone->ContainsLocation(PawnLocation))
            {
                TargetZone = Zone;
                TargetZoneIndex = i;
                break;
            }
        }

        if (!TargetZone)
        {
            return TEXT("현재 위치가 어떤 존 경계에도 속하지 않습니다. 존번호로 지정하세요.");
        }
    }
    else
    {
        if (!Args[0].IsNumeric())
        {
            return TEXT("사용법: /spawn <존번호|here> <마릿수> (존번호는 0부터)");
        }

        TargetZoneIndex = FCString::Atoi(*Args[0]);
        TargetZone = ZoneManager->GetZoneAt(TargetZoneIndex);
        if (!TargetZone)
        {
            return FString::Printf(TEXT("존 %d이(가) 없습니다. (사용 가능: 0 ~ %d)"), TargetZoneIndex, ZoneCount - 1);
        }
    }

    const int32 RequestedCount = FCString::Atoi(*Args[1]);
    if (RequestedCount <= 0)
    {
        return TEXT("마릿수는 1 이상이어야 합니다.");
    }

    UCPP_SpawnerComponent* Spawner = TargetZone->FindComponentByClass<UCPP_SpawnerComponent>();
    if (!Spawner)
    {
        return FString::Printf(TEXT("존 %d(%s)에는 스포너 컴포넌트가 없습니다."), TargetZoneIndex, *TargetZone->GetName());
    }

    // 스폰 상한과 폴백 클래스는 GM의 스트레스 테스트 설정을 재사용한다.
    const ACPP_DungeonGM* DungeonGM = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_DungeonGM>() : nullptr;
    const int32 MaxCount = DungeonGM ? DungeonGM->GetMaxStressTestEnemySpawnCount() : 1000;
    const int32 SpawnCount = FMath::Clamp(RequestedCount, 1, MaxCount);

    const int32 SpawnedCount = Spawner->CheatSpawnEnemies(SpawnCount, DungeonGM ? DungeonGM->GetStressTestEnemyClass() : nullptr);
    if (SpawnedCount <= 0)
    {
        return FString::Printf(TEXT("존 %d 스폰 실패: 스폰할 몬스터 클래스가 없습니다. (WaveData 비어 있음, GM StressTestEnemyClass 미지정)"), TargetZoneIndex);
    }

    FString Result = FString::Printf(TEXT("존 %d(%s, 상태 %s)에 몬스터 %d마리 스폰"),
        TargetZoneIndex,
        *TargetZone->GetName(),
        *UEnum::GetDisplayValueAsText(TargetZone->GetZoneState()).ToString(),
        SpawnedCount);
    if (SpawnCount < RequestedCount)
    {
        Result += FString::Printf(TEXT(" (요청 %d마리는 상한 %d 초과로 제한됨)"), RequestedCount, MaxCount);
    }

    return Result;
}

#if !UE_BUILD_SHIPPING
////////////////////////////
//! \author 장효제
//! \brief 다음 진행 Zone을 실제 Zone Clear 델리게이트 경로로 강제 완료한다.
//! \param Requester 명령을 요청한 플레이어 컨트롤러다.
//! \param Args Args[0]은 OrderedZones의 0-based 인덱스다.
//! \return 실행 결과 또는 거부 이유다.
FString UCPP_CheatCommandSubsystem::HandleClearZoneCommand(
    AMyPlayerController* Requester,
    const TArray<FString>& Args)
{
    if (Args.IsEmpty() || !Args[0].IsNumeric())
    {
        return TEXT("사용법: /clearzone <존번호> (존번호는 0부터)");
    }

    AZoneManager* ZoneManager = FindZoneManager(GetWorld());
    if (!ZoneManager)
    {
        return TEXT("ZoneManager를 찾을 수 없습니다. (존이 배치되지 않은 맵)");
    }

    const int32 ZoneIndex = FCString::Atoi(*Args[0]);
    AZoneBase* TargetZone = ZoneManager->GetZoneAt(ZoneIndex);
    if (!TargetZone)
    {
        return FString::Printf(
            TEXT("존 %d이(가) 없습니다. (사용 가능: 0 ~ %d)"),
            ZoneIndex,
            ZoneManager->GetZoneCount() - 1);
    }

    const int32 ExpectedZoneIndex = ZoneManager->GetCurrentZoneIndex() + 1;
    if (ZoneIndex != ExpectedZoneIndex)
    {
        return FString::Printf(
            TEXT("다음 진행 존만 Clear할 수 있습니다. (요청: %d, 다음 존: %d)"),
            ZoneIndex,
            ExpectedZoneIndex);
    }

    if (!TargetZone->CheatForceClear())
    {
        return FString::Printf(
            TEXT("존 %d 강제 Clear 실패 (상태: %s)"),
            ZoneIndex,
            *UEnum::GetDisplayValueAsText(TargetZone->GetZoneState()).ToString());
    }

    return FString::Printf(
        TEXT("존 %d(%s)을 강제로 Clear했습니다."),
        ZoneIndex,
        *TargetZone->GetName());
}
#endif

////////////////////////////
//! \author 준혁
//! \brief /spawn 인자 자동완성 : 첫 번째 인자(존 대상) 입력 시 "here"와 배치된 존번호 후보를 채우는 함수 (클라 월드에서 실행됨)
//! \param ArgIndex 입력 중인 인자 인덱스
//! \param Prefix 입력 중인 접두어 (대소문자 무시 매칭)
//! \param OutSuggestions 후보 문자열 목록(출력)
//! \return
void UCPP_CheatCommandSubsystem::HandleSpawnMonsterArgumentSuggestions(int32 ArgIndex, const FString& Prefix, TArray<FString>& OutSuggestions)
{
    // 첫 번째 인자(존 대상)만 후보를 제공한다. 두 번째 인자는 마릿수라 후보가 없다.
    if (ArgIndex != 0)
    {
        return;
    }

    if (Prefix.IsEmpty() || FString(TEXT("here")).StartsWith(Prefix, ESearchCase::IgnoreCase))
    {
        OutSuggestions.Add(TEXT("here"));
    }

    const AZoneManager* ZoneManager = FindZoneManager(GetWorld());
    if (!ZoneManager)
    {
        return;
    }

    for (int32 i = 0; i < ZoneManager->GetZoneCount(); ++i)
    {
        const FString IndexString = FString::FromInt(i);
        if (Prefix.IsEmpty() || IndexString.StartsWith(Prefix))
        {
            OutSuggestions.Add(IndexString);
        }
    }
}

#if !UE_BUILD_SHIPPING
////////////////////////////
//! \author 장효제
//! \brief /clearzone의 첫 인자로 아직 Clear하지 않은 다음 ZoneIndex 하나만 제안한다.
//! \param ArgIndex 입력 중인 인자 인덱스다.
//! \param Prefix 입력 중인 접두어다.
//! \param OutSuggestions 자동완성 후보 목록이다.
//! \return
void UCPP_CheatCommandSubsystem::HandleClearZoneArgumentSuggestions(
    int32 ArgIndex,
    const FString& Prefix,
    TArray<FString>& OutSuggestions)
{
    if (ArgIndex != 0)
    {
        return;
    }

    const AZoneManager* ZoneManager = FindZoneManager(GetWorld());
    if (!ZoneManager)
    {
        return;
    }

    const int32 NextZoneIndex = ZoneManager->GetCurrentZoneIndex() + 1;
    if (!ZoneManager->GetZoneAt(NextZoneIndex))
    {
        return;
    }

    const FString IndexString = FString::FromInt(NextZoneIndex);
    if (Prefix.IsEmpty() || IndexString.StartsWith(Prefix))
    {
        OutSuggestions.Add(IndexString);
    }
}
#endif
