# Stance / Fire Regression Fix Plan

## 목표

- 최근 자세 전환 수정 이후 발생한 `크라우치 불가`, `무기 장착 시 비정상 이동`, `카메라 흔들림`, `라이플 연발 중도 중단` 문제를 원인별로 분리해 안정적으로 해결한다.
- 서버/클라이언트의 자세 상태를 일치시키되, 기존 언리얼 이동 입력과 발사 입력 흐름을 깨지 않도록 복구한다.
- 점프, 엎드리기, 크라우치, 연발, 반동이 서로 간섭하지 않도록 책임 경계를 다시 정리한다.

## 현재 관찰된 증상

- 무기를 들지 않은 상태:
  - 엎드리기는 동작한다.
  - 크라우치만 막혀 있다.
- 무기를 든 상태:
  - 방향키와 무관하게 왼쪽으로 이동하는 것처럼 보인다.
  - 화면이 과하게 흔들린다.
  - 가만히 서서 라이플 연발 시 2~5발 사이에서 자주 끊긴다.
  - 이동 중에는 조금 더 발사될 때가 있으나 30발 연속 발사는 재현되지 않았다.

## 1차 원인 가설

### 1. 크라우치 회귀

- 최근 수정에서 `ToggleCrouchFromInput()`가 로컬 `Crouch()/UnCrouch()` 대신 서버 RPC `Server_SetCharacterStanceState()`로 우회되도록 바뀌었다.
- 이 경로는 언리얼 기본 `Crouch()`의 로컬 예측 흐름과 다르기 때문에, 입력 직후 로컬 `bIsCrouched`와 서버 동기화 타이밍이 꼬였을 가능성이 있다.
- 특히 `ApplyCharacterStanceState()` 안에서 `Crouch()/UnCrouch()` 후 즉시 `RefreshMovementState()`와 `UpdateDerivedState()`를 다시 호출하고 있어, 자세 계산이 한 프레임 안에서 중복 갱신된다.

### 2. 무기 장착 시 한 방향 이동 / 카메라 흔들림

- 무기 장착 상태에서만 심해진다면 `ApplyWeaponMovementState()` 또는 반동 적용 경로가 원인일 가능성이 높다.
- `Client_ApplyRecoil()`는 연발 중 매 탄마다 `SetControlRotation()`을 직접 갱신한다.
- 만약 ABP 또는 이동 방향 계산이 `ControlRotation`에 강하게 의존하고 있고, 여기에 과도한 반동이 누적되면 입력 방향과 체감 이동 방향이 어긋나 보일 수 있다.
- 카메라 흔들림이 "화면이 마구 흔들리는" 수준이라면, 단순 CameraShake보다 `Pitch/Yaw`를 매 탄 누적하는 코드가 더 강한 원인일 가능성이 높다.

### 3. 라이플 연발 중도 중단

- 코드상 연발은 `RequestStartFire()` -> `Server_StartFire()` -> `StartAutomaticFire()` -> 타이머 `HandleAutomaticFire()` 흐름으로 동작한다.
- 중간에 멈추는 조건은 사실상 아래 셋 중 하나다.
  1. `bIsHoldingFireInput`가 false가 된다.
  2. `CanFireWeapon()`가 false가 된다.
  3. `ExecuteFire()`가 false를 반환한다.
- `CanFireWeapon()`는 캐릭터 상태와 무기 상태를 함께 본다.
- `ExecuteFire()`는 `CurrentTime - LastFireTime < FireCooldown`, `CachedCharacter->CanFire()`, `ConsumeLoadedAmmo()` 중 하나라도 실패하면 바로 끊긴다.
- 따라서 발사 중 `CharacterState`, `bCanFire`, `bHasWeapon`, `EquippedWeapon`, `ActiveWeaponSlot`, `LoadedAmmo` 중 하나가 순간적으로 흔들리는지 로그로 먼저 확인해야 한다.

## 코드 기준 우선 점검 포인트

- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`
  - `ToggleCrouchFromInput()`
  - `ApplyCharacterStanceState()`
  - `UpdateDerivedState()`
  - `ApplyWeaponMovementState()`
  - `Client_ApplyRecoil()`
- `Source/PUBG_HotMode/WON/Private/Combat/BG_WeaponFireComponent.cpp`
  - `RequestStartFire()`
  - `StartAutomaticFire()`
  - `HandleAutomaticFire()`
  - `CanFireWeapon()`
  - `ExecuteFire()`
- 에디터 자산
  - `Content/GEONU/Data/DT_WeaponFireSpecs.uasset`
  - `Content/WON/Input/IA_BG_Attack.uasset`
  - 사용 중인 캐릭터 ABP / BP_Character

## 수정 방향

### A. 크라우치 복구

1. 최근 추가한 `Server_SetCharacterStanceState()` 기반 크라우치 우회 경로를 재검토한다.
2. 가능하면 크라우치는 다시 언리얼 기본 `Crouch()/UnCrouch()` 흐름을 우선 사용하고, `Prone -> Crouch` 전환에서만 서버 보정 로직을 최소 범위로 적용한다.
3. `ApplyCharacterStanceState()`는 `Prone` 전환 전용으로 축소하거나, `Standing/Crouching` 처리에서 중복 갱신을 줄인다.

### B. 무기 장착 시 비정상 이동 / 화면 흔들림 분리

1. 반동 적용량과 카메라 셰이크를 잠시 분리해서 본다.
2. `Client_ApplyRecoil()`에서 회전값을 직접 더하는 경로가 문제인지 확인하기 위해:
   - `Pitch/Yaw` 직접 누적만 끈 경우
   - CameraShake만 끈 경우
   - 둘 다 켠 경우
   를 나눠 재현한다.
3. 이동 방향 왜곡이 반동과 연결된다면, 연발 중에는 `ControlRotation` 보정량을 줄이거나 상한을 둔다.

### C. 연발 끊김 원인 고정

1. `HandleAutomaticFire()`와 `ExecuteFire()` 실패 사유를 구체 로그로 남긴다.
2. 발사 중단 시점에 다음 값을 찍는다.
   - `bIsHoldingFireInput`
   - `bCanFire`
   - `CharacterState`
   - `bHasWeapon`
   - `CurrentMagazineAmmo`
   - `LoadedAmmo`
   - `ActiveWeaponSlot`
   - `FireCooldown`
   - `CurrentTime - LastFireTime`
3. ABP 또는 BP_Character가 무기 상태를 다시 건드리는지 확인한다.
4. 만약 발사 상태가 애니메이션 이벤트나 BP에서 다시 리셋된다면, 발사 가능 여부는 코드 권한 상태만 기준으로 통합한다.

## 구현 순서

1. 크라우치 회귀를 먼저 복구한다.
2. 무기 장착 상태 이동/화면 흔들림을 반동 경로 중심으로 분리 재현한다.
3. 연발 중단 원인 로그를 추가한다.
4. 로그 결과에 따라 `CanFireWeapon()` 또는 `ExecuteFire()`의 중단 조건을 보정한다.
5. 최종적으로 Dedicated Server 멀티플레이 PIE에서 다음 조합을 모두 확인한다.
   - 맨손 스탠드 / 크라우치 / 엎드리기
   - 무기 장착 스탠드 / 크라우치 / 엎드리기
   - 정지 연발
   - 이동 연발
   - 반동 및 카메라 흔들림

## 검증 기준

1. 맨손 상태에서 크라우치가 다시 정상 토글된다.
2. `Prone -> Crouch` 전환 시 서버/클라이언트 자세가 일치한다.
3. 무기 장착 후 WASD 입력 방향이 정상적으로 반영된다.
4. 연발 중 카메라가 과도하게 흔들리지 않는다.
5. 라이플 홀드 발사 시 탄창이 비워질 때까지 연발이 유지된다.
6. 연발 중에도 반동은 매 탄 지속 적용된다.

## 리스크

- 크라우치를 급하게 롤백하면 처음 해결한 `Prone -> Crouch` 서버 불일치가 다시 살아날 수 있다.
- 반동 완화만 먼저 하면 이동 버그는 가려지지만 실제 원인을 놓칠 수 있다.
- BP_Character 또는 ABP에서 stance/state를 다시 덮어쓰고 있다면 코드 수정만으로는 완전히 끝나지 않을 수 있다.

