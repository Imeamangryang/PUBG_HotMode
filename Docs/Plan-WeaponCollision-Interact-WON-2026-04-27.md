# Weapon Collision / Interact Plan

## 범위

- 수정 범위는 `Source/PUBG_HotMode/WON` 내부로 제한한다.
- `Combat`를 포함한 다른 팀 폴더 코드는 참고만 하고 수정하지 않는다.
- 이번 단계에서는 설계와 구현 계획만 정리하고, 실제 연결 구현은 하지 않는다.

## 현재 확인한 문제

- 총기 발사는 `UBG_WeaponFireComponent::TraceSingleShot()`에서 `ECC_Visibility` 라인트레이스를 사용한다.
- 현재 `WON` 코드에는 피격 전용 콜리전 컴포넌트가 없고, 캐릭터가 `Visibility`를 막는다는 보장도 없다.
- 그래서 라인트레이스가 상대 캐릭터를 통과할 가능성이 높다.
- 상호작용 키는 현재 `ABG_PlayerController::OnInteractInputStarted()`에서 낙하산 액션만 호출한다.
- `ABG_Character`에는 `CurrentInteractableWeapon`만 있고, 범위 감지용 오버랩 컴포넌트와 디버그 흐름은 아직 없다.
- 공격 입력은 현재 `AttackAction`의 `Started`만 바인딩되어 있어서, 버튼 홀드 기반 연사를 처리할 구조가 없다.

## 구현 목표

- 캐릭터 피격 인식을 위한 히트 콜리전을 `ABG_Character` 내부에 추가한다.
- 아이템/무기 상호작용 탐지를 위한 범위 콜리전을 `ABG_Character` 내부에 추가한다.
- 임시 무기 액터 또는 임의 액터에 전용 콜리전 프리셋을 적용하면, 범위 안에서 인터랙트 키 입력 시 디버그 메시지가 뜨도록 한다.
- 피스톨은 단발 유지, 라이플은 같은 공격 키를 눌렀을 때 홀드 연사가 가능하도록 구조를 설계한다.

## 설계 방향

### 1. 캐릭터 히트 콜리전

- `ABG_Character`에 `UCapsuleComponent` 또는 `UBoxComponent` 기반 피격용 서브 콜리전을 추가한다.
- 기본 캡슐과 역할을 분리해서, 이동 충돌은 기존 캐릭터 캡슐이 담당하고 피격 판정은 별도 컴포넌트가 담당하게 만든다.
- 크기는 캐릭터 메시/캡슐에 맞춰 기본값을 설정하되, 블루프린트에서 튜닝 가능하게 `EditDefaultsOnly`로 노출한다.
- 라인트레이스가 이 콜리전을 맞췄을 때 `HitResult.GetActor()`가 캐릭터를 반환하도록 캐릭터 소유 컴포넌트로 붙인다.

### 2. 상호작용 범위 콜리전

- `ABG_Character`에 `USphereComponent` 기반 상호작용 감지 범위를 추가한다.
- `OnComponentBeginOverlap` / `OnComponentEndOverlap`으로 감지 대상 액터를 관리한다.
- 우선순위는 단순하게 최근 진입 액터 1개 또는 가장 가까운 액터 1개만 유지한다.
- 범위 이탈 시 현재 타겟이 빠졌다면 `CurrentInteractableWeapon`을 비운다.
- null 체크 후 실패 시에는 기존 규칙대로 명시적 로그를 남긴다.

### 3. 임시 인터랙트 디버그

- 전용 콜리전 프리셋 이름은 예시로 `WeaponInteractable`로 둔다.
- 이번 단계에서는 실제 무기 클래스가 완성되지 않았으므로, 바닥에 둔 아무 액터라도 해당 프리셋을 가진 PrimitiveComponent가 있으면 상호작용 후보로 취급한다.
- `Interact` 키 입력 시:
- 현재 후보가 있으면 `OnScreenDebugMessage`와 `UE_LOG`로 "interact detected"를 출력한다.
- 현재 후보가 없으면 실패 원인을 로그로 남긴다.
- 낙하산 액션과 상호작용은 분기 조건을 둬서 충돌하지 않게 정리한다.

### 4. 발사 입력 정책

- IA/IMC만으로 무기별 발사 방식을 완전히 해결하는 것은 부족하다.
- 이유는 `InputAction`은 입력 이벤트를 전달할 뿐이고, 피스톨 단발/라이플 연사 같은 무기 규칙은 결국 무기 상태와 서버 권한을 아는 C++ 쪽에서 판단해야 하기 때문이다.
- 권장 구조는 `AttackAction` 하나를 유지하고, `Started` / `Completed`를 모두 바인딩한 뒤 무기 타입에 따라 발사 모드를 분기하는 방식이다.

## 입력 처리 권장안

### 권장 결론

- `IA_Attack` 하나로 유지한다.
- IMC는 같은 키 매핑만 담당한다.
- 실제 동작 차이는 C++에서 처리한다.

### 처리 방식

- `Started`: 공격 입력 시작 플래그 활성화, 단발 무기면 즉시 1회 발사, 연사 무기면 연사 루프 시작
- `Completed`: 공격 입력 종료 플래그 비활성화, 연사 루프 정지
- 피스톨: `Started`에서만 1회 발사
- 라이플: 누르고 있는 동안 `FireCooldown` 기준 반복 발사

### 구현 위치

- 입력 바인딩 변경은 `ABG_PlayerController`
- 발사 모드와 반복 발사 상태 관리는 `UBG_WeaponFireComponent`
- 캐릭터는 입력 위임만 담당하고, 무기별 정책은 컴포넌트로 모은다

## 예상 수정 파일

- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_PlayerController.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_PlayerController.cpp`
- `Source/PUBG_HotMode/WON/Public/Combat/BG_WeaponFireComponent.h`
- `Source/PUBG_HotMode/WON/Private/Combat/BG_WeaponFireComponent.cpp`

## 구현 순서

1. `ABG_Character`에 피격 콜리전과 상호작용 범위 콜리전을 추가한다.
2. 오버랩 진입/이탈 핸들러와 현재 상호작용 후보 관리 로직을 추가한다.
3. `Interact` 입력이 낙하산/상호작용 중 어떤 흐름으로 갈지 분기한다.
4. 임시 인터랙트 디버그 메시지를 붙인다.
5. 공격 입력에 `Completed` 바인딩을 추가한다.
6. `UBG_WeaponFireComponent`에 단발/연사 모드와 홀드 상태 처리 로직을 추가한다.
7. 피격 라인트레이스가 새 히트 콜리전을 맞는지 검증한다.

## 검증 기준

- 캐릭터를 향해 발사했을 때 라인트레이스가 캐릭터를 통과하지 않고 히트 로그가 남는다.
- 캐릭터 크기와 크게 어긋나지 않는 히트 판정이 나온다.
- `WeaponInteractable` 프리셋이 적용된 액터 근처에서만 인터랙트 입력 디버그가 출력된다.
- 범위를 벗어나면 인터랙트 입력이 더 이상 반응하지 않는다.
- 피스톨은 클릭 1회당 1발만 발사된다.
- 라이플은 버튼 홀드 동안만 연속 발사되고, 버튼을 떼면 즉시 멈춘다.

## 메모

- 콜리전 프리셋 생성 자체는 프로젝트 설정 또는 블루프린트 에디터 작업이 필요할 수 있다.
- 코드에서는 프리셋 이름 검사 또는 채널/오브젝트 타입 검사 둘 중 하나를 선택해야 한다.
- 실제 구현 시에는 현재 레벨/블루프린트 세팅까지 함께 확인해야 한다.
