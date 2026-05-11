# ABP_BG_Player_WON 상세 작업 가이드

## 📋 개요
`ABP_BG_Player_WON`에서 크라우치, 엎드리기, 조준경, 사망 애니메이션을 연결하는 작업입니다.
ABP는 Animation Blueprint의 약자로, 캐릭터 애니메이션 로직을 담당합니다.

---

## 🎯 작업 순서

### **1단계: ABP 열기**
- UE5 에디터에서 `ABP_BG_Player_WON` 더블클릭
- `Event Graph` 탭으로 이동

### **2단계: 변수 추가**
`My Blueprint` 패널에서 `Variables` 섹션에 다음 변수를 추가:

| 변수명 | 타입 | 설명 |
|--------|------|------|
| `bIsCrouchingState` | Boolean | 크라우치 상태 |
| `bIsProne` | Boolean | 엎드리기 상태 |
| `bIsAiming` | Boolean | 조준 상태 |
| `bIsDead` | Boolean | 사망 상태 |
| `EquippedWeaponPoseType` | EBGWeaponPoseType | 무기 타입 (Enum) |
| `bHasWeapon` | Boolean | 무기 소지 여부 |

### **3단계: Event Blueprint Update Animation 설정**
`Event Graph`에서 `Event Blueprint Update Animation` 노드를 찾거나 생성:

```
[Event Blueprint Update Animation] → [Try Get Pawn Owner] → [Cast To ABG_Character]
```

캐스트 성공 시 다음 노드 연결:

```
[Cast To ABG_Character] → [Get bIsCrouchingState] → [Set bIsCrouchingState (ABP 변수)]
[Cast To ABG_Character] → [Get bIsProne] → [Set bIsProne (ABP 변수)]
[Cast To ABG_Character] → [Get bIsAiming] → [Set bIsAiming (ABP 변수)]
[Cast To ABG_Character] → [Get bIsDead] → [Set bIsDead (ABP 변수)]
[Cast To ABG_Character] → [Get Equipped Weapon Pose Type] → [Set EquippedWeaponPoseType (ABP 변수)]
[Cast To ABG_Character] → [Get bHasWeapon] → [Set bHasWeapon (ABP 변수)]
```

### **4단계: AnimGraph 설정**
`AnimGraph` 탭으로 이동

#### **기본 구조 생성:**
```
[Final Animation Pose] ← [State Machine] ← [Locomotion State Machine]
```

### **5단계: Locomotion State Machine 생성**
1. `State Machine` 노드 생성 (이름: `Locomotion`)
2. 더블클릭하여 State Machine 에디터 열기

#### **스테이트 추가:**
- `Idle` (기본)
- `Crouch`
- `Prone`
- `Dead`

#### **트랜지션 규칙 설정:**

**Idle → Crouch:**
```
Rule: bIsCrouchingState == true && !bIsDead
Blend Time: 0.2
```

**Crouch → Idle:**
```
Rule: bIsCrouchingState == false || bIsDead
Blend Time: 0.2
```

**Idle → Prone:**
```
Rule: bIsProne == true && !bIsDead
Blend Time: 0.3
```

**Prone → Idle:**
```
Rule: bIsProne == false || bIsDead
Blend Time: 0.3
```

**Any State → Dead:**
```
Rule: bIsDead == true
Blend Time: 0.1
```

### **6단계: 각 스테이트에 애니메이션 연결**

#### **Idle 스테이트 (기본 이동):**
```
[Blendspace Player] → [BS_BG_Player_WON] (무기 미소지 시)
또는
[Blendspace Player] → [BS_BG_Armed_Player_WON] (무기 소지 시)
```

#### **Crouch 스테이트:**
```
[Blendspace Player] → [BS_BG_CourchMove_WON]
```

#### **Prone 스테이트:**
```
[Single Animation] → [Retargeting_anim_Downed_Idle_R]
```
*(이동 애니메이션이 없으므로 우선 Idle만)*

#### **Dead 스테이트:**
```
[Single Animation] → [Death Animation] (또는 DeathMontage 재생)
```

### **7단계: 조준경(ADS) 레이어 추가**
ADS는 상체만 적용하는 것이 좋으므로 `Layered Blend Per Bone` 사용:

#### **AnimGraph에 추가:**
```
[Locomotion State Machine] → [Layered Blend Per Bone] → [Final Animation Pose]
```

#### **Layered Blend Per Bone 설정:**
- `Layer Setup` → `Upper Body` 추가
- `Blend Poses` → Base: Locomotion, Layer: ADS Blend

#### **ADS Blend 생성:**
```
[Blend Poses by Bool] 
- A: [기본 상체]
- B: [ADS 애니메이션]
- Alpha: bIsAiming && bHasWeapon
```

#### **무기별 ADS 애니메이션:**
```
[Enum Switch] (EquippedWeaponPoseType)
- Pistol → [MF_Pistol_Idle_ADS]
- Rifle → [MF_Rifle_Idle_ADS]
- Default → [기본 상체]
```

### **8단계: 컴파일 및 테스트**
1. ABP 컴파일 (Ctrl + Alt + C)
2. 에디터 플레이 테스트
3. 크라우치(C), 엎드리기(X), 조준(RMB) 입력 시 애니메이션 확인

---

## 🔧 세부 설정

### **Blendspace 연결 팁:**
- `Blendspace Player` 노드 사용
- `X Axis`: Speed (이동 속도)
- `Y Axis`: Direction (이동 방향)

### **트랜지션 블렌딩:**
- 크라우치: 0.2초 (부드럽게)
- 엎드리기: 0.3초 (천천히)
- 사망: 0.1초 (즉시)

### **Layered Blend Per Bone:**
- `Layer Setup`에서 `Upper Body` 선택
- `Mask`는 기본 `UpperBody` 마스크 사용

---

## ⚠️ 주의사항

1. **변수 이름 정확히:** ABP 변수 이름이 C++과 정확히 일치해야 함
2. **Enum 타입:** `EBGWeaponPoseType` Enum이 ABP에서 인식되는지 확인
3. **컴파일 순서:** 먼저 C++ 컴파일 후 ABP 컴파일
4. **테스트:** 각 상태별로 플래그 값이 제대로 전달되는지 확인

---

## 🎯 예상 결과

- **C 키:** 크라우치 애니메이션 재생
- **X 키:** 엎드리기 애니메이션 재생  
- **RMB:** 조준경 모션 (상체 ADS)
- **HP 0:** 사망 애니메이션 재생 + 입력 제한

---

## 📝 추가 팁

- ABP에서 디버깅: `Print String` 노드로 변수 값 확인
- 애니메이션 미리보기: State Machine에서 각 스테이트 우클릭 → Preview
- 성능: 불필요한 트랜지션 제거

이 가이드를 따라 ABP를 설정하면 모든 애니메이션이 제대로 연결됩니다!</content>
<parameter name="filePath">c:\Users\user\Documents\Unreal Projects\PUBG_HotMode\Docs\ABP_BG_Player_WON_DetailGuide.md