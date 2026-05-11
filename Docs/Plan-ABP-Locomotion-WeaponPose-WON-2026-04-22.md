# WON 플레이어 ABP 이동/총기 자세 전환 계획

## 목적

- `ABP_BG_Player_WON`에서 비무장 이동과 총기 장착 이동을 자연스럽게 연결한다.
- 현재 `ABG_Character`가 가진 상태값을 기준으로 ABP가 안정적으로 자세를 선택할 수 있게 정리한다.
- 수정 권한 범위를 지키면서 작업 대상을 `Source/PUBG_HotMode/WON`과 `Content/WON/BluePrints/ABP_BG_Player_WON.uasset`로 한정한다.

## 작업 제약

- 코드 수정 가능 범위: `Source/PUBG_HotMode/WON`
- 문서 수정 가능 범위: `Docs`
- 에셋 수정 가능 범위: `Content/WON/BluePrints/ABP_BG_Player_WON.uasset`
- 그 외 공용 `Content` 에셋은 수정하지 않는다.

## 현재 코드 상태 정리

### 확인된 상태값

`ABG_Character`에는 ABP가 읽을 수 있는 기본 상태가 이미 일부 존재한다.

- `bIsWeaponEquipped`
- `bIsAiming`
- `EquippedWeaponPoseType`
- `bIsFalling`
- `bIsAcceleration`

### 현재 구조에서 바로 보이는 이슈

- `Tick`에서 `bHasWeapon`, `WeaponPoseType`, `EBGWeaponType`를 참조하는데, 실제 무장 상태의 기준은 `bIsWeaponEquipped`와 `EquippedWeaponPoseType`로 이미 나뉘어 있다.
- 즉, ABP 기준 상태와 캐릭터 내부 계산 기준이 이중화되어 있다.
- 이 상태로 ABP를 확장하면 총기 포즈 전환 조건이 불안정해질 가능성이 높다.

### 이미 유효한 방향

- 총기 장착 시 `bUseControllerRotationYaw = true`
- 비무장 시 `bOrientRotationToMovement = true`
- 즉, 캐릭터 회전 정책은 이미 "비무장 이동"과 "총기 들고 이동"을 분리할 준비가 되어 있다.

## 목표 애니메이션 구조

### 핵심 원칙

- 하체 locomotion은 가능한 공통으로 유지한다.
- 총기 차이는 상체 중심 overlay 또는 pose layer에서 처리한다.
- 조준은 별도 additive 계층으로 분리한다.

### 권장 ABP 구성

1. Base Locomotion State Machine
2. Weapon Pose Selection
3. Layered Blend Per Bone
4. Aim Offset 또는 상체 Additive
5. Final Output Pose

### Base Locomotion이 가져야 할 값

- `Speed`
- `Direction`
- `bIsFalling`
- 필요 시 `bIsAcceleration`

### Weapon Pose Layer가 가져야 할 값

- `bIsWeaponEquipped`
- `EquippedWeaponPoseType`
- `bIsAiming`

## 구현 방향

### 1. 캐릭터 상태 정리

`ABG_Character`에서 ABP가 읽는 기준 상태를 하나로 정리한다.

- `bHasWeapon`은 `bIsWeaponEquipped`와 중복되므로 제거 또는 내부 호환용으로만 유지
- `WeaponPoseType`과 `EBGWeaponType` 참조는 `EquippedWeaponPoseType` 기준으로 통일
- 이동/조준/무장 상태가 틱과 복제 시점 모두에서 일관되게 반영되도록 정리

### 2. ABP 연동용 값 확정

ABP가 최종적으로 읽어야 할 값은 아래로 고정한다.

- `Speed`
- `Direction`
- `bIsInAir`
- `bIsAccelerating`
- `bIsWeaponEquipped`
- `bIsAiming`
- `EquippedWeaponPoseType`

필요하면 이후 `UAnimInstance` 파생 클래스를 `WON` 폴더 안에 추가해 계산을 옮긴다.  
다만 1차 작업은 기존 `ABP_BG_Player_WON`에서 캐릭터 캐스팅으로 처리해도 된다.

### 3. ABP 그래프 수정

`ABP_BG_Player_WON`에서 아래 구조로 정리한다.

- 비무장 locomotion은 기존 이동 상태머신 유지
- 총기 장착 여부로 locomotion 자체를 완전히 복제하지 말고, 총기 자세는 상체 레이어에서 분기
- `EquippedWeaponPoseType` 기반으로 pistol/rifle/shotgun pose 선택
- `bIsAiming`이 true일 때는 조준용 additive 또는 aim offset 적용

### 4. 자연스러운 이동 전환 포인트

- 총기를 든 상태에서도 하체 이동은 속도 기반 블렌드 유지
- 상체는 총기 idle/walk/run 대응 pose를 준비해 locomotion 위에 덮는다
- 갑작스러운 꺾임을 줄이기 위해 blend time을 짧게라도 명시적으로 준다
- 장착 해제 시 aim 계층이 즉시 꺼지도록 `bIsAiming` reset 흐름 유지

## 세부 작업 순서

1. `ABG_Character`의 중복 상태와 잘못된 타입 참조를 정리한다.
2. ABP가 읽을 최종 변수 목록을 확정한다.
3. 필요하면 `BlueprintPure` getter를 추가해 ABP 접근 경로를 단순화한다.
4. `ABP_BG_Player_WON`의 현재 AnimGraph/EventGraph를 점검한다.
5. locomotion과 weapon upper-body layer를 분리한다.
6. `EquippedWeaponPoseType` 기준 분기를 enum 중심 구조로 정리한다.
7. aim pose가 weapon pose 위에 additive로 올라가도록 연결한다.
8. 비무장, 무장, 조준 상태 전환을 각각 플레이 테스트한다.

## 검증 항목

- 비무장 상태에서 기존 이동이 깨지지 않는가
- 총기 장착 시 캐릭터가 카메라 yaw 기준으로 자연스럽게 회전하는가
- 총기 장착 후 정지, 걷기, 뛰기 전환에서 상체가 튀지 않는가
- 조준 시작/해제 시 상체가 순간이동하듯 꺾이지 않는가
- 서버/클라 환경에서 무장 상태 복제가 ABP 자세와 어긋나지 않는가

## 예상 수정 파일

### 코드

- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`

### 에셋

- `Content/WON/BluePrints/ABP_BG_Player_WON.uasset`

## 이번 계획에서 제외

- 다른 공용 ABP 또는 공용 애니메이션 에셋 수정
- 신규 공용 BlendSpace/AimOffset 생성
- `WON` 외 사용자 폴더 코드 수정

## 구현 전 확인 사항

- `ABP_BG_Player_WON` 내부에 이미 사용 중인 locomotion state machine 이름과 변수 이름을 먼저 확인해야 한다.
- 총기별 전용 애니메이션 클립이 이미 연결되어 있는지 확인해야 한다.
- 기존 ABP가 공용 스켈레톤 애니메이션을 참조하더라도, 이번 작업은 지정된 `ABP_BG_Player_WON` 내부 수정만으로 끝내는 방향을 우선한다.
