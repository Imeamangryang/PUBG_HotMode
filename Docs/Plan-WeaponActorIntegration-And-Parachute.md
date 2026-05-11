## 작업 목표

- `WON`의 `BG_WeaponFireComponent`가 갖고 있는 발사 흐름 중 탄약/무기 타입/장전 상태 책임을 `GEONU/Combat/BG_EquippedWeaponBase`로 옮긴다.
- `BG_WeaponFireComponent`는 캐릭터 부착 컴포넌트로 유지하되, 발사 입력 처리, 몽타주 재생, 반동, 임시 히트스캔 디버그 발사 방식을 담당하게 정리한다.
- 서버 권한 기준으로 장전/발사/무기 교체가 동작하도록 구성한다.
- 임시 무기 장착 상태여도 캐릭터의 `bHasWeapon`, 무기 포즈, ABP 연동 상태가 항상 맞도록 유지한다.
- 낙하산 강하와 일반 점프 공중 상태를 분리하고, 착지 시 후속 애니메이션 재생용 상태를 만든다.

## 책임 분리 방향

### `ABG_EquippedWeaponBase`

- 모든 장비 무기의 부모 액터
- BP에서 설정 가능한 무기 타입/포즈/탄종/장탄수/재장전 시간/발사 방식 참조점 제공
- 현재 장전 탄약(`LoadedAmmo`)과 장전 가능 상태, 재장전 상태를 보관
- `InventoryComponent`의 탄약 수량과 서버에서 동기화하여 실제 장전 수량을 계산
- 발사 시 탄약 차감과 발사 가능 여부 판단
- `BG_WeaponFireTypes` / `WeaponItemDataRow` 기반으로 발사 방식과 타입 정보를 공유
- 무기 교체 시 액터를 파괴하지 않고 숨겨서 탄수 유지

### `UBG_WeaponFireComponent`

- 플레이어 입력 기반 발사 요청 시작/종료
- 반동, 카메라 셰이크, 발사 몽타주, 재장전 몽타주 재생
- 임시 디버그용 히트스캔 처리
- 라이플 자동 연사 타이머 관리
- 실제 탄약 감소/장전 성공 여부는 `EquippedWeaponBase`에 위임

### `UBG_EquipmentComponent`

- 장비 슬롯/활성 슬롯/복제 유지
- 활성 무기를 손 소켓에 붙이고, 비활성 무기는 등 소켓으로 보내며 `SetActorHiddenInGame(false/true)` 대신 필요 시 숨김 정책을 명확히 적용
- 액터 교체 시 destroy보다 기존 액터 유지가 필요한 시나리오를 반영
- 활성 무기 변경 시 캐릭터 무기 포즈/`bHasWeapon` 동기화

### `ABG_Character` / `ABG_PlayerController`

- 입력을 받아 현재 장착 무기 액터에 발사/장전/교체 요청
- `EquippedWeapon != nullptr` 기준으로 맨손 여부 판단
- `X` 키 입력으로 활성 무기 해제(`nullptr`) 상태 전환
- 임시 장착 중이어도 ABP에서 무기 포즈를 읽을 수 있게 상태 유지
- 낙하산/일반 점프 공중 상태 분리 및 착지 처리

## 구현 단계

1. 무기 타입 확장
   - `Sniper` 포즈 타입 추가
   - 관련 enum 매핑 추가
   - 스나이퍼용 발사 세팅/탄약 세팅/몽타주 포인터 추가

2. `ABG_EquippedWeaponBase` 확장
   - 복제되는 런타임 탄약 상태 추가
   - BP 노출용 무기 포즈/탄종/장탄수/재장전 시간/자동연사 여부 또는 데이터 참조 추가
   - `CanFire`, `ConsumeLoadedAmmo`, `CanReload`, `StartReload`, `FinishReload`, `GetMissingAmmoCount` 등 서버 함수 추가
   - 임시 무한 탄약 디버그 옵션 추가

3. `UBG_EquipmentComponent` 보강
   - 활성 무기 액터 조회/동기화 경로 정리
   - 활성 슬롯 변경 시 기존 액터 파괴 금지
   - 비활성 무기는 등으로 보내고 탄수 보존
   - 캐릭터의 무기 포즈를 `WeaponRow` 기준으로 갱신

4. `UBG_WeaponFireComponent` 리팩터링
   - 발사 가능 여부를 활성 `EquippedWeaponBase` 기준으로 조회
   - 장전/발사 성공 시 무기 액터의 상태를 갱신
   - 라이플만 `FullAuto`, 피스톨/샷건/스나이퍼는 단발
   - 임시 히트스캔 유지
   - 나중에 막을 위치를 주석으로 표시

5. `ABG_Character` / `ABG_PlayerController` 연동
   - `Reload` 입력은 현재 장착 무기에게 전달
   - `X`키로 맨손 전환
   - 장비 액터가 있는 동안 `bHasWeapon`과 포즈를 유지
   - 임시 장착 상태도 ABP 반영

6. 낙하산/착지 처리
   - `OnMovementModeChanged` 오버라이드 추가
   - 낙하 시작 높이 기록
   - 공중 진입 후 높이 차가 300cm 이상이면 낙하산용 공중 상태, 이하면 일반 점프 공중 상태
   - 낙하산 강하/낙하산 펼침/착지 몽타주 분기용 상태 변수 추가

## 서버 연동 기준

- 발사, 장전, 활성 무기 변경은 서버 권한에서 최종 승인한다.
- 클라이언트는 입력 요청만 보내고, 탄약 차감/장전 완료/탄수 유지의 최종 상태는 서버 복제를 따른다.
- `InventoryComponent`는 탄약 수량 원본만 관리한다.
- `EquippedWeaponBase`는 장전된 탄약과 발사 소비를 관리한다.

## 임시 디버그 유지 항목

- `InventoryComponent`에 탄약이 없어도 디버그용 무한 탄약 옵션으로 장전이 가능하도록 유지
- `WeaponFireComponent`는 당분간 히트스캔 사용

### 나중에 주석 처리하거나 교체할 위치

- `UBG_WeaponFireComponent` 내부 히트스캔 trace 실행부
- `ABG_EquippedWeaponBase` 내부 디버그용 무한 탄약 fallback 분기
- 임시 키 입력으로 무기 포즈를 강제로 바꾸는 컨트롤러 로직

## 주석 작성 원칙

- 큰 책임 블록 시작점에 "이 블록이 어떤 역할인지" 설명하는 주석을 추가
- 서버 권한, 임시 디버그, ABP 연동, 무기 액터 책임 경계가 드러나는 곳 위주로 작성
- 코드가 이미 표현하는 내용을 반복하는 주석은 넣지 않음
