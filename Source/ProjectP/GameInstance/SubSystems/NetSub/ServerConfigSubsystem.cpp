#include "ServerConfigSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace ServerConfigConstants
{
    static constexpr TCHAR ServerConfigSection[] = TEXT("ProjectP.Server");
    static constexpr TCHAR ExternalServerConfigFileName[] = TEXT("ServerRuntime.ini");
    static constexpr TCHAR ExternalServerConfigCommandLineKey[] = TEXT("ProjectPServerConfig=");
    static constexpr TCHAR ServerHostKey[] = TEXT("ServerHost");
    static constexpr TCHAR GameBackendPortKey[] = TEXT("GameBackendPort");
    static constexpr TCHAR LoginServerPortKey[] = TEXT("LoginServerPort");
    static constexpr TCHAR LobbyServerPortKey[] = TEXT("LobbyServerPort");
    static constexpr TCHAR DungeonServerPortKey[] = TEXT("DungeonServerPort");
    static constexpr TCHAR RequireLobbyTokenVerificationKey[] = TEXT("bRequireLobbyTokenVerification");
    static constexpr TCHAR AllowChatCheatsKey[] = TEXT("bAllowChatCheats");
    static constexpr TCHAR DungeonStateServerAuthKeyKey[] = TEXT("DungeonStateServerAuthKey");

    static constexpr TCHAR LoginEndpoint[] = TEXT("/api/login");
    static constexpr TCHAR RegisterEndpoint[] = TEXT("/api/register");
    static constexpr TCHAR LogoutEndpoint[] = TEXT("/api/logout");
    static constexpr TCHAR SessionPingEndpoint[] = TEXT("/api/session/ping");
    static constexpr TCHAR SessionVerifyEndpoint[] = TEXT("/api/session/verify");
    static constexpr TCHAR DungeonSessionVerifyEndpoint[] = TEXT("/api/dungeon/session/verify");
    static constexpr TCHAR DungeonMemberStateEndpoint[] = TEXT("/api/dungeon/member-state");
    static constexpr TCHAR DungeonMemberStateQueryEndpoint[] = TEXT("/api/dungeon/member-state/query");
    static constexpr TCHAR DungeonAllocateEndpoint[] = TEXT("/api/dungeon/allocate");
    static constexpr TCHAR DungeonShutdownEndpoint[] = TEXT("/api/dungeon/shutdown");
    static constexpr TCHAR LobbyTelemetryEndpoint[] = TEXT("/api/telemetry/lobby");
}

namespace
{
    void AddUniqueExternalServerConfigPath(TArray<FString>& CandidatePaths, const FString& ConfigPath)
    {
        FString NormalizedPath = ConfigPath;
        NormalizedPath.TrimStartAndEndInline();
        if (NormalizedPath.IsEmpty())
        {
            return;
        }

        NormalizedPath = FPaths::ConvertRelativePathToFull(NormalizedPath);
        FPaths::NormalizeFilename(NormalizedPath);
        CandidatePaths.AddUnique(NormalizedPath);
    }

