# 🎮 CatchMeIfYouCan

> "경찰은 추격하고, 도둑은 살아남는다."
>
> 6인 3인칭 멀티플레이 PVP 추격 액션. Unreal Engine 5.6 기반 C++ · GAS · Listen Server · EOS로 구현했습니다.

---

## 📋 1. 프로젝트 개요 (Overview)

* **프로젝트명:** CatchMeIfYouCan
* **유형:** 6인 비대칭 멀티플레이 추격 액션 (경찰 vs 도둑)
* **개발 인원:** 6인 (클라이언트 4 · 레벨 디자인 1 · UI 1)
* **개발 기간:** 2025.09.15. ~ 2025.10.22. (약 5주)
* **본인 역할:** 인게임 모드 / 캐릭터 클래스 / GAS 상호작용 / 사다리 등반 / 클라이언트 초기화 파이프라인
* **핵심 컨셉:**
    * **경찰**: 도둑 추격 → 근접 공격 → 스턴 → 체포 → 감옥 호송
    * **도둑**: 금고 털이 또는 제한 시간 안에 생존 (감옥에서 탈출 가능)
    * **승리 조건**: 도둑 전원 체포 시 경찰 승 / 모든 금고 탈취 시 도둑 승 / 시간 만료 시 체포·탈취 수 기반 판정

---

## 🎥 2. 시연 영상 (Demo Video)

> *아래 링크를 클릭하면 유튜브에서 고화질로 시청할 수 있습니다.*

