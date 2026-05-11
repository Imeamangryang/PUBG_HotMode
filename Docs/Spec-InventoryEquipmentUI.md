# 인벤토리 무기/장비 UI 확장 명세

## 목표

Inventory 화면에서 일반 인벤토리, 장착 무기, 방어구, 가방, 투척 선택 상태를 하나의 사용자 경험으로 표현한다.

성공 기준:

- `UBG_InventoryViewModel`이 Inventory, Weapon, Equipment 표시 데이터를 한 화면에서 소비 가능한 형태로 제공
- Weapon 슬롯은 Primary A, Primary B, Pistol, Melee 장착 상태와 active 상태를 표시
- 총기 슬롯은 loaded ammo를 표시하고, reserve ammo와 magazine size는 무기 로직 확정 전까지 더미값 표시를 허용
- 장비 슬롯은 Helmet, Vest, Backpack 상태와 내구도, 레벨, 무게 보너스를 표시
- Throwable 슬롯은 선택된 투척 아이템과 보유 수량을 표시
- Weapon 아이콘은 `EquippedWeaponClass` 기반 Runtime Capture preview를 지원
- Widget은 gameplay state를 직접 변경하지 않고 ViewModel request API만 호출
- Runtime preview는 local UI presentation으로 처리되어 gameplay replication에 영향을 주지 않음

비목표:

- Weapon fire, reload, attachment gameplay 구현
- attachment 장착/해제 시스템 전체 구현
- 스킨/부착물 runtime state 반영
- cosmetic inventory, skin ownership, monetization, persistence
- DataTable/uasset 자동 reimport 도구 작성
- CommonUI 전체 전환
- Unreal MVVM plugin 도입
- Slate 전용 커스텀 위젯 작성

## 정의

- Inventory 화면: `ABG_PlayerController`가 열고 닫는 `WBP_InventoryHUD` 기반 UMG 화면
- General inventory: `UBG_InventoryComponent`에 stack으로 저장되는 Ammo, Heal, Boost, Throwable 수량
- Weapon slots: `UBG_EquipmentComponent`가 관리하는 PrimaryA, PrimaryB, Pistol, Melee
- Equipment slots: Helmet, Vest, Backpack, Throwable selection
- Active weapon: 손에 들고 입력을 받는 `EBG_EquipmentSlot`
- Static icon: `FBG_ItemDataRow::Icon`의 고정 `UTexture2D`
- Runtime preview icon: `SceneCaptureComponent2D`가 `EquippedWeaponClass` preview actor를 `UTextureRenderTarget2D`에 렌더한 로컬 UI용 무기 이미지
- Preview actor: 실제 장착 actor 인스턴스가 아니라 UI preview 전용으로 local spawn한 `EquippedWeaponClass` actor
- Preview transform: Inventory UI preview 구현 시 weapon Blueprint default에 authoring하는 UI 전용 transform. Capture component는 이 transform을 적용한 뒤 고정 side-view 카메라로 렌더
- Preview key: Inventory UI preview 구현 시 추가하는 runtime preview cache 식별자
- UI display-only ammo: reserve ammo와 magazine size처럼 Weapon Logic 확정 전까지 더미값으로 표시 가능한 UI 값

## 현재 저장소 분석

분석 기준: 2026-05-06, `c:\Users\me\source\UE5\PUBG_HotMode`

| 영역 | 현재 상태 | 확장 방향 |
| --- | --- | --- |
| ViewModel 위치 | `ABG_PlayerController`가 `UBG_InventoryViewModel`을 subobject로 소유 | 현 구조 유지 |
| Widget class | `InventoryWidgetClass` 기본값이 `/Game/GEONU/Blueprints/Widgets/WBP_InventoryHUD` | 현 Widget을 확장 |
| Inventory render data | `FBGInventoryItemRenderData`가 stack item 표시 데이터 제공 | 카테고리/정렬/사용 가능 여부 보강 |
| Equipment render data | `FBGEquipmentSlotRenderData`가 slot, item tag, icon, loaded ammo, durability, active 제공 | weapon actor loaded ammo, reserve/magazine dummy, preview key, slot action 보강 |
| Nearby item render data | `FBGNearbyWorldItemRenderData`가 주변 WorldItem 표시 | 현재 범위 유지 |
| Equipment state | `UBG_EquipmentComponent`가 weapon actor ref, active slot, armor durability 제공 | UI는 query만 사용 |
| Item data | `FBG_ItemDataRow`에 static `Icon`, weapon row에 `EquippedWeaponClass` 존재 | Runtime preview는 별도 local capture 시스템 사용 |
| Weapon actor | `ABG_EquippedWeaponBase`가 equipped mesh, socket/attach, loaded ammo runtime state 제공 | Preview actor는 같은 `EquippedWeaponClass`를 local-only로 spawn하고 preview transform은 UI 구현 시 추가 |
| Runtime capture | 현재 없음 | 신규 local UI component 또는 actor 필요 |

