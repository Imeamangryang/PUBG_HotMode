# 인벤토리 시스템 명세

## 목표

Dedicated Server 멀티플레이 환경에서 PUBG 스타일 인벤토리, 장비, 월드 아이템, 재장전, 아이템 사용 시스템 구현

성공 기준:

- 서버 검증 기반 pickup, stack, drop, equip, unequip, use, reload
- 슬롯 수 제한 없는 무게 기반 일반 인벤토리
- 일반 인벤토리와 분리된 고정 무기 슬롯
- Health, Boost, 장비, 탄약, 재장전, 아이템 사용 상태의 일관된 복제
- ViewModel 기반 UI 갱신
- 기존 무기 상태와 입력 흐름의 최소 변경

## 가정

- 주 작업 위치: `Source/PUBG_HotMode/GEONU`, `Content/GEONU`
- WON 영역 변경: Character, PlayerController, PlayerState, DamageSystem 연결에 필요한 최소 변경
- 서버와 클라이언트의 동일 item registry/DataTable asset 접근 가능
- CSV source를 DataTable uasset 반복 import 원본으로 관리
- UI는 UMG 중심 설계
- 필요한 UI 입력 라우팅, 버튼, 탭, 모달 영역에 CommonUI 사용
- Gameplay/UI 로직 흐름은 C++ 구현
- 순수 표시와 widget 조립은 Blueprint 구현

## 확정 결정

- Health source of truth: `ABG_PlayerState`가 아닌 Character 소유 `UBG_HealthComponent`
- Boost 위치: `UBG_HealthComponent` 내부
- Boost 1차 범위: `BoostGauge` 저장, 복제, 아이템 적용
- Boost 후순위 범위: decay, 자동 HP 회복, 속도 보정
- Health component 구현 위치: 기존 WON 전투 모듈 경로 `Source/PUBG_HotMode/WON/Public/Combat/BG_HealthComponent.h`, `Source/PUBG_HotMode/WON/Private/Combat/BG_HealthComponent.cpp` 유지
- Equipment adapter 위치: `UBG_EquipmentComponent` 내부
- Owner-only 복제 의미: 보안 경계가 아닌 네트워크 최적화
- 런타임 item key: `EBG_ItemType` + `FGameplayTag`
- DataTable RowName: `ItemTag.GetTagName()`과 동일
- 장비 레벨 tag 표기: `Lv1`, `Lv2`, `Lv3`
- Throwable 모델: Inventory 수량 유지, Throwable 슬롯은 선택 타입만 참조
- Armor durability 복제: owner 중심
- World Item 표시: 아이템별 `ABG_WorldItemBase` Blueprint subclass
- Data 원본: `Content/GEONU/Data/Source/*.csv`
- Registry provider: `UGameInstanceSubsystem`
- Registry load: AssetManager 기반
- 제외 기술: GAS, Unreal MVVM plugin

## 현재 저장소 맥락

| 영역            | 현재 상태                                                                       | 변경 방향                                         |
| --------------- | ------------------------------------------------------------------------------- | ------------------------------------------------- |
| Health          | `ABG_Character`가 `UBG_HealthComponent`를 소유, `ABG_PlayerState` health state 제거됨 | 현 구조 유지                                      |
| Damage          | `UBG_DamageSystem`이 target Character의 HealthComponent에 damage 적용            | 현 구조 유지                                      |
| Character state | `CharacterState`, `bIsDead`, `bIsWeaponEquipped`, `EquippedWeaponPoseType` 복제 | Equipment adapter로 동기화                        |
| Health UI       | `UBG_HealthViewModel`이 possessed Character의 HealthComponent에 바인딩           | 현 구조 유지                                      |
| GameInstance    | `UBG_GameInstance` 사용 중                                                      | Registry는 별도 GameInstanceSubsystem 사용        |
| CommonUI        | plugin enabled                                                                  | UMG 중심 설계에서 필요한 UI 입력/위젯 영역에 사용 |

## 범위

### 포함

