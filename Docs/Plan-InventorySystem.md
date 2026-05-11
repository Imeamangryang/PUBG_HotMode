# PUBG 인벤토리 시스템 구현 계획

## 문서 역할

- 요구사항, 확정 결정, 세부 규칙의 원본은 `Docs/Spec-InventorySystem.md`
- 이 문서는 Spec을 구현 가능한 작업 순서로 분해하고, 각 작업의 수용 기준과 검증 방법을 정의
- Plan에는 Spec의 상세 데이터 모델과 게임 규칙을 반복하지 않고, 구현자가 바로 실행할 수 있는 의존성, 파일 범위, 체크포인트를 기록
- Spec 변경이 필요한 설계 판단은 먼저 Spec에 반영한 뒤 이 Plan의 작업 순서와 검증 기준을 갱신
- 작업은 `#1 모듈 설정` 형식으로 표기한다. 숫자는 실행 순서, 이름은 참조 기준이다.

## 구현 분석

### 핵심 의존성

| 축 | 먼저 고정해야 하는 이유 | 후속 작업 |
| --- | --- | --- |
| 모듈, GameplayTags, AssetManager 설정 | 모든 item key, registry lookup, FastArray 복제의 기반 | DataTable, registry, inventory, equipment |
| Item row와 registry | 서버 검증과 클라이언트 표시 데이터의 공통 계약 | pickup, equip, reload, use, UI |
| HealthComponent 기준 상태 | damage, item use, HUD가 동일 source of truth를 사용해야 함 | DamageSystem migration, Heal/Boost use |
| InventoryComponent | ammo, heal, boost, throwable 수량과 무게 계산의 기준 | backpack, reload, item use, UI |
| EquipmentComponent | weapon/armor/backpack 슬롯과 기존 weapon state 연결점 | world item, reload, remote visual replication |
| WorldItemActor | pickup/drop의 서버 검증 진입점 | core gameplay loop, UI 주변 아이템 목록 |
| ViewModel | Widget과 gameplay state의 경계 | Inventory UI, failure UI |

### 위험 우선순위

- `ABG_PlayerState` Health 제거는 respawn, possess, HUD bind 흐름을 깨뜨릴 수 있으므로 Health migration을 별도 체크포인트로 검증
- `UBG_ItemDataRegistrySubsystem` 로드 실패는 이후 모든 요청 검증을 막으므로 초기 단계에서 registry load와 invalid row 로그를 확인
- FastArray owner-only 복제는 UI 갱신 누락이 생기기 쉬우므로 Inventory 단독 단계에서 add/remove/weight delegate를 먼저 안정화
- Equipment는 public visual state와 owner-only 상세 state가 섞일 위험이 있으므로 복제 조건을 task acceptance에 명시
- 기존 weapon input/action flow 변경은 범위를 키우기 쉬우므로 reload task는 기존 흐름을 유지하는 adapter 방식으로 제한
- CommonUI는 C++ 타입을 직접 참조하는 시점에만 Build.cs 의존성을 추가

### 구현 경계

- 주 작업 위치는 `Source/PUBG_HotMode/GEONU`, `Content/GEONU`
- WON 영역 변경은 Character, PlayerController, PlayerState, DamageSystem 연결에 필요한 최소 범위로 제한
- 신규 gameplay 책임은 ActorComponent 기반으로 분리
- 서버 gameplay null failure는 silent fail 없이 명시적 error log를 남김
- 제외 범위는 GAS, Unreal MVVM plugin, SaveGame persistence, weapon fire system rewrite, cosmetic/underwear slot

## 기술 설계 요약

### 런타임 계약

- Item identity: `EBG_ItemType` + `FGameplayTag`
- Row lookup: `UBG_ItemDataRegistrySubsystem` 경유
- State mutation: 서버 권위
- UI request: Widget -> ViewModel -> Component RPC
- Failure feedback: owning client failure RPC
- World item 표시: item row별 actor BP class

### 컴포넌트 연결

| 컴포넌트 | 제공 API | 의존 대상 | 최초 구현 작업 |
| --- | --- | --- | --- |
| `UBG_HealthComponent` | damage, heal, death, boost | Character, DamageSystem, HealthViewModel | #4 Health Component |
| `UBG_InventoryComponent` | stack add/remove, weight, quantity | Item registry, Equipment backpack bonus | #6 Inventory FastArray |
| `UBG_EquipmentComponent` | weapon/equipment slots, active weapon, adapter | Item registry, Inventory | #7 Equipment Component |
| `UBG_ItemUseComponent` | heal/boost timer, cancel, consume/apply | Inventory, Health | #12 Item Use |
| `ABG_WorldItemBase` | pickup/drop replicated item state | Registry, Inventory, Equipment | #8 World Item 흐름 |
| `UBG_InventoryViewModel` | UI render data, request forwarding | Inventory, Equipment, Registry | #9 Inventory ViewModel |