    TArray<FString> BuildExternalServerConfigPaths()
    {
        TArray<FString> CandidatePaths;

        FString CommandLineConfigPath;
        if (FParse::Value(FCommandLine::Get(), ServerConfigConstants::ExternalServerConfigCommandLineKey, CommandLineConfigPath))
        {
            AddUniqueExternalServerConfigPath(CandidatePaths, CommandLineConfigPath);
        }

        AddUniqueExternalServerConfigPath(CandidatePaths, FPaths::Combine(FPaths::LaunchDir(), ServerConfigConstants::ExternalServerConfigFileName));
        AddUniqueExternalServerConfigPath(CandidatePaths, FPaths::Combine(FPaths::ProjectDir(), ServerConfigConstants::ExternalServerConfigFileName));
        AddUniqueExternalServerConfigPath(CandidatePaths, FPaths::Combine(FPaths::ProjectConfigDir(), ServerConfigConstants::ExternalServerConfigFileName));
        AddUniqueExternalServerConfigPath(CandidatePaths, FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"), ServerConfigConstants::ExternalServerConfigFileName));

        return CandidatePaths;
    }

    bool ReadExternalServerConfigProjectPServerString(const TCHAR* Key, FString& OutValue)
    {
        for (const FString& ConfigPath : BuildExternalServerConfigPaths())
        {
            if (!FPaths::FileExists(ConfigPath))
            {
                continue;
            }

            FConfigFile RuntimeConfig;
            RuntimeConfig.Read(ConfigPath);
            if (RuntimeConfig.GetString(ServerConfigConstants::ServerConfigSection, Key, OutValue))
            {
                OutValue.TrimStartAndEndInline();
                if (!OutValue.IsEmpty())
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool ReadExternalServerConfigProjectPServerInt(const TCHAR* Key, int32& OutValue)
    {
        for (const FString& ConfigPath : BuildExternalServerConfigPaths())
        {
            if (!FPaths::FileExists(ConfigPath))
            {
                continue;
            }

            FConfigFile RuntimeConfig;
            RuntimeConfig.Read(ConfigPath);
            if (RuntimeConfig.GetInt(ServerConfigConstants::ServerConfigSection, Key, OutValue))
            {
                return true;
            }
        }

        return false;
    }

    bool ReadExternalServerConfigProjectPServerBool(const TCHAR* Key, bool& bOutValue)
    {
        for (const FString& ConfigPath : BuildExternalServerConfigPaths())
        {
            if (!FPaths::FileExists(ConfigPath))
            {
                continue;
            }

            FConfigFile RuntimeConfig;
            RuntimeConfig.Read(ConfigPath);
            if (RuntimeConfig.GetBool(ServerConfigConstants::ServerConfigSection, Key, bOutValue))
            {
                return true;
            }
        }

        return false;
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // ProjectP 서버 설정 문자열 값을 읽어오는 함수
    // Key : 읽어올 설정 키
    // DefaultValue : 설정 값이 없거나 비어 있을 때 사용할 기본값
    // Return Value : 설정 파일에서 읽은 문자열 값 또는 기본값
    FString ReadServerConfigProjectPServerString(const TCHAR* Key, const TCHAR* DefaultValue)
    {
        FString Value;
        if (ReadExternalServerConfigProjectPServerString(Key, Value))
        {
            return Value;
        }

        if (GConfig && GConfig->GetString(ServerConfigConstants::ServerConfigSection, Key, Value, GGameIni))
        {
            Value.TrimStartAndEndInline();
            if (!Value.IsEmpty())
            {
                return Value;
            }
        }

        return DefaultValue;
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // ProjectP 서버 설정 정수 값을 읽어오는 함수
    // Key : 읽어올 설정 키
    // DefaultValue : 설정 값이 없을 때 사용할 기본값
    // Return Value : 설정 파일에서 읽은 정수 값 또는 기본값
    int32 ReadServerConfigProjectPServerInt(const TCHAR* Key, int32 DefaultValue)
    {
        int32 Value = DefaultValue;
        if (ReadExternalServerConfigProjectPServerInt(Key, Value))
        {
            return Value;
        }

        if (GConfig)
        {
            GConfig->GetInt(ServerConfigConstants::ServerConfigSection, Key, Value, GGameIni);
        }

        return Value;
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // 공통 서버 Host와 포트 설정을 조합해서 서버 접속 주소를 만드는 함수
    // PortKey : 읽어올 포트 설정 키
    // Return Value : ServerHost와 포트가 합쳐진 접속 주소
    FString BuildServerConfigProjectPServerAddress(const TCHAR* PortKey)
    {
        FString ServerHost = ReadServerConfigProjectPServerString(ServerConfigConstants::ServerHostKey, TEXT(""));
        const int32 ServerPort = ReadServerConfigProjectPServerInt(PortKey, 0);

        ServerHost.TrimStartAndEndInline();
        while (ServerHost.EndsWith(TEXT("/")))
        {
            ServerHost.LeftChopInline(1);
        }

        if (ServerHost.IsEmpty() || ServerPort <= 0)
        {
            return TEXT("");
        }

        return FString::Printf(TEXT("%s:%d"), *ServerHost, ServerPort);
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // ProjectP 서버 설정 bool 값을 읽어오는 함수
    // Key : 읽어올 설정 키
    // bDefaultValue : 설정 값이 없을 때 사용할 기본값
    // Return Value : 설정 파일에서 읽은 bool 값 또는 기본값
    bool ReadServerConfigProjectPServerBool(const TCHAR* Key, bool bDefaultValue)
    {
        bool bValue = bDefaultValue;
        if (ReadExternalServerConfigProjectPServerBool(Key, bValue))
        {
            return bValue;
        }

        if (GConfig)
        {
            GConfig->GetBool(ServerConfigConstants::ServerConfigSection, Key, bValue, GGameIni);
        }

        return bValue;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 서버 기본 URL을 반환하는 함수
// Return Value : GameBackend 기본 URL
FString UServerConfigSubsystem::GetLoginServerBaseUrl() const
{
    return GetGameBackendBaseUrl();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// GameBackend 기본 URL을 반환하는 함수
// Return Value : 프로토콜과 슬래시가 정리된 GameBackend 기본 URL
FString UServerConfigSubsystem::GetGameBackendBaseUrl() const
{
    FString BaseUrl = BuildServerConfigProjectPServerAddress(ServerConfigConstants::GameBackendPortKey);
    if (BaseUrl.IsEmpty())
    {
        BaseUrl = BuildServerConfigProjectPServerAddress(ServerConfigConstants::LoginServerPortKey);
    }

    BaseUrl.TrimStartAndEndInline();
    if (BaseUrl.Equals(TEXT("http:"), ESearchCase::IgnoreCase) || BaseUrl.Equals(TEXT("https:"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogTemp, Warning, TEXT("GameBackendBaseUrl was parsed as '%s'. Use ServerHost and GameBackendPort in ServerRuntime.ini or DefaultGame.ini."), *BaseUrl);
        return TEXT("");
    }

    if (BaseUrl.IsEmpty())
    {
        return TEXT("");
    }

    if (!BaseUrl.Contains(TEXT("://")))
    {
        BaseUrl = FString::Printf(TEXT("http://%s"), *BaseUrl);
    }

    while (BaseUrl.EndsWith(TEXT("/")))
    {
        BaseUrl.LeftChopInline(1);
    }

    return BaseUrl;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 요청 URL을 반환하는 함수
// Return Value : GameBackend 기본 URL과 로그인 Endpoint가 합쳐진 URL
FString UServerConfigSubsystem::GetLoginRequestUrl() const
{
    return BuildGameBackendUrl(ServerConfigConstants::LoginEndpoint);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 회원 가입 요청 URL을 반환하는 함수
// Return Value : GameBackend 기본 URL과 회원 가입 Endpoint가 합쳐진 URL
FString UServerConfigSubsystem::GetRegisterRequestUrl() const
{
    return BuildGameBackendUrl(ServerConfigConstants::RegisterEndpoint);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그아웃 요청 URL을 반환하는 함수
// Return Value : GameBackend 기본 URL과 로그아웃 Endpoint가 합쳐진 URL
FString UServerConfigSubsystem::GetLogoutRequestUrl() const
{
    return BuildGameBackendUrl(ServerConfigConstants::LogoutEndpoint);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 세션 ping 요청 URL을 반환하는 함수
// Return Value : GameBackend 기본 URL과 세션 ping Endpoint가 합쳐진 URL
FString UServerConfigSubsystem::GetSessionPingUrl() const
{
    return BuildGameBackendUrl(ServerConfigConstants::SessionPingEndpoint);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 세션 검증 요청 URL을 반환하는 함수
// Return Value : GameBackend 기본 URL과 세션 검증 Endpoint가 합쳐진 URL
FString UServerConfigSubsystem::GetSessionVerifyUrl() const
{
    return BuildGameBackendUrl(ServerConfigConstants::SessionVerifyEndpoint);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 세션 입장 검증 요청 URL을 반환하는 함수
// Return Value : GameBackend 기본 URL과 던전 세션 검증 Endpoint가 합쳐진 URL
FString UServerConfigSubsystem::GetDungeonSessionVerifyUrl() const
{
    return BuildGameBackendUrl(ServerConfigConstants::DungeonSessionVerifyEndpoint);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 멤버 접속 상태 보고 URL을 반환하는 함수
// Return Value : GameBackend 기본 URL과 던전 멤버 상태 보고 Endpoint가 합쳐진 URL
FString UServerConfigSubsystem::GetDungeonMemberStateUrl() const
{
    return BuildGameBackendUrl(ServerConfigConstants::DungeonMemberStateEndpoint);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 멤버 접속 상태 조회 URL을 반환하는 함수
// Return Value : GameBackend 기본 URL과 던전 멤버 상태 조회 Endpoint가 합쳐진 URL
FString UServerConfigSubsystem::GetDungeonMemberStateQueryUrl() const
{
    return BuildGameBackendUrl(ServerConfigConstants::DungeonMemberStateQueryEndpoint);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 서버 할당 요청 URL을 반환하는 함수
// Return Value : GameBackend 기본 URL과 던전 서버 할당 Endpoint가 합쳐진 URL
FString UServerConfigSubsystem::GetDungeonAllocateUrl() const
{
    return BuildGameBackendUrl(ServerConfigConstants::DungeonAllocateEndpoint);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 서버 종료 요청 URL을 반환하는 함수
// Return Value : GameBackend 기본 URL과 던전 서버 종료 Endpoint가 합쳐진 URL
FString UServerConfigSubsystem::GetDungeonShutdownUrl() const
{
    return BuildGameBackendUrl(ServerConfigConstants::DungeonShutdownEndpoint);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버 접속 현황 보고 URL을 반환하는 함수
// Return Value : GameBackend 기본 URL과 로비 텔레메트리 Endpoint가 합쳐진 URL
FString UServerConfigSubsystem::GetLobbyTelemetryUrl() const
{
    return BuildGameBackendUrl(ServerConfigConstants::LobbyTelemetryEndpoint);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버 접속 주소를 반환하는 함수
// Return Value : 설정 파일의 로비 서버 접속 주소
FString UServerConfigSubsystem::GetLobbyServerAddress() const
{
    return BuildServerConfigProjectPServerAddress(ServerConfigConstants::LobbyServerPortKey);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 서버 접속 주소를 반환하는 함수
// Return Value : 설정 파일의 던전 서버 접속 주소
FString UServerConfigSubsystem::GetDungeonServerAddress() const
{
    return BuildServerConfigProjectPServerAddress(ServerConfigConstants::DungeonServerPortKey);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 서버가 GameBackend에 상태 보고를 보낼 때 사용할 서버 인증키를 반환하는 함수
// Return Value : 던전 상태 보고 서버 인증키
FString UServerConfigSubsystem::GetDungeonStateServerAuthKey() const
{
    return ReadServerConfigProjectPServerString(ServerConfigConstants::DungeonStateServerAuthKeyKey, TEXT(""));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버 입장 때 로그인 토큰 검증이 필요한지 확인하는 함수
// Return Value : 토큰 검증이 필요하면 true, 필요하지 않으면 false
bool UServerConfigSubsystem::IsLobbyTokenVerificationRequired() const
{
    return ReadServerConfigProjectPServerBool(ServerConfigConstants::RequireLobbyTokenVerificationKey, true);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 채팅창 치트 명령 실행이 허용되는지 확인하는 함수
// Return Value : 치트가 허용되면 true, 기본값은 false
bool UServerConfigSubsystem::IsChatCheatAllowed() const
{
    return ReadServerConfigProjectPServerBool(ServerConfigConstants::AllowChatCheatsKey, false);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// GameBackend 기본 URL과 Endpoint 경로를 합쳐 최종 요청 URL을 만드는 함수
// EndpointPath : GameBackend API Endpoint 경로
// Return Value : GameBackend 기본 URL과 Endpoint가 합쳐진 최종 요청 URL
FString UServerConfigSubsystem::BuildGameBackendUrl(const FString& EndpointPath) const
{
    const FString BaseUrl = GetGameBackendBaseUrl();
    if (BaseUrl.IsEmpty())
    {
        return TEXT("");
    }

    FString Endpoint = EndpointPath;
    Endpoint.TrimStartAndEndInline();

    if (Endpoint.IsEmpty())
    {
        return BaseUrl;
    }

    if (!Endpoint.StartsWith(TEXT("/")))
    {
        Endpoint = TEXT("/") + Endpoint;
    }

    return BaseUrl + Endpoint;
}