- Item enum과 DataTable row struct
- GameplayTag 기반 item identity
- AssetManager 기반 item registry
- Health migration
- BoostGauge 저장/복제/아이템 적용
- FastArray 기반 일반 인벤토리
- 무게 계산과 Backpack weight bonus
- Weapon, Armor, Backpack 장비 상태
- 기존 Character weapon state adapter
- Generic World Item pickup/drop
- Inventory ammo 기반 reload
- Heal/Boost item use
- Inventory ViewModel
- Dedicated Server + 2 Client 검증

### 제외

- GAS 통합
- Unreal MVVM plugin 통합
- 코스메틱 슬롯
- 속옷 슬롯
- 완전한 무기 발사 시스템 재작성
- CommonUI 기반 UI polish 전체
- SaveGame persistence
- SkeletalMesh World Item 표시
- DataTable/Registry를 우회한 임의 World Item actor class spawn
- 최종 밸런스 데이터 확정

## 스택과 의존성

- Engine: UE 5.7 Source Build
- Compiler: VC++ 2022
- Runtime: Multiplayer with Dedicated Server
- 기존 module: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `UMG`
- 추가 module: `GameplayTags`, `NetCore`
- UI framework: UMG 중심, 필요한 부분에 CommonUI 사용
- CommonUI module: C++에서 CommonUI 타입 직접 참조 시 추가
- GameplayTags config: `Config/DefaultGameplayTags.ini`, `Config/Tags/BG_InventoryItems.ini`
- AssetManager config: `Config/DefaultGame.ini` item registry Primary Asset Type

## 검증 명령

```powershell
& "<UE_ROOT>\Engine\Build\BatchFiles\Build.bat" PUBG_HotModeEditor Win64 Development -Project="D:\Dev\P4\PUBG_HotMode\PUBG_HotMode.uproject" -WaitMutex
```

수동 검증:

- Dedicated Server + 2 Clients PIE
- 소유 클라이언트 기준 pickup/drop/equip/reload/use
- 원격 클라이언트 기준 weapon/armor/world item 외형 상태

## 네이밍과 코드 스타일

- UE5 naming convention
- 신규 GEONU class의 기존 underscore 스타일 허용
- Health component 이름: `UBG_HealthComponent`
- 기존 WON class/file rename 금지
- Code identifier: English
- 주석: 기존 파일 언어 우선
- Null failure: 명시적 error log
- 설계 기본값: ActorComponent
- 기존 source behavior 유지

## 아키텍처

```text
ABG_Character
├── UBG_HealthComponent
├── UBG_InventoryComponent
├── UBG_EquipmentComponent
│   └── 기존 weapon/action state adapter
└── UBG_ItemUseComponent

ABG_PlayerController
├── UBG_HealthViewModel
└── UBG_InventoryViewModel

UGameInstance
└── UBG_ItemDataRegistrySubsystem
    └── AssetManager 기반 registry cache
```

| 클래스                          | 책임                                                                  |
| ------------------------------- | --------------------------------------------------------------------- |
| `UBG_HealthComponent`           | HP, death, damage, heal, BoostGauge, Health/Boost 복제                |
| `UBG_InventoryComponent`        | stack inventory, weight, add/remove, pickup/drop 요청                 |
| `UBG_EquipmentComponent`        | weapon slots, armor, backpack, active slot, 기존 weapon state adapter |
| `UBG_ItemUseComponent`          | Heal/Boost 사용, 취소, 소모, 효과 적용                                |
| `UBG_ItemDataRegistry`          | 타입별 DataTable 참조와 row 조회                                      |
| `UBG_ItemDataRegistrySubsystem` | registry load/cache/provider                                          |
| `ABG_WorldItemBase`            | 서버 소유 pickup/drop world item 상태                                 |
| `UBG_InventoryViewModel`        | Inventory/Equipment UI 데이터와 요청 전달                             |
| `UBG_HealthViewModel`           | Health/Boost HUD 데이터                                               |

의존 방향:

```text
Widget
  -> ViewModel
    -> Component
      -> Server RPC
        -> Server state
          -> Replication / failure RPC
            -> ViewModel broadcast
```

## Health와 Boost

