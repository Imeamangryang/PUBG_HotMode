# HP 0 → 사망 상태 및 애니메이션 구현 계획

## 📋 개요
HP가 0이 되었을 때 플레이어의 행동을 제한하고, 죽음 애니메이션을 재생하며 관전자 모드로 전환하는 시스템 구현.
추가로 크라우치, 엎드리기, 조준경 모션에 애니메이션을 연결하는 작업.

**현재 상태:**
- ✅ HP 시스템: `BG_HealthComponent`에서 네트워크 HP 감소 구현됨
- ✅ 사망 상태 플래그: `BG_Character`에 `bIsDead`, `CharacterState` 있음
- ✅ 애니메이션 기반: `ABP_BG_Player_WON` 존재, 몬타쥬 자산 풍부
- ❌ 죽음 애니메이션 몬타쥬: 없음 (생성 필요)
- ❌ 죽음 상태 로직: 행동 제한, 관전자 모드 로직 미구현
- ❌ 스탠스/조준 애니메이션: 크라우치, 엎드리기, ADS 애니메이션 ABP에 연결 필요

---

## 🎯 작업 분류

### **A. 코드 작업 (WON 폴더 내 C++)**

#### A1. `BG_Character.cpp` - 사망 상태 처리 로직
**목표:** HP 0 시 플레이어 행동 제한

**변경 사항:**
- `HandleHealthDeathStateChanged()` 함수 확장
  - `CharacterState`를 `EBGCharacterState::Dead`로 설정
  - 모든 입력 바인딩 비활성화 (Jump, Attack, Crouch, Prone, Lean, Aim)
  - 캐릭터 이동 제어 비활성화 (`DisableInput()` 또는 `bCanEverTick = false`)
  - 죽음 애니메이션 몬타쥬 재생 함수 호출
  
- 사망 애니메이션 재생 함수 추가
  ```cpp
  UFUNCTION(NetMulticast, Reliable)
  void Multicast_PlayDeathAnimation();
  ```

- Replication 체크
  - `bIsDead` 플래그가 이미 Replicated임 확인
  - 사망 상태 변경 시 모든 클라이언트에서 동기화되도록 구현

#### A2. `BG_PlayerController.cpp` - 관전자 모드 전환
**목표:** 사망 후 관전자 모드 활성화

**변경 사항:**
- 사망 감지 콜백 함수 추가
  ```cpp
  UFUNCTION()
  void OnCharacterDeath();
  ```
- 관전자 모드 전환
  - `FollowCamera` 활성화 (또는 FreeCam 모드)
  - 입력 비활성화 (이미 캐릭터에서 처리되지만 컨트롤러 레벨에서도 체크)
  - HUD 업데이트: "관전 중" 표시

#### A3. `BG_HealthComponent.cpp` - 사망 이벤트 보정
**현재 상태:** `OnDeathStateChanged` 델리게이트 이미 있음
**확인 사항:** 
- 델리게이트 바인딩 확인
- 필요시 추가 로그 또는 상태 동기화 로직 추가

---

### **B. 애니메이션 블루프린트 작업 (ABP_BG_Player_WON)**

#### B1. 사망 애니메이션 몬타쥬 생성
**자산 이름:** `MM_Player_Death` (또는 여러 변형)

**필요 자산:**
- 죽음 애니메이션 시퀀스 (Standing, Crouch, Prone 각각)
  - 예: `Death_Stand.uasset`, `Death_Crouch.uasset`, `Death_Prone.uasset`
  - 또는 기존 메타휴먼 데이터셋에서 가져오기

**구현:**
- 몬타쥬 생성 (ABP 내 또는 별도)
- ABP 스테이트머신에 `Death` 스테이트 추가
  - 입력: `bIsDead` 플래그
  - 출력: 죽음 애니메이션 재생 후 Idle 유지

#### B2. 크라우치 애니메이션 연결
**현재 상태:** 
- Crouch 폴더에 이미 `Retargeting_anim_Crouch_*.uasset` 있음
- `BS_BG_CourchMove_WON` 블렌드스페이스 있음

**ABP 작업:**
- ABP 스테이트머신에 `Crouch` 스테이트 추가/확인
  - 입력: `bIsCrouchingState` 플래그 + 이동 속도
  - 출력: Crouch Blendspace 재생 (Idle, Walk, Jog)
- 트랜지션 규칙:
  - `bIsCrouchingState && !bIsDead` → Crouch 진입
  - `!bIsCrouchingState || bIsDead` → Standing 복귀

#### B3. 엎드리기(Prone) 애니메이션 연결
**현재 상태:** 
- Prone 애니메이션은 크라우치 폴더에 `Retargeting_anim_Downed_Idle_R.uasset` 하나만 있음
- 엎드리기용 이동 애니메이션 부족

**ABP 작업:**
- ABP 스테이트머신에 `Prone` 스테이트 추가/확인
  - 입력: `bIsProne` 플래그
  - 출력: Prone Idle 재생 (이동 중 애니메이션은 추가 필요 시 TBD)
- 트랜지션 규칙:
  - `bIsProne && !bIsDead` → Prone 진입
  - `!bIsProne || bIsDead` → Standing 복귀

#### B4. 조준경(ADS) 애니메이션 연결
**현재 상태:**
- Rifle: `MF_Rifle_Idle_ADS.uasset`, `MM_Rifle_Idle_ADS_AO_**.uasset` 있음
- Pistol: `MF_Pistol_Idle_ADS.uasset`, `MM_Pistol_Idle_ADS.uasset` 있음