주요 결론:

- 기존 ViewModel은 이미 Inventory와 Equipment를 한 화면에 공급할 기본 형태를 가짐
- 현재 `Icon`은 static texture라 장착 Blueprint의 실제 외형을 보여주기에는 부족함
- Runtime capture는 gameplay component가 아니라 local UI presentation 계층으로 분리해야 함
- Weapon preview는 실제 장착 actor를 직접 캡처하지 않고 `EquippedWeaponClass` preview actor를 local-only로 구성해야 함
- 부착물/스킨은 현재 단계에서 없는 것으로 보며, Blueprint에 직접 붙어 있는 static mesh/component는 actor 외형의 일부로만 캡처
- #3 무기 장착 actor lifecycle까지 구현된 현재 상태에서도 장착 상태 표시와 runtime preview 설계는 진행 가능
- loaded ammo는 equipped weapon actor runtime state에서 ViewModel이 읽어 표시
- reserve ammo와 magazine size의 정확한 계산은 Weapon Logic 확정 이후로 보류하고, 현 UI 구현에서는 더미값으로 표시 가능

## 확정 결정

- 기존 `UBG_InventoryViewModel`을 확장하고 별도 `UBG_EquipmentViewModel`은 만들지 않음
- Widget은 `UBG_InventoryViewModel`과 local preview provider만 참조
- Widget은 `UBG_EquipmentComponent`, `UBG_InventoryComponent`, `ABG_EquippedWeaponBase`를 직접 변경하지 않음
- Weapon loaded ammo source of truth는 `ABG_EquippedWeaponBase` runtime state
- `UBG_EquipmentComponent`는 slot/tag/actor lifecycle을 관리하고 최종 구조에서 loaded ammo를 소유하지 않음
- General inventory와 Equipment slot은 분리된 render data 배열로 유지
- Weapon slot, Armor slot, Backpack slot, Throwable slot은 하나의 `FBGEquipmentSlotRenderData` 계약을 확장해 사용
- Runtime weapon preview는 `UTextureRenderTarget2D`를 UI material에 넣어 표시
- Runtime preview는 owning local client에서만 생성
- Runtime preview actor/component는 replication을 사용하지 않음
- Runtime preview는 `CaptureEveryFrame=false`, `CaptureOnMovement=false`로 두고 상태 변경 시에만 capture
- 실제 장착 actor는 runtime preview 캡처 대상으로 직접 사용하지 않음
- Preview actor는 weapon row의 `EquippedWeaponClass`를 사용하고 별도 preview 전용 class는 두지 않음
- Preview transform은 DataTable이 아니라 weapon Blueprint default에 두되, InventoryEquipmentUI 구현 시 추가
- `FBG_ItemDataRow::Icon`은 fallback static icon으로 유지
- Weapon preview key가 없거나 capture 실패 시 static icon으로 fallback
- preview scene 구성 실패, mesh/class 누락, render target 생성 실패는 명시적 error log
- 1차 PreviewKey는 InventoryEquipmentUI 구현 시 `ItemTag + Slot + EquippedWeaponClass` 기반으로 구성
- 현 단계에서 스킨/부착물 runtime state는 PreviewKey와 capture 입력에 포함하지 않음
- `PreviewTransform`, `PreviewIconKey`는 UI preview 전용 계약이며 WeaponSystem 전투 로직 선행조건이 아님
- `MagazineSize`, `ReserveAmmo`의 정확한 무기 로직은 현 단계에서 보류하고 UI에서는 dummy render data를 허용
- UI에서 weapon slot 클릭으로 active weapon 변경을 지원하지 않음
- Inventory UI의 weapon slot drag/drop은 PrimaryA/B 상호 교체와 바닥 drop을 지원

## 기능 명세

### Inventory 화면 구조

화면은 다음 영역을 제공한다.

- 주변 아이템 목록
- 일반 인벤토리 목록
- 장착 무기 슬롯
- 장착 장비 슬롯
- 선택 아이템 상세 정보
- 실패 사유/작업 feedback
- 무게 표시

장착 무기 슬롯:

- Primary A
- Primary B
- Pistol
- Melee

장착 장비 슬롯:

- Helmet
- Vest
- Backpack
- Throwable

### ViewModel render data