- 기준 상태: `UBG_HealthComponent`
- PlayerState Health gameplay state 제거
- DamageSystem target: Character HealthComponent
- HealthViewModel target: possessed Character HealthComponent
- Character death state 동기화: HealthComponent death event
- Boost 1차 구현: gauge 저장, 복제, AddBoost 처리
- Boost 후순위 구현: decay, 자동 회복, 속도 보정

복제:

| 상태         | 방침                       |
| ------------ | -------------------------- |
| `CurrentHP`  | 전체 복제                  |
| `MaxHP`      | 전체 복제 또는 config 고정 |
| `bIsDead`    | 전체 복제                  |
| `BoostGauge` | owner 중심 복제            |

## 아이템 데이터

Runtime key:

```cpp
EBG_ItemType ItemType;
FGameplayTag ItemTag;
```

장비 tag:

```text
Item.Equipment.Helmet.Lv1
Item.Equipment.Helmet.Lv2
Item.Equipment.Helmet.Lv3
Item.Equipment.Vest.Lv1
Item.Equipment.Vest.Lv2
Item.Equipment.Vest.Lv3
Item.Equipment.Backpack.Lv1
Item.Equipment.Backpack.Lv2
Item.Equipment.Backpack.Lv3
```

DataTable:

- `DT_Items_Ammo`
- `DT_Items_Heal`
- `DT_Items_Boost`
- `DT_Items_Throwable`
- `DT_Items_Weapon`
- `DT_Items_Backpack`
- `DT_Items_Armor`

Row 규칙:

- `ItemType` 필수
- `ItemTag` 필수
- RowName과 `ItemTag.GetTagName()` 일치
- 요청 타입과 row `ItemType` 일치
- Armor row: Helmet/Vest 전용
- Backpack row: Backpack DataTable 전용
- World Item 표시: `WorldItemClass`, icon, display data

CSV 원본:

```text
Content/GEONU/Data/Source/
├── Items_Ammo.csv
├── Items_Heal.csv
├── Items_Boost.csv
├── Items_Throwable.csv
├── Items_Weapon.csv
├── Items_Backpack.csv
└── Items_Armor.csv
```

## Item Data Registry

- Registry asset: `UBG_ItemDataRegistry : UPrimaryDataAsset`
- Registry asset path: `Content/GEONU/Data/DA_ItemDataRegistry`
- Provider: `UBG_ItemDataRegistrySubsystem : UGameInstanceSubsystem`
- Load path: AssetManager
- Component 접근: Subsystem 경유
- Missing registry/row: error log + validation failure

## Inventory Component

저장 대상:

- Ammo
- Heal
- Boost
- stackable Throwable

제외 대상:

- Weapon
- 장착 중 Helmet
- 장착 중 Vest
- 장착 중 Backpack

규칙:

- 슬롯 수 제한 없음
- 무게 제한 적용
- `MaxStackSize` 적용
- 무게 초과 시 수용 가능 수량만 추가
- 서버 권위 state mutation
- FastArray 기반 owner 중심 복제

## Equipment Component

슬롯:

```text
Primary A
Primary B
Pistol
Melee
Throwable
Helmet
Vest
Backpack
```

규칙:

- Weapon은 고정 weapon slot에만 저장
- Client `ItemTag` 단독 weapon 생성 금지
- Weapon pickup/equip 기준: 서버 검증 WorldItem 또는 서버 slot state
- Primary A/B 동일 제약
- slot 교체 시 old weapon world drop
- Helmet/Vest 독립 장착
- Backpack 변경 시 Inventory max weight 갱신
- Backpack 해제/교체 시 overweight 거부

Throwable:

- 수량 위치: Inventory
- 슬롯 의미: 선택된 throwable 타입
- 사용 처리: Inventory 수량 차감
- 수량 0 처리: 슬롯 비움

기존 weapon adapter:

- 위치: `UBG_EquipmentComponent`
- active weapon to `EBGWeaponPoseType`
- 기존 `ABG_Character::SetWeaponState` 활용
- 기존 `bIsWeaponEquipped`, `EquippedWeaponPoseType`, `bCanAim`, `bCanReload`, `bCanFire` 유지

복제:

- 원격 외형 state: 전체 복제
- owner UI state: owner 중심 복제
- armor durability: owner 중심 복제

