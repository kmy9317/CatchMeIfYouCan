# 🎮 CatchMeIfYouCan

> 경찰과 도둑 컨셉의 6인 비대칭 멀티플레이 추격 액션 프로토타입

---

## 프로젝트 개요

| 항목 | 내용 |
|------|------|
| **엔진** | Unreal Engine 5.6.1 (C++) |
| **네트워크** | Listen Server |
| **인원** | 6인 (클라이언트 개발 4 · 레벨 디자인 1 · UI 1) |
| **기간** | 2025.00.00 ~ 2025.00.00 |
| **온라인** | EOS (Epic Online Services) |

경찰은 도둑을 추격·체포하고, 도둑은 금고를 털며 생존하는 멀티플레이 게임입니다.  
핵심 게임플레이 루프(추격 → 근접 공격 → 체포)와 리슨 서버 기반 클라이언트-서버 동기화를 검증

---

## 시연 영상

| 구분 | 링크 |
|:---:|:---|
| 🎮 **플레이 영상** | [https://youtu.be/VKtYdtn0VPA](https://youtu.be/VKtYdtn0VPA) |
| ✨ **하이라이트 영상** | [https://youtu.be/3Zr-4vPcAFU](https://youtu.be/3Zr-4vPcAFU) |
| 🎬 **클립 영상** | [https://youtu.be/gibfNhD6XUU](https://youtu.be/gibfNhD6XUU) |

[![플레이 영상](https://img.youtube.com/vi/VKtYdtn0VPA/0.jpg)](https://youtu.be/VKtYdtn0VPA)
[![하이라이트 영상](https://img.youtube.com/vi/3Zr-4vPcAFU/0.jpg)](https://youtu.be/3Zr-4vPcAFU)
[![클립 영상](https://img.youtube.com/vi/gibfNhD6XUU/0.jpg)](https://youtu.be/gibfNhD6XUU)

---

## 담당 구현 (김민영)

**인게임 모드 / 스테이트 설계**  
- GameMode에서 PawnData 비동기 로드 후 팀 비율 기반 자동 분배, GameState에서 페이즈 머신과 승리 조건 판정을 관리

**캐릭터 클래스 계층 구조**  
- `CharacterBase → PlayerCharacter → Cop/Robber` 분기 구조, PawnData 기반 동적 능력 배정 및 Seamless Travel 시 상태 보존

**상호작용 시스템 (GAS 기반)**  
- 구체 스캔 + 레이캐스트 이중 감지 구조, 홀딩/즉시 분기 처리, 문/금고/체포/사다리 타입별 GA 계층 구성

**사다리 타기 시스템**  
- CMC 확장으로 `CMOVE_Climbing` 커스텀 이동 모드와 Phase 상태 머신 구현
- `FSavedMove_CY`, `FCYCharacterNetworkMoveData` 확장으로 등반 시작/진행 상태를 예측 파이프라인에 포함

**PlayerController / PlayerState**  
- 클라이언트 초기화 파이프라인(PS·Pawn·ASC·GameState·HUD 유효성 재시도), RTT 기반 서버 시간 동기화

---

## 주요 구현 내용

| 기능 | 요약 |
|------|------|
| **팀 분배** | Required 최소 인원 보장 후 비율 기반 랜덤 배정 |
| **매치 진행** | Preparing 카운트다운 → InProgress → 시간 만료 시 체포 수 기반 승패 판정 |
| **전투 흐름** | 근접 공격(AnimNotify 이벤트) → 스턴(이동 제한 + 회복) → 체포(감옥 텔레포트) |
| **아이템** | 인벤토리(무기 3 + 아이템 6 슬롯), 스택킹, 트랩 설치, 소비품(힐/스피드/투명화) |
| **감옥/탈옥** | JailPoint 볼륨 이탈 시 Jail GE 제거 및 생존 카운트 복구 |
| **사다리** | 진입 몽타주/보간 분기, MotionWarping 상단 탈출, CMC 예측 파이프라인 |

---

## 사용 기술

`Unreal Engine 5.6.1` · `C++` · `Gameplay Ability System` · `Listen Server` · `Epic Online Services` · `Enhanced Input` · `Motion Warping` · `AI Perception` · `Behavior Tree`

---

## 팀원 역할 분배

| 이름 | 담당 |
|------|------|
| **김민영** | 인게임 모드 로직, 캐릭터 클래스, 어빌리티 시스템 설계 |
| 홍승조 | 아이템, 공격 어빌리티 설계 |
| 안지호 | AI 정찰견 |
| 고건희 | EOS 로그인 연동 |
| 곽준상 | 맵 제작 및 레벨 디자인 |
| 임동휘 | 인게임 UI 위젯 설계 |

---

## 향후 확장 아이디어

- 경찰/도둑 직업군 추가 (고유 능력 차별화)
- 특수 무기, 감속 장판 등 전술 요소 확장
