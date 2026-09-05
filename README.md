# D.O.G (ProjectP)

Unreal Engine 5.7 기반 **3인 협동 멀티플레이 던전 액션** 게임 프로젝트입니다.
Dedicated Server 구조를 전제로 설계되었으며, 게임 클라이언트/서버 C++ 코드와
Node.js 백엔드, CI/E2E 자동화 파이프라인으로 구성되어 있습니다.

> **이 저장소는 포트폴리오용 코드 전용 저장소입니다.**
> 언리얼 에셋(`Content/`, 약 7GB), 빌드 산출물, 게임 데이터, 내부 문서는 포함하지 않으므로
> 클론 후 바로 빌드되지 않습니다. 설계와 구현 코드를 확인하는 용도입니다.

---

## 기술 스택

| 영역 | 사용 기술 |
|---|---|
| 엔진 | Unreal Engine 5.7 (C++) |
| 네트워크 | Dedicated Server, Replication, RPC / 3인 파티 기준 |
| 전투 | Gameplay Ability System (GAS), GameplayTags, Gameplay Message Router |
| AI | StateTree, GameplayStateTree, NavigationSystem |
| UI | UMG, CommonUI |
| 백엔드 | Node.js, Express, MySQL (mysql2), Swagger |
| CI / 자동화 | Jenkins, Gauntlet 자동화 테스트, PowerShell 패키징 스크립트 |
| 테스트 | Gauntlet(E2E), Jest 기반 구조 검증 테스트 |

---

## 저장소 구조

```
Source/ProjectP/          게임 C++ 모듈 (502 파일)
├─ GAS/                   Gameplay Ability System - 어빌리티, 어트리뷰트, 이펙트
├─ Player/                플레이어 캐릭터, 입력, 상태
├─ Boss/                  보스 AI 및 페이즈 로직
├─ Enemy/                 일반 몬스터 AI
├─ Dungeon/               던전 게임모드, 세션/재접속 처리
├─ Lobby/                 로비, 파티 매칭, 던전 할당
├─ Widget/                UMG 위젯 (48 클래스)
├─ Zone/                  존 기반 스폰/트리거 시스템
├─ Streaming/             레벨 스트리밍 및 대사/연출 재생
├─ Indicator/             MOBA 스타일 스킬 인디케이터
├─ God / Shop / Item /    성장·재화·아이템 시스템
│  Messenger
├─ World/                 월드 구성 액터
├─ GameInstance/          서브시스템 (로그인, Gauntlet 자동화 등)
└─ Tests/                 언리얼 자동화 테스트

Plugins/                  자체 제작 플러그인 (Source 만 포함)
├─ CameraFocusFX          카메라 포커스 연출
├─ GraphicsCVarControl    그래픽 옵션 CVar 제어
├─ Laser                  레이저 이펙트
├─ SunZoneController      광원 존 제어
├─ TranslucentWall        반투명 벽 처리
└─ WhisperRuntime         음성 인식(Whisper) 런타임 연동

GameBackend/              Node.js + Express + MySQL 인증/세션/던전 관리 서버
Build/Scripts/Gauntlet/   Gauntlet E2E 자동화 테스트 (C#)
tests/                    Jest 기반 구조/정책 검증 테스트 (16개)
Config/                   프로젝트 설정 (ini)
Jenkinsfile               CI 파이프라인 정의
Run_*.ps1                 클라이언트/서버 패키징 및 Gauntlet 실행 스크립트
*_AllServers.bat          로컬 서버 일괄 기동/종료 스크립트
```

---

## 주요 구현 포인트

### 1. Dedicated Server 기반 3인 멀티플레이
모든 게임플레이 기능은 서버 권한(Server Authoritative)을 전제로 구현했습니다.
로비에서 파티를 구성하고, 던전 서버를 동적으로 할당받아 진입하는 구조입니다.

### 2. 던전 재접속 / 복구 처리
플레이어가 던전 진행 중 연결이 끊겨도 파티원 상태를 백엔드에 보존하고,
재접속 시 세션을 검증해 원래 상태로 복원합니다.

### 3. GAS 기반 전투 시스템
어빌리티, 어트리뷰트 세트, 상태이상(Status Effect)을 GAS 위에 구성했습니다.
상태이상 정의는 데이터 테이블로 분리해 밸런스 조정과 코드를 분리했습니다.

### 4. Jenkins + Gauntlet E2E 자동화
로그인 → 로비 → 파티 구성 → 던전 진입까지의 흐름을 Gauntlet 테스트로 자동 검증하고,
Jenkins 파이프라인에서 빌드·패키징·테스트를 수행합니다.

### 5. 음성 인식(Whisper) 연동
`WhisperRuntime` 플러그인으로 로컬 음성 인식을 게임 내 상호작용에 연결했습니다.

---

## 백엔드 API

`GameBackend`는 인증/세션/던전 관리 API를 제공합니다.

```
GET  /api/health                    헬스 체크
GET  /api/health/db                 DB 연결 확인
POST /api/login                     로그인
POST /api/logout                    로그아웃
POST /api/session/ping              세션 유지
POST /api/session/verify            세션 검증
POST /api/dungeon/session/verify    던전 세션 검증
POST /api/dungeon/member-state      던전 파티원 상태 저장
POST /api/dungeon/session-ended     던전 종료 처리
```

실행 설정은 `GameBackend/config.example.json`을 복사해 `config.json`으로 사용합니다.
(실제 설정 파일은 저장소에 포함하지 않습니다.)

---

## 저장소에서 제외한 항목

| 항목 | 이유 |
|---|---|
| `Content/` | 언리얼 에셋 약 7.1GB |
| `SourceAssets/`, `Data/` | 원본 리소스 및 게임 밸런스 데이터 |
| `AI_Docs/` | 내부 설계·구현 계획 문서 |
| `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` | 빌드 산출물 |
| `Plugins/*/Content`, `Plugins/*/Resources` | 플러그인 에셋 |
| `Plugins/KawaiiPhysics`, `VisualStudioTools`, `GameplayMessageRouter` | 외부 제작 플러그인 |
| `ThirdParty/MySQL` | 외부 라이브러리 바이너리 |
| `node_modules/` | 의존성 패키지 |
| `ServerRuntime.ini`, `GameBackend/config.json` | 실행 환경별 설정 (example 파일만 포함) |