## World Item Actor

- Actor: `ABG_WorldItemBase`
- Blueprint: 아이템별 `ABG_WorldItemBase` subclass BP
- 표시 mesh: Blueprint subclass가 소유
- 표시 데이터: DataTable row
- category별 C++ child class 없음
- SkeletalMesh 없음
- row별 actor class override는 `WorldItemClass` soft class로만 허용

상태:

| 필드               | 용도                  |
| ------------------ | --------------------- |
| `ItemType`         | DataTable 선택        |
| `ItemTag`          | row 식별              |
| `Quantity`         | 남은 수량             |
| `WeaponLoadedAmmo` | weapon drop 탄약 보존 |

픽업 검증:

- WorldItem valid
- 요청 pawn/controller valid
- 요청 수량 양수
- interaction range 내부
- registry row 일치
- stack item은 Inventory 처리
- Weapon/Armor/Backpack은 Equipment 처리

드롭 검증:

- 보유 item 존재
- 요청 수량 양수
- 요청 수량 보유량 이하
- state 제거 후 world item spawn
- 실패 사유 owning client 전달

## Item Use

대상:

- Heal
- Boost

규칙:

- 요청 key: `EBG_ItemType` + `FGameplayTag`
- 서버 row 검증
- 서버 수량 검증
- death/use state 검증
- 서버 timer 기반 진행
- owner progress 복제
- 완료 시 수량 재검증
- 소모 성공 후 효과 적용
- 취소 조건: 이동, 피격, 발사, weapon switch, death, explicit cancel

효과:

```cpp
NewHP = FMath::Min(CurrentHP + HealAmount, HealCap);
NewBoost = FMath::Clamp(CurrentBoost + BoostAmount, 0.f, MaxBoost);
```

## 재장전 연동

Flow:

```text
Client reload input
  -> existing input/action flow
  -> Equipment active slot
  -> server active weapon validation
  -> weapon row AmmoItemTag
  -> Inventory ammo quantity
  -> server reload timer
  -> completion revalidation
  -> Inventory ammo consume
  -> LoadedAmmo update
```

취소 조건:

- weapon switch
- fire
- death
- equipment change
- active slot change

## UI

- `UBG_InventoryViewModel` 위치: `ABG_PlayerController`
- Bind target: possessed Character Inventory/Equipment
- Pawn 변경 시 rebind
- Render data: replicated state + registry display row
- Widget action: ViewModel 경유 request/RPC
- Failure UI: owning-client failure RPC
- Health UI와 Inventory UI 책임 분리
- Gameplay/UI 로직 흐름: C++
- Widget 표시와 조립: Blueprint
- 기본 설계: UMG 중심
- CommonUI 사용: 필요한 입력 라우팅, 버튼, 탭, 모달 영역

## 네트워크 규칙

- 서버 권위 state mutation
- Client RPC는 요청 전용
- 서버 재검증: item type, tag, quantity, range, ownership, slot compatibility
- UI 갱신: replicated state 또는 owning-client failure RPC
- server-only delegate 기반 client UI 갱신 금지
- Owner-only replication은 최적화 선택

## 실패 사유

```cpp
UENUM(BlueprintType)
enum class EBGInventoryFailReason : uint8
{
    None,
    InvalidItem,
    InvalidQuantity,
    TooFar,
    Overweight,
    SlotMismatch,
    MissingAmmo,
    AlreadyUsingItem,
    HealthCapReached,
    ServerRejected
};
```

## 프로젝트 구조