### 의존성 그래프

```text
#1 모듈 설정
  -> #2 아이템 데이터 계약
    -> #3 Item Registry Subsystem
      -> #6 Inventory FastArray
        -> #7 Equipment Component
          -> #8 World Item 흐름
          -> #11 Reload 연동
      -> #9 Inventory ViewModel
        -> #10 Inventory UI

#1 모듈 설정
  -> #4 Health Component
    -> #5 Health Migration
    -> #12 Item Use

#4 Health Component + #6 Inventory FastArray + #9 Inventory ViewModel
  -> #12 Item Use

#1 모듈 설정부터 #12 Item Use까지의 핵심 작업
  -> #14 최종 검증

#13 Boost 후순위 효과는 #12 Item Use 이후 진행
```

### 병렬 가능성

| 시점 | 병렬 가능한 작업 | 조건 |
| --- | --- | --- |
| #1 모듈 설정 완료 후 | #2 아이템 데이터 계약과 #4 Health Component | item data 계약과 HealthComponent가 직접 충돌하지 않음 |
| #3 Item Registry Subsystem 완료 후 | #5 Health Migration 준비와 #6 Inventory FastArray 구현 | Health migration과 InventoryComponent 파일 범위 분리 |
| #6 Inventory FastArray 완료 후 | #7 Equipment Component 일부와 #9 Inventory ViewModel render data 설계 | Equipment public contract를 먼저 합의해야 함 |
| #8 World Item 흐름 완료 후 | #10 Inventory UI 조립과 #11 Reload 연동 | ViewModel request API가 고정되어야 함 |

## 공통 검증

Build:

```powershell
& "<UE_ROOT>\Engine\Build\BatchFiles\Build.bat" PUBG_HotModeEditor Win64 Development -Project="D:\Dev\P4\PUBG_HotMode\PUBG_HotMode.uproject" -WaitMutex
```

수동 검증:

- Dedicated Server + 2 Clients PIE
- owner client 기준 pickup, stack, drop, equip, unequip, reload, use
- remote client 기준 weapon, armor, world item 외형 상태
- owner-only inventory, ammo, durability, boost 정보가 원격 클라이언트에 노출되지 않는지 확인

## 구현 단위

### #1 모듈 설정

Description: Inventory 시스템 기반 기능에 필요한 빌드 모듈, GameplayTag, AssetManager 설정을 먼저 고정한다. C++에서 CommonUI 타입을 직접 참조하지 않는 동안에는 CommonUI 모듈 추가를 보류한다.

Acceptance:
- `GameplayTags`, `NetCore`가 `PUBG_HotMode.Build.cs`에 추가됨
- `DefaultGameplayTags.ini`와 `Config/Tags/*.ini`에 Spec의 item tag와 장비 `Lv1`/`Lv2`/`Lv3` tag가 등록됨
- `DA_ItemDataRegistry`용 Primary Asset Type 설정이 config 또는 문서화된 editor 절차로 확정됨

Verification:
- Build 성공
- Editor에서 GameplayTag와 Primary Asset Type 조회

Dependencies: 없음

Files likely touched:
- `Source/PUBG_HotMode/PUBG_HotMode.Build.cs`
- `Config/DefaultGameplayTags.ini`
- `Config/Tags/BG_InventoryItems.ini`
- `Config/DefaultGame.ini`

Scope: S

### #2 아이템 데이터 계약

Description: Runtime key인 `EBG_ItemType` + `FGameplayTag`를 기준으로 공통 Row와 타입별 Row를 정의하고, CSV 원본과 DataTable asset의 반복 import 구조를 만든다.

Acceptance:
- 공통 Row에 표시 정보, `WorldItemClass`, weight, stack 정보가 포함됨
- Ammo, Heal, Boost, Throwable, Weapon, Backpack, Armor 전용 Row가 Spec의 전용 필드를 가짐
- `Content/GEONU/Data/Source/*.csv`와 `Content/GEONU/Data/DT_Items_*` 이름 규칙이 Spec과 일치함

Verification:
- Build 성공
- 샘플 CSV import 후 RowName과 `ItemTag.GetTagName()` 일치 수동 확인

Dependencies: #1 모듈 설정