| 구분 | 링크 |
|:---:|:---|
| 🎮 **플레이 영상** | [https://youtu.be/VKtYdtn0VPA](https://youtu.be/VKtYdtn0VPA) |
| ✨ **하이라이트 영상** | [https://youtu.be/3Zr-4vPcAFU](https://youtu.be/3Zr-4vPcAFU) |
| 🎬 **클립 영상** | [https://youtu.be/gibfNhD6XUU](https://youtu.be/gibfNhD6XUU) |

[![플레이 영상](https://img.youtube.com/vi/VKtYdtn0VPA/0.jpg)](https://youtu.be/VKtYdtn0VPA)
[![하이라이트 영상](https://img.youtube.com/vi/3Zr-4vPcAFU/0.jpg)](https://youtu.be/3Zr-4vPcAFU)
[![클립 영상](https://img.youtube.com/vi/gibfNhD6XUU/0.jpg)](https://youtu.be/gibfNhD6XUU)

---

## 🛠️ 3. 사용 기술 (Tech Stack)

### Engine & Language
* **Unreal Engine 5.6.1** : Core Engine
* **C++** : 게임 로직, 커스텀 컴포넌트, 네트워크 예측 파이프라인
* **Blueprint** : DataAsset, UI, 애니메이션 그래프

### Framework & System
* **Gameplay Ability System (GAS)** : 어빌리티/어트리뷰트/이펙트 기반 액션 일원화
* **Enhanced Input** : InputAction · InputMappingContext 기반 입력 매핑
* **Motion Warping** : 사다리 등반 보간
* **AI Perception · Behavior Tree** : 정찰견 시야/소리 감지 및 스플라인 순찰

### Networking
* **Listen Server** : 호스트 기반 6인 P2P 매치
* **Epic Online Subsystem (EOS)** : 로그인 · 세션 생성/검색/조인
* **Custom Network Prediction** : `FSavedMove` / `FCharacterNetworkMoveData` 확장

---

## 💡 4. 담당 구현 (Features)

### 4-1. 인게임 모드 / 스테이트 설계
GameMode에서 PawnData를 비동기로 로드하고, 로드 완료 전에 접속한 플레이어는 `PendingPlayers` 큐에 보관했다가 일괄 처리합니다. 팀 배정은 각 팀 최소 필요 인원(경찰 1 / 도둑 2)을 먼저 채운 뒤 **경찰:도둑 = 1:2 비율 목표**로 자동 분배하며, 비율 편차에 따라 가중치를 조정해 우연한 쏠림을 보정합니다. GameState는 `Lobby → WaitingToStart → Preparing → InProgress → CopsWin / RobbersWin → Ending` 페이즈를 서버 시간 기반 타이머로 관리하고, 승리 판정은 시간 만료뿐 아니라 **도둑 전멸 / 금고 전부 탈취** 시 즉시 종결되도록 구성했습니다.

### 4-2. 캐릭터 클래스 계층 구조
`CharacterBase → PlayerCharacter → Cop / Robber` 분기 구조로 설계하고, `PawnData` DataAsset으로 캐릭터별 AbilitySet · AttributeSet · 입력을 데이터 기반 방식으로 부여합니다. 직업/역할 추가 시 코드 변경을 최소화하고 데이터 에셋 추가만으로 확장할 수 있는 구조를 목표로 했습니다.

### 4-3. 상호작용 시스템 (GAS 기반)
구체 스캔(`InteractionScanRange = 500.f`)과 레이캐스트(`InteractionTraceRange = 150.f`) 이중 감지 구조로 후보를 필터링한 뒤, 대상 카테고리에 맞는 자식 GA(`GA_Interact_Door / Safe / Arrest / Ladder / Info / Object / Active`)를 트리거합니다. `FCYInteractionInfo.Duration` 값에 따라 **즉시(원샷, Duration ≤ 0) / 홀딩(지속 입력과 타이머, Duration > 0)** 분기를 처리하며, 진행률 UI와 동기화됩니다.

### 4-4. 사다리 타기 시스템
CharacterMovementComponent를 확장해 커스텀 이동 모드 `CMOVE_Climbing`과 Phase 상태 머신(`None → Entering → Climbing → ExitingTop / ExitingBottom`)을 구성했습니다. `FSavedMove_CY` · `FCYCharacterNetworkMoveData`를 확장해 등반 시작 요청 / 진행 상태 / 부착 지점 등을 네트워크 예측 파이프라인에 포함시켰고, MotionWarping으로 진입·상단 탈출 구간의 위치 보간을 처리했습니다.

### 4-5. PlayerController / PlayerState
클라이언트 초기화 파이프라인에서 PlayerState · Pawn · AbilitySystemComponent · GameState · HUD의 유효성을 검증합니다. `ReceivedPlayer`, `OnRep_PlayerState`, `OnRep_Pawn`, 타이머 재시도 등 다중 진입점에서 체크합니다. 또한 RTT 기반 서버 시간 동기화(`ServerRequestServerTime` ↔ `ClientReportServerTime`, `SingleTripTime = 0.5 × RTT`)로 매치 타이머와 페이즈 전환을 클라이언트에 일관되게 표시합니다.

---

## 🧩 5. 주요 구현 내용 (요약)

| 기능 | 요약 |
|------|------|
| **팀 분배** | 최소 필요 인원(경찰 1 / 도둑 2) 충족 후 1:2 비율 기반 자동 배정 |
| **매치 진행** | Preparing 카운트다운 → InProgress → 도둑 전멸 / 금고 전부 탈취 / 시간 만료 시 종결 |
| **전투 흐름** | 근접 공격(AnimNotify 이벤트) → 스턴(이동 제한) → 체포(감옥 텔레포트) |
| **아이템** | 인벤토리(무기 3 / 아이템 6 슬롯), 무기 1 / 트랩 5 / 소비품 10 스택 |
| **감옥/탈옥** | JailPoint 볼륨 이탈 시 Jail GE 제거 및 생존 도둑 카운트 복구 |
| **사다리** | 진입·상단·하단 Phase 분기, MotionWarping 보간, CMC 예측 파이프라인 |

---

## 👥 6. 팀원 역할 분배

| 이름 | 담당 |
|------|------|
| 김민영 | 인게임 모드 로직, 캐릭터 클래스, GAS 상호작용 시스템 설계, 사다리 등반(커스텀 이동 모드 · 네트워크 예측), 클라이언트 초기화 파이프라인 |
| 홍승조 | 아이템, 공격 어빌리티 설계 |
| 안지호 | AI 정찰견 |
| 고건희 | EOS 로그인 연동 |
| 곽준상 | 맵 제작 및 레벨 디자인 |
| 임동휘 | 인게임 UI 위젯 설계 |

---

## 📚 7. 향후 확장 아이디어 (Future Plan)

* **직업군 확장** : PawnData / AbilitySet 데이터 자산 추가만으로 신규 경찰·도둑 직업 도입
* **전술 요소 추가** : 특수 무기, 감속 장판, 디코이 등 GAS 기반 신규 어빌리티 확장
* **호스트 마이그레이션** : Listen Server 호스트 이탈 대응 (또는 Dedicated Server 전환)