```text
Source/PUBG_HotMode/GEONU/
├── Inventory/
│   ├── BG_ItemTypes.h
│   ├── BG_ItemDataRow.h
│   ├── BG_ItemDataRegistry.h
│   ├── BG_ItemDataRegistry.cpp
│   ├── BG_ItemDataRegistrySubsystem.h
│   ├── BG_ItemDataRegistrySubsystem.cpp
│   ├── BG_InventoryTypes.h
│   ├── BG_InventoryComponent.h
│   ├── BG_InventoryComponent.cpp
│   ├── BG_EquipmentComponent.h
│   ├── BG_EquipmentComponent.cpp
│   ├── BG_ItemUseComponent.h
│   ├── BG_ItemUseComponent.cpp
│   ├── BG_WorldItemBase.h
│   └── BG_WorldItemBase.cpp
├── UI/
│   ├── BG_InventoryViewModel.h
│   └── BG_InventoryViewModel.cpp
├── BG_HealthViewModel.h
└── BG_HealthViewModel.cpp

Source/PUBG_HotMode/WON/
├── Public/Combat/BG_HealthComponent.h
└── Private/Combat/BG_HealthComponent.cpp

Content/GEONU/
├── Data/
│   ├── DA_ItemDataRegistry
│   ├── Source/
│   │   ├── Items_Ammo.csv
│   │   ├── Items_Heal.csv
│   │   ├── Items_Boost.csv
│   │   ├── Items_Throwable.csv
│   │   ├── Items_Weapon.csv
│   │   ├── Items_Backpack.csv
│   │   └── Items_Armor.csv
│   ├── DT_Items_Ammo
│   ├── DT_Items_Heal
│   ├── DT_Items_Boost
│   ├── DT_Items_Throwable
│   ├── DT_Items_Weapon
│   ├── DT_Items_Backpack
│   └── DT_Items_Armor
└── UI/
    └── WBP_Inventory
```

최소 WON 변경:

- `ABG_Character`: component attachment, adapter hook
- `ABG_PlayerController`: ViewModel attachment/lookup
- `UBG_HealthComponent`: 기존 WON Combat 파일 유지, Character 소유 Health/Boost source of truth 역할
- `ABG_PlayerState`: Health gameplay state 제거
- `UBG_DamageSystem`: HealthComponent target 전환

## 구현 순서

1. `GameplayTags`, `NetCore`, item gameplay tags
2. item row struct, CSV source, registry asset
3. `UBG_ItemDataRegistrySubsystem`
4. `UBG_HealthComponent` migration
5. Inventory FastArray와 weight
6. Equipment state와 weapon adapter
7. World Item pickup/drop
8. Inventory ViewModel과 기본 UI
9. Reload ammo consume
10. Heal/Boost item use
11. Boost decay, 자동회복, 속도 보정
12. Dedicated Server + 2 Client 검증

## 테스트 전략

컴파일:

- 주요 component slice별 build
- module dependency 누락 확인

로직 검증:

- weight fitting
- stack add/remove
- DataTable validation
- Backpack capacity
- reload ammo count
- tag/RowName validation

PIE 수동 검증:

- Dedicated Server + 2 Clients
- server-validated pickup
- owner inventory update
- remote weapon/armor visual state
- Backpack max weight 증가
- overweight Backpack unequip 거부
- drop world item spawn
- reload ammo consume
- reload cancel
- Heal item consume + HP restore
- Boost item consume + BoostGauge 증가
- item use cancel
- requesting client failure UI

## 경계

항상:

- 서버 검증
- gameplay null failure error log
- ViewModel 경유 UI 요청
- `EBG_ItemType` + `FGameplayTag` runtime identity
- 기존 source behavior 유지
- CSV source와 DataTable uasset 동기화

먼저 확인:

- GEONU 범위 밖 대형 리팩터
- 기존 weapon state 제거
- 기존 input flow 교체
- 새 third-party dependency
- content authoring 이후 public gameplay tag 변경
- Save persistence 도입

금지:

- client tag만 신뢰한 weapon spawn
- Widget의 gameplay state 직접 변경
- server gameplay null failure 무시
- build 통과 목적의 test/check 약화

## 성공 기준

- `Docs/Plan-InventorySystem.md` 갱신
- required module dependency 포함 build 성공
- PlayerState Health 제거
- `UBG_HealthComponent` Health/Boost 기준 상태 동작
- Inventory add/remove/weight 서버 동작
- Equipment pickup/equip/drop과 기존 weapon state 공존
- GameInstanceSubsystem registry lookup server/client 동작
- CSV source 기반 DataTable import 절차 확정
- ViewModel replicated update broadcast
- Dedicated Server + 2 Client 핵심 flow 통과