Files likely touched:
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemTypes.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemDataRow.h`
- `Content/GEONU/Data/Source/*.csv`
- `Content/GEONU/Data/DT_Items_*`

Scope: M

### #3 Item Registry Subsystem

Description: `UBG_ItemDataRegistry`를 `UPrimaryDataAsset`으로 구현하고, `UBG_ItemDataRegistrySubsystem`이 AssetManager를 통해 registry를 로드, 캐시, 제공하도록 만든다.

Acceptance:
- `DA_ItemDataRegistry`가 7개 타입별 DataTable을 참조함
- server/client 양쪽에서 typed row lookup 가능
- registry 또는 row 누락 시 error log와 validation failure가 발생함

Verification:
- Build 성공
- PIE 시작 로그에서 registry load 성공 확인
- 잘못된 tag 샘플 조회 시 명시적 error log 확인

Dependencies: #1 모듈 설정, #2 아이템 데이터 계약

Files likely touched:
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemDataRegistry.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemDataRegistry.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemDataRegistrySubsystem.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemDataRegistrySubsystem.cpp`
- `Content/GEONU/Data/DA_ItemDataRegistry`

Scope: M

### Checkpoint A: 데이터 기반 계약

- #1 모듈 설정, #2 아이템 데이터 계약, #3 Item Registry Subsystem 완료 후 build와 registry load를 먼저 확인
- 이후 작업은 DataTable 직접 참조 대신 subsystem provider만 사용
- RowName, `ItemTag`, `ItemType` 불일치가 error log로 드러나는지 확인

### #4 Health Component

Description: 기존 WON Combat 경로의 Character 소유 `UBG_HealthComponent`를 HP, death, damage, heal, BoostGauge 기준 상태로 서버 권위 구현한다.

Acceptance:
- `CurrentHP`, `MaxHP`, `bIsDead`는 전체 복제, `BoostGauge`는 owner 중심 복제됨
- `ApplyDamage`, `ApplyHeal`, `AddBoost`, `OnDamaged`, `OnHealthChanged` 경로가 있음
- null target 또는 invalid state는 silent fail 없이 error log를 남김

Verification:
- Build 성공
- Listen/Dedicated PIE에서 damage, heal, boost 값 복제 수동 확인

Dependencies: #1 모듈 설정

Files likely touched:
- `Source/PUBG_HotMode/WON/Public/Combat/BG_HealthComponent.h`
- `Source/PUBG_HotMode/WON/Private/Combat/BG_HealthComponent.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`

Scope: M

### #5 Health Migration

Description: `ABG_PlayerState`의 Health gameplay state를 제거하고 DamageSystem, Character death state, HealthViewModel을 Character HealthComponent 기준으로 전환한다.

Acceptance:
- `ABG_PlayerState`에 Health source of truth가 남지 않음
- `UBG_DamageSystem`이 target Character의 `UBG_HealthComponent`에 damage를 적용함
- `UBG_HealthViewModel`이 possessed Character 변경 시 component에 rebind됨

Verification:
- Build 성공
- Possess, respawn, pawn 변경 시 HUD health 갱신 수동 확인

Dependencies: #4 Health Component

Files likely touched:
- `Source/PUBG_HotMode/WON/Public/Player/BG_PlayerState.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_PlayerState.cpp`
- `Source/PUBG_HotMode/WON/Public/Combat/BG_DamageSystem.h`
- `Source/PUBG_HotMode/WON/Private/Combat/BG_DamageSystem.cpp`
- `Source/PUBG_HotMode/GEONU/BG_HealthViewModel.h`
- `Source/PUBG_HotMode/GEONU/BG_HealthViewModel.cpp`

Scope: L

### Checkpoint B: Health 기준 상태

- #4 Health Component와 #5 Health Migration 완료 후 Health/Boost source of truth를 안정화
- DamageSystem, Character death state, Health HUD가 모두 HealthComponent 기준인지 확인
- Inventory, ItemUse, UI 작업은 HealthComponent public API 기준으로 진행

### #6 Inventory FastArray

Description: owner-only FastArray 기반 일반 인벤토리와 무게 계산을 구현한다. 저장 대상은 Ammo, Heal, Boost, stackable Throwable이며 Weapon과 장착 장비는 제외한다.

Acceptance:
- `TryAddItem`, `TryRemoveItem`, `CanAddItem`, `GetQuantity`, `SetBackpackWeightBonus`가 서버 기준으로 동작함
- `MaxStackSize`와 weight limit이 적용되고 초과 시 수용 가능한 수량만 추가됨
- `InventoryList`, `CurrentWeight`, `MaxWeight`가 owner-only로 복제되고 UI delegate가 발생함

Verification:
- Build 성공
- PIE에서 stack add/remove, partial pickup, overweight 거부 수동 확인

Dependencies: #3 Item Registry Subsystem

Files likely touched:
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryTypes.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryComponent.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`

Scope: M

### #7 Equipment Component

Description: Primary A/B, Pistol, Melee, Throwable, Helmet, Vest, Backpack 상태를 관리하고 기존 Character weapon state와 최소 변경으로 동기화한다.

Acceptance:
- Weapon public state는 전체 복제, loaded ammo와 armor durability는 owner 중심 복제됨
- Backpack 장착, 해제, 교체가 Inventory max weight와 overweight 거부를 반영함
- Throwable 슬롯은 선택 타입만 보관하고 실제 수량은 Inventory에서 관리함

Verification:
- Build 성공
- 2 Client PIE에서 무기/방어구 외형 복제와 owner-only 상세 정보 수동 확인

Dependencies: #3 Item Registry Subsystem, #6 Inventory FastArray

Files likely touched:
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`

Scope: L

### #8 World Item 흐름

Description: 서버 소유 `ABG_WorldItemBase`를 구현하고, item row별 Blueprint subclass를 통해 pickup/drop world item 표시를 구성한다. pickup/drop 요청은 Inventory 또는 Equipment로 라우팅한다.

Acceptance:
- WorldItem은 `ItemType`, `ItemTag`, `Quantity`, `WeaponLoadedAmmo`를 복제함
- drop spawn은 DataTable row의 `WorldItemClass`를 우선 사용하고 비어 있으면 component fallback class를 사용함
- pickup은 range, quantity, registry row, slot compatibility를 서버에서 재검증함
- drop은 state 제거 후 generic World Item을 spawn하고 실패 사유를 owning client에 전달함

Verification:
- Build 성공
- Dedicated Server + 2 Client PIE에서 pickup/drop, partial pickup, too far 거부 수동 확인

Dependencies: #6 Inventory FastArray, #7 Equipment Component

Files likely touched:
- `Source/PUBG_HotMode/GEONU/Inventory/BG_WorldItemBase.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_WorldItemBase.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryComponent.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.cpp`
- `Content/GEONU/Data/DT_Items_*`
- `Content/GEONU/Data/Source/*.csv`

Scope: L

### Checkpoint C: Core Gameplay Loop

- #6 Inventory FastArray, #7 Equipment Component, #8 World Item 흐름 완료 후 pickup, stack, equip, drop을 Dedicated Server에서 먼저 검증
- 이 시점 이후 UI, reload, item use는 core component API를 변경하지 않는 방식으로 연결
- owner-only inventory 상태와 remote visual state의 복제 경계를 확인

### #9 Inventory ViewModel

Description: `ABG_PlayerController`에 `UBG_InventoryViewModel`을 두고 possessed Character의 Inventory/Equipment에 bind하여 UI 표시용 RenderData와 request API를 제공한다.

Acceptance:
- Pawn 변경 시 기존 delegate unbind 후 새 component에 rebind됨
- Inventory, weight, weapon slots, equipment, nearby item render data가 registry display row 기반으로 구성됨
- Widget 요청은 ViewModel을 통해 server request로만 전달됨

Verification:
- Build 성공
- PIE에서 possess 변경, inventory 변경, failure RPC 수신 시 delegate broadcast 확인

Dependencies: #6 Inventory FastArray, #7 Equipment Component, #8 World Item 흐름

Files likely touched:
- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.h`
- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_PlayerController.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_PlayerController.cpp`

Scope: M

### #10 Inventory UI

Description: UMG 중심으로 인벤토리 화면을 만들고, 필요한 입력 라우팅, 탭, 모달, 버튼 영역에 한해 CommonUI 사용 여부를 확정한다.

Acceptance:
- Inventory, nearby items, weight, weapon slots, Helmet/Vest/Backpack 표시가 ViewModel만 참조함
- pickup, drop, Backpack unequip, partial drop 요청이 ViewModel API로 연결됨
- CommonUI 타입을 C++에서 직접 참조하면 Build.cs 모듈 추가가 함께 반영됨

Verification:
- PIE에서 inventory open/close, 목록 갱신, 실패 메시지 표시 수동 확인
- C++ CommonUI 참조가 생긴 경우 build 성공

Dependencies: #9 Inventory ViewModel

Files likely touched:
- `Content/GEONU/UI/WBP_Inventory`
- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.h`
- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.cpp`
- `Source/PUBG_HotMode/PUBG_HotMode.Build.cs`

Scope: M

### Checkpoint D: UI Contract

- #9 Inventory ViewModel과 #10 Inventory UI 완료 후 Widget이 gameplay state를 직접 변경하지 않는지 점검
- owner-only 정보가 원격 클라이언트 UI에 노출되지 않는지 Dedicated Server에서 확인
- ViewModel request API가 reload/use 추가 전에 안정적인지 확인

### #11 Reload 연동

Description: 기존 reload input/action flow를 유지하면서 active weapon slot, weapon row의 `AmmoItemTag`, Inventory ammo quantity를 서버에서 검증해 재장전 완료 시 탄약을 소모한다.

Acceptance:
- 탄약이 없거나 active weapon이 유효하지 않으면 reload가 시작되지 않음
- 완료 시점에 weapon, slot, ammo를 재검증하고 `min(Need, Have)`만 차감함
- weapon switch, fire, death, equipment change, active slot change 시 reload가 취소됨

Verification:
- Build 성공
- PIE에서 no ammo reject, partial reload, reload cancel, owner LoadedAmmo HUD 갱신 확인

Dependencies: #6 Inventory FastArray, #7 Equipment Component

Files likely touched:
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryComponent.cpp`
- 기존 weapon/action flow 관련 WON 파일

Scope: M

### #12 Item Use

Description: `UBG_ItemUseComponent`가 Heal/Boost 사용 요청, 서버 timer, owner progress replication, 완료 재검증, 취소 조건을 처리한다.

Acceptance:
- Heal/Boost typed row, quantity, death/use state, HealthCap을 서버에서 검증함
- 완료 시 `TryRemoveItem` 성공 후에만 `ApplyHeal` 또는 `AddBoost`가 적용됨
- 이동, 피격, 발사, weapon switch, death, explicit cancel 조건에서 사용이 취소됨

Verification:
- Build 성공
- PIE에서 item consume, HP restore, BoostGauge 증가, 중복 사용 거부, 취소 조건 확인

Dependencies: #4 Health Component, #6 Inventory FastArray, #9 Inventory ViewModel

Files likely touched:
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemUseComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemUseComponent.cpp`
- `Source/PUBG_HotMode/WON/Private/Combat/BG_HealthComponent.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`

Scope: L

### Checkpoint E: Combat Consumables

- #11 Reload 연동과 #12 Item Use 완료 후 reload와 item use가 서로 취소 조건을 공유하는지 점검
- Inventory 수량, weapon ammo, Health/Boost가 서버 기준으로 일치하는지 확인
- failure reason이 요청 owning client에만 전달되는지 확인

### #13 Boost 후순위 효과

Description: 1차 성공 기준 이후 BoostGauge decay, 자동 HP 회복, 속도 보정을 별도 slice로 구현한다.

Acceptance:
- BoostGauge 구간별 decay와 자동 HP 회복 규칙이 Data/Config 또는 명시 상수로 분리됨
- 속도 보정은 Character movement와 충돌 없이 서버 기준으로 적용/복제됨
- Boost 1차 기능을 끄거나 변경하지 않고 확장됨

Verification:
- Build 성공
- PIE에서 시간 경과 decay, 자동 회복, 속도 보정 on/off 수동 확인

Dependencies: #4 Health Component, #12 Item Use

Files likely touched:
- `Source/PUBG_HotMode/WON/Public/Combat/BG_HealthComponent.h`
- `Source/PUBG_HotMode/WON/Private/Combat/BG_HealthComponent.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`

Scope: M

### #14 최종 검증

Description: 모든 핵심 flow를 Dedicated Server + 2 Clients에서 검증하고, 실패 사유와 owner-only 복제 경계를 확인한다.

Acceptance:
- pickup, stack, drop, equip, unequip, reload, use가 서버 검증 기반으로 통과함
- owner inventory, weight, ammo, durability, Boost 정보가 원격 클라이언트에 노출되지 않음
- 원격 클라이언트에서 weapon, armor, world item 외형 상태가 일관되게 보임

Verification:
- Dedicated Server + 2 Clients PIE
- build 명령 재실행
- 테스트 체크리스트 전 항목 수동 기록

Dependencies: #1 모듈 설정부터 #12 Item Use까지의 핵심 작업, #13 Boost 후순위 효과는 후순위 효과 검증 시에만 필요

Files likely touched:
- `Docs/Plan-InventorySystem.md`
- 필요 시 검증 기록 문서

Scope: S

## 테스트 체크리스트

### 데이터와 Registry

- [ ] GameplayTag 등록 확인
- [ ] DataTable RowName과 `ItemTag.GetTagName()` 일치 확인
- [ ] registry load 성공 로그 확인
- [ ] invalid row lookup error log 확인

### Health

- [ ] 데미지 적용 시 `CurrentHP` 감소
- [ ] HP 0 도달 시 `bIsDead` true
- [ ] `UBG_HealthViewModel`이 HealthComponent 변경을 수신
- [ ] 피격 delegate가 ItemUse 취소에 연결

### Inventory

- [ ] 같은 아이템 추가 시 기존 스택 우선 증가
- [ ] `MaxStackSize` 초과 시 새 엔트리 생성
- [ ] 무게 초과 시 일부 수량만 추가
- [ ] `CurrentWeight`와 `MaxWeight`가 owner UI에 갱신

### Equipment

- [ ] Primary A/B에 weapon 장착
- [ ] slot mismatch 장착 거부
- [ ] Helmet/Vest 독립 장착
- [ ] Backpack 장착 시 `MaxWeight` 증가
- [ ] Backpack 해제 시 초과 무게면 거부

### World Item

- [ ] 거리 밖 pickup 거부
- [ ] 잘못된 `ItemType`, `ItemTag` pickup 거부
- [ ] 부분 pickup 후 WorldItem 수량 감소
- [ ] 전체 pickup 후 WorldItem 제거
- [ ] drop 후 WorldItem 생성

### Reload

- [ ] 탄약 없을 때 reload 거부
- [ ] reload 완료 시 `min(Need, Have)`만 차감
- [ ] reload 중 weapon switch 시 취소
- [ ] 완료 시점에 탄약이 사라졌으면 loaded ammo 증가 없음

### Item Use

- [ ] HP가 HealCap 이상이면 heal item 사용 거부
- [ ] 사용 중 재사용 거부
- [ ] 이동, 피격, 발사, weapon switch, death 시 취소
- [ ] 완료 전에 아이템이 사라지면 효과 미적용
- [ ] 완료 시 item 1개 소모 후 HP/Boost 적용

### UI

- [ ] Widget이 ViewModel 없이 gameplay 객체를 직접 변경하지 않음
- [ ] Inventory, weight, equipment 변경 시 UI 갱신
- [ ] 서버 실패 사유가 요청 client에 표시
- [ ] 다른 client의 owner-only 정보가 표시되지 않음

## #1~#7 검증 기록 (2026-04-28)

자동 검증:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" PUBG_HotModeEditor Win64 Development -Project="D:\Dev\P4\PUBG_HotMode\PUBG_HotMode.uproject" -WaitMutex
```

결과: 성공. XGE license warning만 발생.

| 작업 | 결과 | 근거 |
| --- | --- | --- |
| #1 모듈 설정 | 통과 | `GameplayTags`, `NetCore` 모듈 추가, `Config/Tags/BG_InventoryItems.ini` item tags 등록, `Config/DefaultGame.ini` Primary Asset Type 설정 확인 |
| #2 아이템 데이터 계약 | 통과 | `EBG_ItemType`, 타입별 Row struct, CSV 원본, `Content/GEONU/Data/DT_Items_*` asset 구조 확인 |
| #3 Item Registry Subsystem | 통과 | `UBG_ItemDataRegistry` PrimaryDataAsset, AssetManager 기반 sync load, row/type/tag validation과 error log 경로 확인 |
| #4 Health Component | 통과 | `UBG_HealthComponent`는 기존 WON Combat 경로에 구현됨. HP/death full replication, Boost owner-only replication, damage/heal/boost API와 error log 확인 |
| #5 Health Migration | 통과 | `ABG_PlayerState` health state 제거, `UBG_DamageSystem` target HealthComponent 전환, `UBG_HealthViewModel` possessed Character bind 확인 |
| #6 Inventory FastArray | 통과 | owner-only FastArray, `TryAddItem`, `TryRemoveItem`, `CanAddItem`, weight partial accept, Backpack bonus 반영 확인 |
| #7 Equipment Component | 조건부 통과 | equipment state, owner-only ammo/durability, Backpack capacity, Throwable quantity hook, Character weapon adapter 확인. 기존 PlayerController 임시 숫자키 장착 입력은 EquipmentComponent를 우회하므로 후속 정리가 필요 |

수동 미검증:

- Dedicated Server + 2 Clients PIE의 owner-only 복제 경계
- 원격 클라이언트 weapon/armor visual state
- Editor에서 `DA_ItemDataRegistry` row import와 runtime registry load 로그

## #8 구현 기록 (2026-04-28)

구현:

- `ABG_WorldItemBase` 추가: `ItemType`, `ItemTag`, `Quantity`, `WeaponLoadedAmmo` 복제와 item별 Blueprint subclass spawn
- Character 소유 `Server_RequestPickupWorldItem` 경유 pickup 요청 추가. 서버에서 range, quantity, controller, registry row를 재검증
- stack item pickup은 `UBG_InventoryComponent::TryAddItem`으로 라우팅하고 partial pickup 시 WorldItem `Quantity` 감소
- Weapon, Armor, Backpack pickup은 `UBG_EquipmentComponent`로 라우팅하고 교체된 장비를 generic WorldItem으로 재스폰
- `UBG_InventoryComponent::RequestDropItem`/`TryDropItem` 추가. 일반 인벤토리 아이템 제거 후 WorldItem 스폰, 스폰 실패 시 수량 rollback
- `UBG_EquipmentComponent::RequestDropEquipment`/`TryDropEquipment` 추가. 무기 loaded ammo 보존, armor/backpack drop, throwable slot은 Inventory 수량 drop으로 위임
- `EBGInventoryFailReason`과 `ABG_Character::Client_ReceiveInventoryFailure` 추가

자동 검증:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" PUBG_HotModeEditor Win64 Development -Project="D:\Dev\P4\PUBG_HotMode\PUBG_HotMode.uproject" -WaitMutex
```

결과: 성공. XGE license warning만 발생.

수동 미검증:

- Dedicated Server + 2 Clients PIE에서 pickup/drop, partial pickup, too far 거부
- item별 WorldItem Blueprint의 실제 월드 표시 품질
- item별 WorldItem Blueprint authoring과 `WorldItemClass` override 경로

## #8 WorldItem BP subclass 구조 변경 기록 (2026-04-28)

구현:

- `FBG_ItemDataRow`에 `WorldItemClass` soft class 추가. Drop spawn은 row class를 우선 사용하고, 값이 비어 있거나 로드 실패 시 기존 component `WorldItemClass` fallback을 사용
- `ABG_WorldItemBase`의 runtime state를 Blueprint class/instance에서 authoring 가능하도록 `EditAnywhere`로 노출
- BP subclass가 자체 mesh와 relative transform을 완전히 소유하도록 DataTable mesh fallback을 제거
- CSV source에 `WorldItemClass` 컬럼 추가
- `Items_Heal.csv`의 Bandage row에 예정 `BP_WorldItem_Bandage_C` class 경로 반영

수동 필요:

- `Content/GEONU/Blueprints/Items/BP_WorldItem_Bandage` 생성 후 parent를 `ABG_WorldItemBase`로 설정. CSV source가 이 exact generated class path를 참조함: `BlueprintGeneratedClass'/Game/GEONU/Blueprints/Items/BP_WorldItem_Bandage.BP_WorldItem_Bandage_C'`
- BP의 `WorldMesh` component에 `SM_Bandage`를 지정하고 relative transform을 Bandage 표시에 맞게 조정
- `DT_Items_Heal`을 `Content/GEONU/Data/Source/Items_Heal.csv`에서 reimport하고 Bandage row의 `WorldItemClass`가 `BP_WorldItem_Bandage_C`로 들어갔는지 확인
- 다른 `DT_Items_*`도 새 `WorldItemClass` 컬럼을 반영하도록 CSV source에서 reimport

## #9 구현 기록 (2026-04-28)

구현:

- `UBG_InventoryViewModel` 추가. `ABG_PlayerController`가 소유하고 possessed Character의 `UBG_InventoryComponent`, `UBG_EquipmentComponent`, nearby WorldItem 변경 알림에 bind
- Inventory render data: 일반 인벤토리 entry를 registry display row 기반의 표시 이름, 설명, icon, quantity, weight 데이터로 변환
- Equipment render data: Weapon, Throwable, Helmet, Vest, Backpack 슬롯을 고정 순서로 구성하고 active weapon, loaded ammo, armor durability, throwable quantity를 표시 데이터에 포함
- Nearby item render data: Character interaction range에 들어온 `ABG_WorldItemBase` 목록을 registry display row 기반으로 구성
- `ABG_WorldItemBase::OnWorldItemStateChanged` 추가. partial pickup처럼 actor 수량만 바뀌는 경우도 nearby item render data가 갱신되도록 ViewModel이 현재 nearby WorldItem에 bind
- Widget request API: pickup, nearby pickup, inventory drop, equipment drop 요청을 ViewModel에서 Character/Component server request 경로로 전달
- Failure feedback: `ABG_Character::Client_ReceiveInventoryFailure`가 owning client의 `ABG_PlayerController`를 통해 `UBG_InventoryViewModel::OnInventoryFailureReceived`로 broadcast

자동 검증:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" PUBG_HotModeEditor Win64 Development -Project="D:\Dev\P4\PUBG_HotMode\PUBG_HotMode.uproject" -WaitMutex -LiveCoding
```

결과: 성공. XGE license warning만 발생.

수동 미검증:

- PIE에서 possess 변경 시 기존 delegate unbind 후 새 component에 rebind되는지 확인
- Inventory, weight, equipment, nearby item 변경 시 Widget delegate broadcast 확인
- Dedicated Server + 2 Clients PIE에서 실패 사유가 요청 owning client UI에만 전달되는지 확인

## #10 C++ 구현 기록 (2026-04-28)

구현:

- `WBP_Inventory`는 C++ 부모 클래스 없이 순수 `UserWidget` Blueprint로 제작하는 구조로 확정
- `ABG_PlayerController`에 `InventoryAction`, `InventoryWidgetClass`, `OpenInventoryUI`, `CloseInventoryUI`, `ToggleInventoryUI`, `IsInventoryUIOpen` 추가
- `ABG_PlayerController::GetInventoryViewModel`을 BlueprintPure로 노출하여 순수 Blueprint 위젯이 owning player에서 ViewModel을 직접 조회할 수 있게 함
- Inventory UI open 시 `UBG_InventoryViewModel::RefreshAllRenderData`와 `ForceBroadcastAll`을 호출하고 `FInputModeGameAndUI`, mouse cursor on으로 전환
- Inventory UI close 시 widget collapse, `FInputModeGameOnly`, mouse cursor off로 복귀
- C++에서 CommonUI 타입은 직접 참조하지 않음. `PUBG_HotMode.Build.cs` 의존성 추가 없음

자동 검증:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" PUBG_HotModeEditor Win64 Development -Project="D:\Dev\P4\PUBG_HotMode\PUBG_HotMode.uproject" -WaitMutex -LiveCoding
```

결과: 성공. XGE license warning만 발생.

Blueprint 작업 필요:

- `Content/GEONU/Blueprints/Widgets/WBP_Inventory` 생성. Parent Class는 기본 `UserWidget`
- PlayerController Blueprint/Class Defaults에서 `InventoryWidgetClass = WBP_Inventory` 지정
- Enhanced Input에 `IA_Inventory` 생성 후 기본 Mapping Context에 원하는 키로 매핑하고 `InputConfig.InventoryAction`에 지정
- `WBP_Inventory`의 `Event Construct`에서 `Get Owning Player` -> `Cast to BG_PlayerController` -> `GetInventoryViewModel`로 ViewModel을 변수에 저장하고 `OnInventoryItemsChanged`, `OnInventoryWeightChanged`, `OnEquipmentSlotsChanged`, `OnNearbyWorldItemsChanged`, `OnInventoryFailureReceived`에 bind
- Inventory list는 `GetInventoryItems`, nearby list는 `GetNearbyWorldItems`, equipment slots는 `GetEquipmentSlots`, weight는 `GetCurrentWeight`/`GetMaxWeight`만 읽어서 표시
- Pickup 버튼은 row의 `WorldItem`과 `Quantity`로 `RequestPickupWorldItem` 호출. 부분 pickup UI는 수량 입력값을 Quantity로 전달
- Drop 버튼은 row의 `ItemType`, `ItemTag`, 수량 입력값으로 `RequestDropInventoryItem` 호출
- Backpack unequip/drop 버튼은 `RequestDropEquipment(Backpack)` 호출. Helmet, Vest, weapon slot도 같은 API에 슬롯만 바꿔 연결
- 닫기 버튼 또는 ESC UI 경로는 `Get Owning Player` -> `Cast to BG_PlayerController` -> `CloseInventoryUI` 호출

수동 미검증:

- PIE에서 inventory open/close, mouse cursor/input mode 전환 확인
- Widget list rebuild와 failure message 표시 확인
- Dedicated Server + 2 Clients PIE에서 owner-only inventory 정보가 원격 클라이언트 UI에 노출되지 않는지 확인

## 남은 리스크

- Health migration은 기존 PlayerState 의존 코드가 숨어 있으면 컴파일 이후 런타임에서 드러날 수 있음
- AssetManager 설정은 editor asset 생성 절차와 config가 어긋나면 packaged 환경에서 registry load 실패 가능
- Backpack 해제/교체는 Inventory와 Equipment state mutation 순서가 잘못되면 아이템 유실 또는 중복 spawn 위험
- #8의 WorldItem runtime flow는 컴파일 검증만 완료됐고 Dedicated Server + 2 Clients PIE 수동 검증이 아직 필요함
- #9의 ViewModel delegate와 owner-only UI 경계는 컴파일 검증만 완료됐고 Dedicated Server + 2 Clients PIE 수동 검증이 필요함
- #10의 실제 UMG asset authoring과 버튼 binding은 Editor Blueprint 수동 작업이 필요함
- item별 WorldItem BP와 DataTable reimport는 Editor 수동 작업이 필요함
- 기존 PlayerController 숫자키 임시 장착 입력은 `UBG_EquipmentComponent`를 거치지 않고 `ABG_Character::SetWeaponState`를 직접 호출하므로 Equipment state와 Character weapon state가 어긋날 수 있음
- Reload와 ItemUse 취소 조건은 기존 weapon/action state와 이벤트 순서가 달라질 수 있음
- CommonUI 입력 라우팅은 기존 Enhanced Input과 충돌할 수 있으므로 UI task에서 별도 수동 검증 필요