`UBG_InventoryViewModel`은 equipment render data를 `TMap<EBG_EquipmentSlot, FBGEquipmentSlotRenderData>`로 보관한다.
Inventory 화면은 슬롯별 고정 위치 Widget을 사용하므로, 각 Widget은 자신의 `EBG_EquipmentSlot`로 `GetEquipmentSlotData(Slot, OutSlotData)`를 호출해 데이터를 얻는다.
`GetEquipmentSlots()`는 전체 slot map 조회용으로 유지한다.

`FBGEquipmentSlotRenderData`는 다음 정보를 제공해야 한다.

- slot enum
- item type
- item tag
- display name
- description
- static icon
- equipped 여부
- active 여부
- loaded ammo
- magazine size
- reserve ammo
- armor durability
- max armor durability
- equipment level
- backpack weight bonus
- throwable quantity
- runtime preview 사용 여부
- preview key
- capture fallback 가능 여부
- display row 존재 여부
- drag 가능 여부
- drop 가능 여부

Weapon slot 규칙:

- `LoadedAmmo`: slot의 `ABG_EquippedWeaponBase` runtime state에서 조회
- `MagazineSize`: Weapon Logic 확정 전까지 dummy 값 허용
- `ReserveAmmo`: Weapon Logic 확정 전까지 dummy 값 허용
- `bActive`: `EquipmentComponent->GetActiveWeaponSlot() == Slot`
- `bUseRuntimePreviewIcon`: weapon이 장착되어 있고 runtime preview provider가 지원 대상이면 true
- `PreviewIconKey`: InventoryEquipmentUI 구현 시 weapon tag, slot, `EquippedWeaponClass`로 구성
- `bCanDrag`: PrimaryA 또는 PrimaryB에 weapon이 장착되어 있으면 true
- `bCanDropToGround`: 장착된 weapon slot이면 true
- `bCanSwapWithPrimarySlot`: PrimaryA/B 상호 drag/drop 가능 여부

Armor slot 규칙:

- `Durability`: `UBG_EquipmentComponent::GetArmorDurability(Slot)`
- `MaxDurability`: armor row의 `MaxDurability`
- `EquipmentLevel`: armor row의 `EquipmentLevel`
- Runtime preview를 사용하지 않음

Backpack slot 규칙:

- `EquipmentLevel`: backpack row의 `EquipmentLevel`
- `BackpackWeightBonus`: backpack row의 `WeightBonus`
- Runtime preview를 사용하지 않음

Throwable slot 규칙:

- `Quantity`: Inventory의 throwable 수량
- Runtime preview를 사용하지 않음

### Runtime weapon icon capture

런타임 preview provider는 다음 책임을 가진다.

- PreviewKey 기준 render target cache 관리
- weapon preview actor 생성과 제거
- `EquippedWeaponClass` actor spawn과 visual component capture
- preview bounds 계산
- SceneCapture2D 카메라/ortho width 설정
- `CaptureScene()` 호출
- render target을 Widget에 반환

권장 타입:

```text
ABG_PlayerController
├── UBG_InventoryViewModel
└── UBG_WeaponIconCaptureComponent
```

또는:

```text
ABG_PlayerController
└── ABG_WeaponIconCaptureActor owned locally
```

1차 구현은 component를 우선한다.

Runtime capture 설정:

- projection: Orthographic
- render target size: 256x128 또는 512x256
- capture source: Final Color 또는 alpha가 필요한 별도 설정
- background: transparent 또는 UI 색상에 맞춘 neutral color
- `CaptureEveryFrame=false`
- `CaptureOnMovement=false`
- collision disabled
- hidden in game world 또는 `ShowOnlyActors`/`ShowOnlyComponents` 사용
- preview actor 위치는 gameplay world와 격리된 위치

카메라 규칙:

- weapon Blueprint default의 preview transform을 preview actor에 적용
- 기본 side direction은 capture provider의 고정 side-view 카메라가 담당
- actor visual bounds center를 바라보도록 카메라 회전
- `OrthoWidth = Max(BoundsSize.Y, BoundsSize.Z) * Padding`
- padding 기본값 1.15
- 무기별 보정은 weapon Blueprint default의 preview transform으로 해결
- 필요 시 Blueprint default로 preview ortho width override를 추가할 수 있음

스킨/부착물 규칙:

- 현 단계에서 스킨/부착물 runtime state는 없는 것으로 간주
- `EquippedWeaponClass` Blueprint에 직접 붙어 있는 static mesh/component는 별도 해석 없이 함께 캡처
- preview key는 `ItemTag`, `Slot`, `EquippedWeaponClass`만 포함

Fallback 규칙:

- render target 생성 실패: static icon 사용
- preview actor spawn 실패: static icon 사용
- mesh/component 없음: static icon 사용
- static icon도 없으면 empty slot placeholder 표시
- 실패는 owning local log에 error 또는 warning으로 남김

