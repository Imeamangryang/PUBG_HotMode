# Pickup Montage / Recoil Plan

## 목표

- 붕대, 총기, 탄약 등 월드 아이템을 줍는 공용 흐름에서 아이템 타입별 줍기 몽타주를 넣을 수 있는 확장 지점을 만든다.
- 총 발사 반동 애니메이션은 C++이 직접 Pose를 만들지 않고, ABP가 읽을 수 있는 발사 신호를 노출하는 구조로 정리한다.

## 현재 구조 요약

- 아이템 픽업 성공 판정은 `ABG_WorldItemBase::TryPickup()` 서버 로직에서 끝난다.
- 총 발사 애니메이션 재생은 현재 `UBG_WeaponFireComponent::PlayWeaponFireAnimation()`에서 몽타주 방식으로 처리 중이다.
- `ABG_Character`는 상태와 입력 허브 역할을 하고 있고, 장비 상태는 `UBG_EquipmentComponent`가 캐릭터 pose 상태로 반영한다.

## 구현 방향

### 1. Pickup Montage 전용 컴포넌트 추가

- 신규 `UBG_InteractionAnimationComponent`를 추가한다.
- 책임:
  - 아이템 타입별 pickup montage 선택
  - 캐릭터 mesh / anim instance 검증
  - montage 재생 공통 처리
- 기본 노출 값:
  - `DefaultPickupMontage`
  - `AmmoPickupMontage`
  - `HealPickupMontage`
  - `WeaponPickupMontage`
  - `ArmorPickupMontage`
  - `BackpackPickupMontage`
  - `ThrowablePickupMontage`
  - `PickupMontageSlotName`

### 2. 성공한 픽업에서 공용 호출

- `ABG_Character`에 pickup 성공 알림 함수와 multicast RPC를 추가한다.
- `ABG_WorldItemBase::TryPickup()`의 각 성공 경로에서 `RequestingCharacter`를 통해 pickup montage 재생을 요청한다.
- 이렇게 하면 붕대, 탄약, 총기, 방어구, 가방이 모두 같은 진입점으로 애니메이션을 재생한다.

### 3. 발사 반동용 ABP 신호 추가

- `UBG_WeaponFireComponent`에 최근 발사 시점을 추적하는 값을 추가한다.
- 서버 발사 성공 시 replicated counter를 갱신하고, 클라이언트는 `OnRep`에서 로컬 발사 시각을 기록한다.
- ABP에서 사용할 수 있도록 다음 조회 함수를 노출한다.
  - `GetTimeSinceLastFireAnimation()`
  - `HasRecentFireAnimation()`
- `ABG_Character`에는 `GetWeaponFireComponent()` getter를 추가한다.

## 반동 적용 기준

- 총 발사 반동 “애니메이션”은 ABP에서 적용하는 쪽이 맞다.
- 이유:
  - 반동은 locomotion, 조준, 자세, lean과 블렌딩되어야 한다.
  - 캐릭터/컴포넌트는 “언제 발사했는지” 신호만 보내고, ABP가 그 신호로 additive pose / slot montage / layered blend를 결정하는 편이 확장성이 좋다.
- 캐릭터 또는 컴포넌트가 할 일:
  - 발사 성공 시점 기록
  - 무기 타입, 조준 여부 같은 상태 제공
- ABP가 할 일:
  - 최근 발사 여부 기반 반동 additive 적용
  - 자세별 blend
  - 필요 시 무기군별 recoil curve 또는 montage 분기

## 수동 후속 작업

- 에디터에서 `ABG_Character` Blueprint의 `InteractionAnimationComponent`에 pickup montage asset들을 지정해야 한다.
- ABP에서는 `Get Owning Actor -> Cast to BG_Character -> GetWeaponFireComponent`로 recent fire 신호를 읽어 recoil pose를 연결해야 한다.

## 검증 계획

- 붕대 pickup 시 montage 재생 확인
- 탄약 pickup 시 montage 재생 확인
- 총기 pickup / 교체 pickup 시 montage 재생 확인
- dedicated server PIE에서 owner와 remote client 모두 pickup montage가 보이는지 확인
- 발사 직후 ABP가 읽을 수 있는 recent fire 값이 갱신되는지 확인
