# 🎮 Hot Drop Survival Mode - TRD

## 1. 🎯 Objective

본 프로젝트는 **대규모 동시 접속 환경에서의 전투 및 서버 구조 검증**을 목표로 한다.

- 50명 이상의 플레이어가
- 좁은 지역에 동시에 착지 후
- **단시간 내에 1인 생존 시 종료되는 서바이벌 모드**

### 초기 구현 범위

- 이동 시스템
- 근접 전투 (주먹)
- 생존 판정

---

## 2. 🧱 System Overview

### 핵심 구조

- Dedicated Server 기반
- 일부 실제 플레이어 + 다수 Dummy Client
- Server Authoritative 방식

### 시스템 구성

| 시스템 | 설명 |
|------|------|
| Player System | 이동, 상태, 체력 |
| Combat System | 주먹 공격, 데미지 |
| Match System | 시작, 종료, 생존 판정 |
| Dummy System | 부하 테스트용 AI |
| Replication | 상태 동기화 |

---

## 3.🧍Player System

### 기능

- 이동 (WASD)
- 체력 (HP)
- 사망 처리


## 4. 👊 Combat System (Melee)

### 규칙

- 공격 방식: 주먹
- 공격 범위: 근거리 (Sphere Trace 또는 Capsule Trace)
- 공격 쿨타임 존재
- 데미지: 고정값

### 처리 흐름

Client Input  
→ Server RPC (Attack)  
→ Server Hit Detection  
→ Apply Damage  
→ Replication (HP Sync)

### 설계 원칙

- ❌ 클라이언트에서 Hit 판정 금지
- ✅ 서버에서만 판정 (Server Authoritative)

---

## 5. ⚔️ Match System

### 흐름

Waiting → Start → Combat → End

### 시작 조건

- 최소 인원 도달 (예: 50명)
- 테스트용 강제 시작 가능

### 종료 조건

```cpp
if (AlivePlayerCount <= 1)
{
    EndMatch();
}
```

# 6. 역할 분담

| 역할            | 담당자 |
|---------------|-----|
| Player System | 정창원 |
| Combat System | 정창원 |
| Server        | 신준섭 |
| Dummy Client  | 신준섭 |
| Match System  | 신준섭 |
| Inventory     | 김건우 |
| UI            | 김건우 |
| RDBMS 연동      | 김건우 |