### UI request API

ViewModel은 다음 request를 제공해야 한다.

- pickup nearby item
- pickup selected world item
- drop inventory item
- drop equipment slot
- swap PrimaryA/B weapon slots
- select throwable
- clear throwable
- use inventory item
- cancel item use

규칙:

- request는 ViewModel 또는 Character request API를 경유
- Widget에서 component server RPC 직접 호출 금지
- weapon slot click은 active weapon 변경 request를 보내지 않음
- active weapon 변경은 Inventory UI가 아니라 WeaponSystem input action 경로에서 처리
- drag source가 empty slot이면 false 반환과 failure feedback
- PrimaryA/B 외 swap 요청은 false 반환과 failure feedback
- server rejection은 existing inventory failure path로 표시

### 네트워크

- Inventory/Equipment state mutation은 서버 권위
- ViewModel은 local owning client에서 replicated state를 읽어 render data 생성
- Runtime preview는 local-only visual
- Runtime preview render target은 replication 대상이 아님
- Runtime preview와 PreviewKey는 local UI 전용이며 네트워크 state로 복제하지 않음
- public equipment visual은 기존 replicated equipped actor/state로만 표현

### 성능

- Inventory 화면이 닫혀 있으면 runtime capture를 수행하지 않음
- 같은 PreviewKey는 render target cache 재사용
- slot 상태 변경, `EquippedWeaponClass` 변경, 화면 open 시에만 recapture
- 기본 max cached previews는 8개
- capture actor와 render target은 `EndPlay`, controller unpossess, UI close 정책에 맞춰 정리
- 매 프레임 capture 금지

## 수용 기준

- Inventory 화면에서 일반 인벤토리와 장비 슬롯이 동시에 표시됨
- PrimaryA/B/Pistol/Melee 슬롯에 장착 weapon name, active 상태, loaded ammo가 표시됨
- 총기 slot에서 reserve ammo와 magazine size는 더미값으로라도 표시 가능함
- Helmet/Vest 슬롯에서 durability와 level이 표시됨
- Backpack 슬롯에서 level과 weight bonus가 표시됨
- Throwable 슬롯에서 선택 아이템과 보유 수량이 표시됨
- Runtime capture 지원 weapon은 static icon 대신 render target preview를 표시함
- Runtime capture 실패 시 static icon fallback이 표시됨
- Inventory UI 닫힘 상태에서 preview recapture가 반복되지 않음
- Widget command가 ViewModel API만 호출함
- weapon slot 클릭으로 active weapon이 바뀌지 않음
- PrimaryA/B weapon slot을 서로 drag/drop하면 두 슬롯의 장착 weapon이 교체됨
- 장착 weapon slot을 바닥/drop 영역으로 drag/drop하면 해당 weapon이 world item으로 drop됨
- Dedicated Server + 2 Clients PIE에서 runtime preview가 gameplay replication에 영향을 주지 않음

## 프로젝트 맥락

관련 문서:

- `Docs/Spec-InventorySystem.md`
- `Docs/Plan-InventorySystem.md`
- `Docs/Spec-WeaponSystem.md`
- `Docs/Plan-WeaponSystem.md`

관련 C++:

- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.h`
- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemDataRow.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.h`
- `Source/PUBG_HotMode/WON/Public/Player/BG_PlayerController.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_PlayerController.cpp`

관련 Content:

- `Content/GEONU/Blueprints/Widgets/WBP_InventoryHUD.uasset`
- `Content/GEONU/Data/Source/Items_Weapon.csv`
- `Content/GEONU/Data/Source/Items_Armor.csv`
- `Content/GEONU/Data/Source/Items_Backpack.csv`
- `Content/GEONU/Data/Source/Items_Throwable.csv`
- `Content/GEONU/Data/DT_Items_Weapon.uasset`
- `Content/GEONU/Data/DT_Items_Armor.uasset`
- `Content/GEONU/Data/DT_Items_Backpack.uasset`
- `Content/GEONU/Data/DT_Items_Throwable.uasset`

검증 명령:

```powershell
& "<UE_ROOT>\Engine\Build\BatchFiles\Build.bat" PUBG_HotModeEditor Win64 Development -Project="c:\Users\me\source\UE5\PUBG_HotMode\PUBG_HotMode.uproject" -WaitMutex
```

수동 검증:

- Dedicated Server + 2 Clients PIE
- owner client: inventory open, weapon/equipment display, runtime preview, drag/drop command
- remote client: runtime preview가 다른 플레이어 gameplay state를 변경하지 않음

## 열린 질문

- 없음
