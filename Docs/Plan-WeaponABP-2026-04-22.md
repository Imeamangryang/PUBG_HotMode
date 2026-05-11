# 기본 이동 + 무장 이동 ABP 설계 계획

## 목적

- 기본 이동 ABP와 무기 장착 후 이동 ABP를 수업용 예제를 그대로 복붙하지 않고, 추후 권총 / 라이플 / 샷건까지 확장 가능한 구조로 설계한다.
- 현재 프로젝트 구조 기준으로 C++ 책임을 `Character`, `PlayerController`, `Weapon Actor`, `AnimInstance` 중 어디에 둘지 정리한다.
- 이후 구현 단계에서 필요한 최소 C++ 변수와 애님 파라미터를 먼저 정의한다.

## 결론 요약

- 수업에서 배운 `HasPistol bool 하나로 ABP를 갈라타는 방식`은 학습용으로는 괜찮지만, 실전 확장성은 낮다.
- 현재 팀이 생각한 `무기 주변 콜리전 + 캐릭터 주변 콜리전 오버랩 후 입력으로 줍기` 방식 자체는 괜찮다.
- 다만 `HasPistol`, `HasRifle`, `HasShotgun` 같은 bool을 계속 늘리는 방식은 피하고, `현재 장착 무기 타입`을 나타내는 enum 기반으로 가는 것이 좋다.
- 입력 수집은 `PlayerController`, 실제 줍기/장착 상태와 서버 권한 처리는 `Character`, 월드에 놓인 무기 정보는 `Weapon Actor`, 애니메이션 계산은 `AnimInstance/ABP`가 담당하는 구조가 적절하다.

## 1. 수업 예제를 그대로 써도 되는가?

### 그대로 써도 되는 부분

- Locomotion State Machine의 기본 구조
- Speed, Direction, IsFalling 같은 기본 이동 파라미터
- `Layered Blend Per Bone`로 하체는 이동, 상체는 무기 자세를 덮는 방식
- Aim Offset 또는 Pitch 보정용 Control Rig / Transform Bone 사용

### 그대로 쓰면 아쉬운 부분

- `HasPistol` 하나로 분기하는 구조
- State Machine 안에 권총 전용 상태를 직접 박아 넣는 구조
- 무기 타입이 늘 때마다 bool과 transition rule이 폭증하는 구조

### 권장 방향

- `bool HasPistol` 대신 `EWeaponPoseType` 또는 `EEquippedWeaponType` enum 사용
- 예:
  - `None`
  - `Pistol`
  - `Rifle`
  - `Shotgun`
- ABP는 `현재 무기 타입`에 따라 상체 pose를 선택하고, 하체 locomotion은 최대한 공통으로 유지한다.

## 2. C++ 어디 파일에서 작업하는 게 좋은가?

### PlayerController에 둘 것

- 입력 바인딩
- 줍기 키 입력 수신
- 조준 입력 수신
- 발사 입력 수신

현재 프로젝트 기준 후보 파일:

- `Source/PUBG_HotMode/WON/Public/Player/BG_PlayerController.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_PlayerController.cpp`

이유:

- 지금도 Enhanced Input 바인딩 책임이 이미 `ABG_PlayerController`에 있다.
- 입력은 컨트롤러가 받고, 실제 게임 규칙 수행은 캐릭터에게 위임하는 패턴이 현재 코드와 맞다.

### Character에 둘 것

- 현재 줍기 가능한 무기 추적
- 장착/해제 상태
- 서버 RPC를 통한 실제 장착 처리
- 장착에 따라 이동 회전 정책 변경
- ABP가 읽을 무장 상태 변수 관리

현재 프로젝트 기준 후보 파일:

- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`

이유:

- 무기를 실제로 "소유"하고 "장착"하는 주체는 Pawn/Character다.
- 복제(Replication), 장착 소켓 부착, 캐릭터 회전 정책 변경, 전투 상태 전환은 캐릭터에 있는 편이 자연스럽다.
- 애님 블루프린트도 보통 소유 캐릭터의 상태를 읽는다.

### Weapon Actor 또는 Weapon Pickup Actor에 둘 것

- 무기 종류 데이터
- 장착 시 어떤 자세를 써야 하는지
- 픽업 가능 범위 콜리전
- 캐릭터가 접근했을 때 상호작용 대상이 되는 정보

권장 신규 클래스 예시:

- `ABG_WeaponBase`
- `ABG_WeaponPickup`

최소 필요 책임:

- `WeaponType`
- `WeaponPoseType`
- `PickupCollision`
- `SkeletalMesh or StaticMesh`

### AnimInstance / ABP에 둘 것

- Speed
- Direction
- bIsInAir
- bIsAiming
- WeaponPoseType
- bIsWeaponEquipped

중요:

- ABP는 상태를 "계산해서 결정"하기보다, 캐릭터가 이미 결정한 상태를 받아서 pose를 선택하는 쪽이 유지보수에 유리하다.

## 3. 캐릭터 파일인가, 컨트롤러 파일인가?

### 최종 권장 답

- `키 입력 감지`: `PlayerController`
- `줍기 가능 대상 판정 결과 보관`: `Character`
- `실제 무기 장착 / 해제`: `Character`
- `무기 액터 자체 데이터`: `Weapon Actor`

즉, 질문에 한 줄로 답하면:

- **입력은 Controller, 상태와 장착은 Character**

## 4. 팀이 계획한 오버랩 방식은 괜찮은가?

### 가능하다

- 월드 무기마다 `PickupCollision`
- 캐릭터에도 `InteractionCollision`
- 서로 오버랩되면 줍기 가능 상태 진입

이 방식은 직관적이고 구현도 쉽다.

### 하지만 권장 수정점

- 충돌체를 양쪽 다 크게 둘 필요는 없다.
- 보통은 무기 쪽에만 픽업 콜리전을 두고, 캐릭터는 Capsule 또는 작은 상호작용 감지 컴포넌트 하나만 둔다.
- 오버랩 시 `현재 후보 무기`를 한 개만 쓰지 말고, 후보 목록을 관리한 뒤 가장 가까운 무기를 선택하는 방식이 더 안전하다.

권장 이유:

- 좁은 공간에서 여러 무기가 겹치면 마지막으로 오버랩된 무기만 기억하는 구조는 불안정하다.
- 후보 배열 또는 set을 두고 거리 기준으로 `BestInteractableWeapon`을 고르는 쪽이 확장성 있다.

## 5. C++에서 먼저 만들어 둘 변수 방향

### Character 쪽 권장 변수

- `bool bIsWeaponEquipped`
- `bool bIsAiming`
- `EEquippedWeaponType EquippedWeaponType`
- `ABG_WeaponBase* EquippedWeapon`
- `TArray<ABG_WeaponBase*> OverlappingWeapons`
- `ABG_WeaponBase* CurrentInteractableWeapon`

### 추천 enum

```cpp
UENUM(BlueprintType)
enum class EEquippedWeaponType : uint8
{
    None,
    Pistol,
    Rifle,
    Shotgun
};
```

또는 pose용으로 분리:

```cpp
UENUM(BlueprintType)
enum class EWeaponPoseType : uint8
{
    Unarmed,
    OneHandedPistol,
    TwoHandedRifle,
    TwoHandedShotgun
};
```

실무적으로는 `무기 타입`과 `애님 pose 타입`을 분리하는 편이 더 좋다.

이유:

- 라이플 2종이 있어도 pose는 같을 수 있다.
- 샷건도 펌프액션/더블배럴/전술샷건이 있어도 이동 pose는 동일 계열일 수 있다.

## 6. 애니메이션 그래프는 어떻게 작업하는가?

### 추천 구조

- 하체 locomotion은 공통 상태머신 1개 유지
- 상체/전신 무장 자세는 무기 타입별 pose 선택 구조 사용
- 최종 출력에서 `Layered Blend Per Bone`으로 합성

### 권장 Anim Graph 흐름

1. `BaseLocomotion` 캐시
2. `WeaponUpperBodyPose` 캐시
3. `Layered Blend Per Bone`
4. 필요 시 Aim Offset / Control Rig 추가
5. Output Pose

예시 흐름:

- `State Machine (Idle/Walk/Run/Jump)` -> `Save Cached Pose: BaseLocomotion`
- `Blend Poses by Enum (WeaponPoseType)` 또는 동등한 구조 -> `Save Cached Pose: WeaponPose`
- `Layered Blend Per Bone`
  - Base Pose = `BaseLocomotion`
  - Blend Pose = `WeaponPose`
  - Branch Bone = spine_01 또는 spine_02
- 이후 `Aim Offset` 또는 상체 pitch 보정

### 왜 State Machine을 무기별로 크게 쪼개지 않는가?

- `Unarmed Locomotion`, `Pistol Locomotion`, `Rifle Locomotion`, `Shotgun Locomotion`을 각각 별도 상태머신으로 크게 나누면 중복이 많아진다.
- 걷기/달리기/점프 전환 규칙은 대부분 공통이라, 하체 상태머신은 공통으로 유지하고 무기 차이는 상체 pose 레이어에서 처리하는 편이 낫다.

### 언제 별도 상태머신이 필요한가?

- 권총과 라이플의 전신 실루엣 차이가 매우 커서 하체 stride 자체가 달라질 때
- 조준 이동, 앉기 이동, 엎드리기 이동까지 무기별로 전부 다를 때
- 시작/정지(turn in place, start/stop) 애니메이션이 무기별로 크게 다를 때

초기 단계에서는 아래 방식이 가장 적절하다:

- **공통 locomotion + 무기별 상체 overlay**

## 7. 현재 첨부한 그래프 구조에 대한 판단

### 좋은 점

- Cached Pose를 사용하고 있다.
- `Layered Blend Per Bone`를 사용하고 있다.
- 무기 자세와 일반 이동을 분리하려는 방향은 맞다.

### 아쉬운 점

- `HasPistol` bool 중심 구조라 확장 시 분기 폭증 가능성이 높다.
- `PistolMove`처럼 특정 무기 이름이 상태 이름에 직접 들어가면 라이플/샷건 추가 때 구조가 금방 커진다.
- `Any State -> PistolMove` 류 전환은 디버깅이 점점 어려워질 수 있다.

### 수정 권장안

- `HasPistol` -> `WeaponPoseType != Unarmed`
- `PistolMove` -> `ArmedOverlay` 또는 `WeaponOverlay`
- 권총/라이플/샷건 차이는 `Blend Poses by Enum` 또는 별도 linked layer로 선택

## 8. 회전 설정도 함께 고려해야 한다

총을 든 이동에서 자연스럽게 보이려면 애니메이션만 바꾸면 끝나지 않는다.

### 비무장 이동

- `bOrientRotationToMovement = true`
- `bUseControllerRotationYaw = false`

### 총 장착 이동

- 보통은 아래 쪽이 더 자연스럽다.
- `bOrientRotationToMovement = false`
- `bUseControllerRotationYaw = true`

즉, 장착 시 캐릭터 회전 정책도 `Character`에서 함께 바꿔야 한다.

이 부분은 ABP보다 체감이 더 크다.

## 9. 구현 순서 계획

1. `WeaponType / WeaponPoseType` enum 정의
2. 무기 pickup actor 또는 weapon base actor 정의
3. `Character`에 현재 겹친 무기 목록, 현재 상호작용 대상, 장착 상태 변수 추가
4. `PlayerController`에 Pickup 입력 바인딩 추가
5. `Character`에서 서버 RPC 기반 장착 처리 구현
6. 장착 시 소켓 부착 + 회전 정책 전환
7. `AnimInstance` 또는 ABP에서 캐릭터 상태 읽기
8. ABP를 `HasPistol` 기반이 아닌 `WeaponPoseType` 기반으로 재구성
9. 마지막으로 Aim Offset, 조준 상태, 발사 반동 몽타주를 얹기

## 10. 이번 단계에서 구현하지 말고 먼저 확정해야 할 것

- 무기 클래스를 액터로 둘지, 데이터 에셋 중심으로 둘지
- 현재 단계에서 `권총만 우선`으로 갈지, 처음부터 enum 구조까지 열어둘지
- 장착 후 회전 방식을 PUBG처럼 카메라 기준 회전으로 할지
- ABP를 하나로 통합할지, 무기군별 linked anim layer를 쓸지

## 추천 최종안

현재 팀 상황에서는 다음 방식이 가장 안정적이다.

- 입력은 `BG_PlayerController`
- 장착/상태/복제는 `BG_Character`
- 무기 데이터는 `Weapon Actor`
- ABP는 `공통 locomotion + 무기별 상체 overlay`
- 분기 기준은 `HasPistol`이 아니라 `WeaponPoseType enum`

즉, 수업 예제를 "개념 참고용"으로만 쓰고, 구조는 enum 기반으로 한 단계 일반화해서 들어가는 것이 맞다.