**ABP 작업:**
- ABP 스테이트머신에 `Aiming` 서브-스테이트 또는 레이어 추가
  - 입력: `bCanAim` + 우클릭(RMB) 상태
  - 출력: ADS Idle 모션 재생 (무기별로 다름)
- 무기별 블렌딩:
  - Rifle 소지 시: `MF_Rifle_Idle_ADS` 사용
  - Pistol 소지 시: `MF_Pistol_Idle_ADS` 사용
  - 무기 미소지 시: ADS 불가

---

### **C. 직접 수정 필요 항목 (사용자 체크리스트)**

#### **C1. 코드에서 몬타쥬 레퍼런스 추가**
**파일:** `BG_Character.h` / `BG_Character.cpp`

**변경 항목:**
```cpp
// BG_Character.h에 추가
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Death")
TObjectPtr<UAnimMontage> DeathMontage = nullptr;
```

**블루프린트 할당:**
- `BP_BG_Character_WON` 블루프린트를 열어서
- `Death` 카테고리 → `DeathMontage` 프로퍼티에 `MM_Player_Death` 할당

#### **C2. 크라우치/프론 모션 입력 바인딩 확인**
**파일:** `BG_PlayerController.cpp`

**확인 사항:**
- `OnCrouchInputStarted()` 함수: 실제로 `BG_Character::ToggleCrouchFromInput()` 호출하는지 확인
- `OnProneInputStarted()` 함수: 실제로 `BG_Character::ToggleProneFromInput()` 호출하는지 확인
- 둘 다 구현되었는지 확인

#### **C3. ABP 애니메이션 레이어 구성 확인**
**파일:** `ABP_BG_Player_WON` (블루프린트)

**필수 확인:**
- 스테이트머신에 있는 스테이트들:
  - ✅ Standing
  - ✅ Crouching (또는 Crouch) 
  - ✅ Prone
  - ❌ Dead (추가 필요)
  - ❌ Aiming/ADS (추가 또는 레이어 필요)

---

### **D. 주석 추가 위치 (중요 부분)**

#### D1. `BG_HealthComponent.cpp`
```cpp
// HP가 0이 되면 사망 상태로 설정
// 이 플래그 변경은 모든 클라이언트에 레플리케이트됨
```

#### D2. `BG_Character.cpp`
```cpp
// 사망 상태 전환: 플레이어의 모든 입력을 비활성화하고 
// 죽음 애니메이션 재생 후 관전자 모드로 전환
```

#### D3. `ABP_BG_Player_WON`
```
// Death 스테이트 트랜지션:
// bIsDead = true이면 즉시 Death 스테이트로 전환
// Death 애니메이션 완료 후 특정 포즈에서 유지
```

---

## 📊 구현 순서

1. **C++ 코드 변경** (A1, A2, A3)
   - `BG_Character::HandleHealthDeathStateChanged()` 함수 확장
   - 입력 제한 로직 추가
   - 네트워크 레플리케이션 확인

2. **사망 애니메이션 몬타쥬 생성 또는 찾기** (B1)
   - 기존 UE5 기본 애니메이션에서 Death 애니메이션 찾기
   - 몬타쥬로 래핑

3. **블루프린트 할당** (C1)
   - `BP_BG_Character_WON`에서 DeathMontage 할당

4. **ABP 스테이트머신 업데이트** (B2, B3, B4)
   - Dead, Crouch, Prone, Aiming 스테이트 추가/확인
   - 트랜지션 규칙 설정

5. **테스트**
   - 싱글플레이: HP 0 시 죽음 애니메이션 재생 및 입력 제한 확인
   - 멀티플레이: 다른 플레이어 사망 시 모든 클라이언트에서 동기화 확인
   - 크라우치, 프론, ADS 모션 재생 확인

---

## 🔍 문제 해결

| 문제 | 원인 | 해결책 |
|------|------|-------|
| 사망 후에도 플레이어가 움직임 | 입력 제한이 안 됨 | `DisableInput()` 추가 호출 |
| 죽음 애니메이션이 안 재생됨 | 몬타쥬가 할당 안 됨 | C1 체크리스트 확인 |
| 크라우치 모션이 이상함 | ABP 블렌스페이스 미연결 | ABP 스테이트머신 확인 |
| 다른 플레이어 사망이 보이지 않음 | 네트워크 레플리케이션 누락 | `Multicast_PlayDeathAnimation()` 함수 확인 |

---

## 📝 예상 파일 변경

| 파일 | 변경 | 라인 수 | 난이도 |
|------|------|--------|--------|
| `BG_Character.h` | +프로퍼티 2개 | ~5줄 | 🟢 쉬움 |
| `BG_Character.cpp` | HandleHealthDeathStateChanged 확장 + 새 함수 | ~30줄 | 🟡 중간 |
| `BG_PlayerController.cpp` | 사망 콜백 함수 추가 | ~10줄 | 🟢 쉬움 |
| `BG_HealthComponent.cpp` | 주석 추가 | ~3줄 | 🟢 쉬움 |
| `ABP_BG_Player_WON` | 스테이트 추가/수정 | 블루프린트 작업 | 🟡 중간 |

---

## ✅ 확인 체크리스트

- [ ] C코드 변경 완료
- [ ] 블루프린트 몬타쥬 할당 완료
- [ ] ABP 스테이트머신 업데이트 완료
- [ ] 싱글플레이 테스트 완료
- [ ] 멀티플레이 테스트 완료 (네트워크 동기화)
