# 인벤토리 무기/장비 UI 확장 구현 계획

## 문서 역할

- 원본 요구사항과 UI 표시 계약은 `Docs/Spec-InventoryEquipmentUI.md`
- 이 문서는 Spec을 구현 가능한 순서, 의존성, 검증 기준으로 분해
- 기존 Inventory/Weapon 시스템의 gameplay 구현은 이 Plan의 범위가 아니며 필요한 연결점만 명시
- 구현 중 outcome이 바뀌는 판단은 Spec을 먼저 갱신한 뒤 Plan을 갱신

## 스펙 검토 결과

- 구현을 차단하는 미해결 질문 없음
- 스킨/부착물은 현 단계에서 없는 것으로 간주하고 preview 입력과 PreviewKey에 포함하지 않음
- Runtime capture는 local UI presentation으로 분리하고 실제 장착 actor 인스턴스를 직접 캡처하지 않는 방향으로 확정
- Preview actor는 weapon row의 `EquippedWeaponClass`를 사용하고 Preview Transform은 InventoryEquipmentUI 구현 시 weapon Blueprint default에 추가
- `PreviewTransform`, `PreviewIconKey`는 UI preview 전용 계약으로 취급
- Weapon Logic에 속하는 정확한 `MagazineSize`, `ReserveAmmo` 계산은 보류하고 현 UI에서는 더미값 표시를 허용
- Inventory UI에서 weapon slot 클릭으로 active weapon 변경을 지원하지 않음
- Inventory UI drag/drop은 PrimaryA/B 교체와 장착 weapon 바닥 drop을 지원
- local multiplayer와 split screen은 구현 계획이 없으므로 preview cache owner 분리 요구에서 제외

계획에서 고정한 세부 결정:

| 항목 | 결정 |
| --- | --- |
| ViewModel | 기존 `UBG_InventoryViewModel` 확장 |
| Preview provider | 신규 local `UBG_WeaponIconCaptureComponent` 우선 |
| Preview actor | weapon row의 `EquippedWeaponClass` 사용 |
| Preview transform | InventoryEquipmentUI 구현 시 weapon Blueprint default에서 authoring |
| UI texture | `UTextureRenderTarget2D`를 UI material texture parameter로 표시 |
| Fallback | Runtime preview 실패 시 `FBG_ItemDataRow::Icon` 사용 |
| Network | Runtime capture는 local-only, replication 없음 |
| Cache | PreviewKey 기준 render target cache |
| Widget command | ViewModel API만 호출. weapon slot click activate 없음 |

## 구현 분석

### 핵심 의존성

| 축 | 먼저 고정해야 하는 이유 | 후속 작업 |
| --- | --- | --- |
| Render data 계약 | Widget과 C++ ViewModel이 같은 구조를 사용해야 함 | Widget binding, preview 요청 |
| Preview key | capture cache와 UI material update의 기준 | Runtime capture component |
| Preview provider | render target 생성과 SceneCapture lifecycle 담당 | Widget 이미지 표시 |
| Weapon runtime state | loaded ammo source of truth | ViewModel loaded ammo 표시 |
| Equipment drag/drop command API | UI에서 PrimaryA/B swap과 equipment drop을 ViewModel 경유로 호출 | Drag/drop |
| Widget binding | 실제 UI 표시와 수동 검증 대상 | 최종 PIE 검증 |

### 위험 우선순위

- `FBGEquipmentSlotRenderData` 변경은 Blueprint pin invalidation을 일으킬 수 있으므로 Widget 재컴파일 필요
- `UTextureRenderTarget2D`는 static `UTexture2D`와 다르게 soft pointer로 보관하면 안 되므로 runtime provider와 Widget 연결을 분리해야 함
- SceneCapture가 매 프레임 켜져 있으면 UI 열기만으로 성능 비용이 커짐
- preview actor가 실제 equipped actor를 재사용하면 owner/socket/gameplay hook과 충돌 가능
- `EquippedWeaponClass` Blueprint가 gameplay 초기화에 강하게 의존하면 local preview spawn에서 실패할 수 있음

### 구현 경계

- C++ 신규/수정 중심 위치: `Source/PUBG_HotMode/GEONU/UI`
- gameplay state source는 `UBG_InventoryComponent`, `UBG_EquipmentComponent`, item registry 유지
- Content 작업은 `WBP_InventoryHUD`, UI material, Inventory UI preview용 weapon Blueprint Preview Transform authoring에 한정
- 무기 전투, reload, attachment gameplay 구현은 별도 WeaponSystem 작업으로 유지
- null/invalid state는 silent fail 없이 log

## 기술 설계 요약

### 런타임 데이터 흐름

```text
Inventory UI open
  -> ABG_PlayerController::OpenInventoryUI
  -> UBG_InventoryViewModel::RefreshAllRenderData
  -> Fixed slot Widget calls GetEquipmentSlotData(Slot)
  -> Widget asks WeaponIconCaptureComponent for PreviewIconKey
  -> component returns RenderTarget or nullptr
  -> Widget falls back to static Icon when needed
```

```text
Equipment changed
  -> UBG_EquipmentComponent::OnEquipmentChanged
  -> UBG_InventoryViewModel::HandleEquipmentChanged
  -> EquipmentSlots map rebuilt
  -> OnEquipmentSlotsChanged broadcast
  -> Widget rebuilds slot widgets
  -> changed PreviewIconKey recaptured
```

```text
Weapon slot drag/drop command
  -> Widget drag/drop
  -> UBG_InventoryViewModel swap/drop request
  -> Equipment request API
  -> Server mutation
  -> Replicated state
  -> ViewModel refresh
```

### 주요 인터페이스 초안

`UBG_InventoryViewModel` equipment slot 조회:

```cpp
TMap<EBG_EquipmentSlot, FBGEquipmentSlotRenderData> GetEquipmentSlots() const;
bool GetEquipmentSlotData(EBG_EquipmentSlot Slot, FBGEquipmentSlotRenderData& OutSlotData) const;
```

- Inventory UI는 고정 위치 slot Widget을 사용하므로 `GetEquipmentSlotData`를 기본 binding API로 사용
- `GetEquipmentSlots()`는 전체 slot map 조회와 디버그/검증용 계약

`FBGEquipmentSlotRenderData` 확장:

```cpp
int32 MagazineSize;
int32 ReserveAmmo;
float MaxDurability;
int32 EquipmentLevel;
float BackpackWeightBonus;
bool bUseRuntimePreviewIcon;
FName PreviewIconKey;
bool bCanDrag;
bool bCanDropToGround;
bool bCanSwapWithPrimarySlot;
```

규칙:

- `MagazineSize`, `ReserveAmmo`는 Weapon Logic 확정 전까지 dummy display value로 채울 수 있음
- `PreviewIconKey`는 InventoryEquipmentUI runtime preview cache 전용 key이며 WeaponSystem 로직에 전달하지 않음

`UBG_InventoryViewModel` command 확장:

```cpp
bool SwapWeaponSlots(EBG_EquipmentSlot SourceSlot, EBG_EquipmentSlot TargetSlot);
bool DropEquipment(EBG_EquipmentSlot EquipmentSlot);
bool SelectThrowable(FGameplayTag ThrowableItemTag);
bool ClearThrowable();
```

규칙:

- `SwapWeaponSlots`는 PrimaryA/B 상호 교체만 허용
- `DropEquipment`는 장착 weapon slot을 바닥/drop 영역으로 drag/drop할 때 사용
- weapon slot click은 active weapon 변경 API를 호출하지 않음
- active weapon 변경과 unequip은 WeaponSystem input action 경로에서 처리

신규 preview provider 초안:

```cpp
UCLASS(ClassGroup="UI", meta=(BlueprintSpawnableComponent))
class UBG_WeaponIconCaptureComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Inventory UI|Weapon Preview")
    UTextureRenderTarget2D* GetOrCreateWeaponPreview(const FBGEquipmentSlotRenderData& SlotData);

    UFUNCTION(BlueprintCallable, Category="Inventory UI|Weapon Preview")
    void InvalidateWeaponPreview(FName PreviewIconKey);

    UFUNCTION(BlueprintCallable, Category="Inventory UI|Weapon Preview")
    void ClearPreviewCache();
};
```

`ABG_EquippedWeaponBase` preview authoring 계약:

```cpp
FTransform GetPreviewTransform() const;
```

- `PreviewTransform`은 `EditDefaultsOnly`로 weapon Blueprint default에서 authoring
- capture component는 preview actor spawn 후 해당 transform을 적용하고 actor visual bounds로 side-view framing 계산
- DataTable에는 preview transform을 추가하지 않음
- 이 계약은 InventoryEquipmentUI 구현 범위에서 추가하며 WeaponFire/Reload 로직 선행조건이 아님

### 의존성 그래프

```text
#1 Render data 계약 확장
  -> #2 ViewModel equipment 세부 데이터 계산
  -> #3 ViewModel drag/drop command API
  -> #4 Runtime preview provider
    -> #5 Preview Data와 Weapon Mesh 구성
      -> #6 Widget runtime preview 연결
        -> #7 Dedicated Server UI 검증
          -> #8 문서 갱신과 후속 정리
```

### 병렬 가능성

| 시점 | 병렬 가능한 작업 | 조건 |
| --- | --- | --- |
| #1 완료 후 | #3 drag/drop command API와 #4 preview provider skeleton | `PreviewIconKey` 필드명 고정 |
| WeaponSystem loaded ammo API 사용 가능 후 | #2 loaded ammo 표시 고도화 | reserve/magazine 정확도는 이후 Weapon Logic 작업 |
| #2 완료 후 | Widget 표시 초안 | Equipment request API와 weapon 표시 데이터 사용 가능 |
| #4 완료 후 | UI material 제작과 Widget binding | RenderTarget 반환 계약 고정 |

## 공통 검증

Build:

```powershell
& "<UE_ROOT>\Engine\Build\BatchFiles\Build.bat" PUBG_HotModeEditor Win64 Development -Project="c:\Users\me\source\UE5\PUBG_HotMode\PUBG_HotMode.uproject" -WaitMutex
```

수동 검증:

- Dedicated Server + 2 Clients PIE
- owner client: inventory open/close, weapon/equipment display, runtime preview display, drag/drop command
- remote client: runtime preview가 gameplay replication/state를 변경하지 않음
- UI close 상태에서 preview recapture 반복 없음

## 구현 단위

### #1 Render Data 계약 확장

Description: `FBGEquipmentSlotRenderData`에 weapon/equipment 상세 표시와 runtime preview 식별자를 추가한다. Blueprint 사용성을 위해 필드는 `BlueprintReadOnly`로 노출하고 기본값을 안전하게 둔다.

Acceptance:

- Weapon slot render data에 `MagazineSize`, `ReserveAmmo`, `bUseRuntimePreviewIcon`, `PreviewIconKey`, drag/drop flag가 추가됨
- `MagazineSize`, `ReserveAmmo`는 1차 구현에서 dummy value 허용
- Armor/Backpack render data에 `MaxDurability`, `EquipmentLevel`, `BackpackWeightBonus`가 추가됨
- empty slot은 모든 numeric 값이 0이고 preview key가 none
- 기존 `Icon` 필드는 static fallback으로 유지됨

Verification:

- Build 성공
- Editor에서 `FBGEquipmentSlotRenderData` Blueprint pin 갱신 확인

Dependencies: 없음

Files likely touched:

- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.h`

Scope: S

### #2 ViewModel Equipment 세부 데이터 계산

Description: `UBG_InventoryViewModel::BuildEquipmentSlotRenderData`가 typed item row를 활용해 loaded ammo, armor durability max, equipment level, backpack bonus, throwable quantity를 계산한다. 정확한 reserve ammo와 magazine size는 Weapon Logic 확정 전까지 더미값으로 둔다.

Acceptance:

- Equipped weapon actor runtime state 기준으로 loaded ammo가 계산됨
- reserve ammo와 magazine size는 dummy value로 표시 가능
- Armor row의 `MaxDurability`, `EquipmentLevel`이 표시 데이터에 반영됨
- Backpack row의 `EquipmentLevel`, `WeightBonus`가 표시 데이터에 반영됨
- Throwable quantity는 기존 Inventory quantity 기준으로 유지됨
- row lookup 실패 시 fallback display name은 유지되고 error log는 기존 정책을 따름

Verification:

- Build 성공
- PIE에서 M416/P92 등 weapon slot의 loaded ammo와 reserve/magazine dummy 표시 수동 확인
- Helmet/Vest/Backpack 장착 시 level/durability/bonus 값 수동 확인

Dependencies: #1 Render Data 계약 확장

Files likely touched:

- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.h`
- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.cpp`

Scope: M

### #3 ViewModel Drag/Drop Command API

Description: Widget이 장착 슬롯 drag/drop 상호작용을 ViewModel 경유로만 수행하도록 PrimaryA/B swap, equipment drop, throwable select/clear API를 정리한다. Weapon slot 클릭으로 active weapon을 바꾸는 기능은 만들지 않는다.

Acceptance:

- `SwapWeaponSlots(SourceSlot, TargetSlot)`가 PrimaryA/B 상호 교체만 허용하고 component request로 전달함
- `DropEquipment(EBG_EquipmentSlot)`가 장착 weapon slot drag/drop을 component request로 전달함
- `SelectThrowable(FGameplayTag)`와 `ClearThrowable()`이 EquipmentComponent request로 전달됨
- invalid slot, empty slot, PrimaryA/B 외 swap, invalid tag는 false 반환과 명시적 log/failure feedback
- weapon slot click은 `ActivateWeapon`/`Unequip` request를 호출하지 않음
- Widget에서 EquipmentComponent 직접 mutation 없이 연결 가능

Verification:

- Build 성공
- PIE에서 PrimaryA/B drag/drop swap 확인
- PIE에서 weapon slot을 drop 영역으로 drag/drop해 world item drop 확인
- weapon slot 클릭만으로 active weapon이 변경되지 않는지 확인
- empty slot drag/drop 실패 feedback 확인

Dependencies: #1 Render Data 계약 확장

Files likely touched:

- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.h`
- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.cpp`
- 필요 시 `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.h`
- 필요 시 `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.cpp`

Scope: M

### Checkpoint A: ViewModel Contract

- #1, #2, #3 완료 후 Widget 없이 C++ build와 PIE log로 ViewModel 계약을 먼저 검증
- `GetEquipmentSlotData(Slot)`만으로 고정 위치 Inventory UI가 필요한 weapon/equipment 표시 데이터를 얻는지 확인
- Widget 직접 component mutation 없이 drag/drop command API가 충분한지 확인

### #4 Runtime Weapon Icon Capture Component

Description: local PlayerController에 붙는 `UBG_WeaponIconCaptureComponent`를 추가해 PreviewKey별 render target을 생성, 캐시, 갱신한다. Preview actor는 weapon row의 `EquippedWeaponClass`를 사용한다.

Acceptance:

- component는 local controller에서만 capture를 수행함
- `SceneCaptureComponent2D`, preview root, `EquippedWeaponClass` preview actor, light, render target을 관리함
- `CaptureEveryFrame=false`, `CaptureOnMovement=false`
- `GetOrCreateWeaponPreview`가 cache hit 시 기존 render target을 반환함
- capture 실패 시 nullptr 반환과 log
- UI close 또는 `EndPlay`에서 preview cache와 spawned preview actor를 정리함

Verification:

- Build 성공
- PIE local client에서 weapon slot preview render target 생성 확인
- UI 닫힘 상태에서 반복 capture가 발생하지 않는지 log 확인

Dependencies: #1 Render Data 계약 확장

Files likely touched:

- `Source/PUBG_HotMode/GEONU/UI/BG_WeaponIconCaptureComponent.h`
- `Source/PUBG_HotMode/GEONU/UI/BG_WeaponIconCaptureComponent.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_PlayerController.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_PlayerController.cpp`

Scope: M

### #5 Preview Data와 Weapon Mesh 구성

Description: runtime preview가 `EquippedWeaponClass`와 Inventory UI preview용 weapon Blueprint default Preview Transform을 이용해 preview actor를 구성하도록 한다. 스킨/부착물 runtime state는 현 단계에서 고려하지 않는다.

Acceptance:

- 1차 구현은 `EquippedWeaponClass`를 로드해 preview 전용 actor를 구성함
- preview actor는 owner/instigator/gameplay init 없이 local visual로만 사용됨
- collision과 overlap은 disabled
- weapon Blueprint default의 Preview Transform이 preview actor에 적용됨
- Preview Transform은 InventoryEquipmentUI 구현 중 추가하며 WeaponSystem 전투 로직과 분리됨
- actor visual bounds 기준으로 side-view camera와 ortho width가 자동 계산됨
- Blueprint에 직접 붙어 있는 static mesh/component는 별도 attachment 정보 없이 함께 캡처됨

Verification:

- Build 성공
- M416/P92/Pan 등 서로 다른 mesh 비율의 preview framing 확인
- weapon Blueprint Preview Transform 변경 후 preview framing 반영 확인
- mesh/class 누락 시 static icon fallback 확인

Dependencies: #4 Runtime Weapon Icon Capture Component

Files likely touched:

- `Source/PUBG_HotMode/GEONU/UI/BG_WeaponIconCaptureComponent.h`
- `Source/PUBG_HotMode/GEONU/UI/BG_WeaponIconCaptureComponent.cpp`
- `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.cpp`

Scope: M

### #6 Widget 표시 연결

Description: `WBP_InventoryHUD`가 확장된 render data를 읽고, weapon slot은 runtime preview render target 또는 static icon fallback을 표시한다.

Acceptance:

- 일반 인벤토리, 주변 아이템, weapon slots, equipment slots가 같은 화면에서 표시됨
- weapon slot image는 preview provider render target을 우선 사용함
- preview provider가 nullptr를 반환하면 `Icon` static texture를 표시함
- loaded ammo, reserve/magazine dummy, active 상태, durability, level, backpack bonus가 UI에 표시됨
- weapon slot click은 active weapon 변경을 수행하지 않음
- PrimaryA/B weapon slot drag/drop으로 서로 교체됨
- weapon slot을 drop 영역으로 drag/drop하면 바닥 drop request가 수행됨
- drag/drop command는 ViewModel API만 호출함
- UI element text가 compact slot 영역에서 겹치지 않음

Verification:

- PIE에서 inventory open/close와 list rebuild 수동 확인
- weapon pickup/drop/swap 후 slot UI 갱신 확인
- weapon slot 클릭만으로 active weapon이 바뀌지 않는지 확인
- preview 실패 fallback 확인

Dependencies: #2 ViewModel Equipment 세부 데이터 계산, #3 ViewModel Drag/Drop Command API, #4 Runtime Weapon Icon Capture Component

Files likely touched:

- `Content/GEONU/Blueprints/Widgets/WBP_InventoryHUD.uasset`
- 필요 시 `Content/GEONU/Blueprints/Widgets/MI_WeaponPreviewIcon.uasset`

Scope: M

### Checkpoint B: Runtime Preview UI

- #4, #5, #6 완료 후 weapon preview가 static icon fallback과 함께 안정적으로 표시되는지 확인
- `CaptureEveryFrame`이 꺼져 있고 상태 변경 시에만 recapture되는지 확인
- UI close/open 반복 시 render target 누수나 stale preview가 없는지 확인

### #7 Dedicated Server UI 검증

Description: Dedicated Server + 2 Clients 환경에서 runtime preview local-only 경계, remote visual 경계, drag/drop command를 검증한다.

Acceptance:

- owner client는 자기 inventory quantity, equipped weapon actor loaded ammo, reserve/magazine dummy, durability를 봄
- remote client의 gameplay state가 owner client runtime preview 생성으로 변하지 않음
- remote client는 public equipped weapon visual만 봄
- owner client의 PrimaryA/B swap과 equipment drop command가 서버 검증 후 UI에 반영됨
- weapon slot click은 active weapon 변경 request를 보내지 않음
- failure reason은 requesting owning client에만 표시됨

Verification:

- Dedicated Server + 2 Clients PIE
- weapon pickup, ammo pickup, armor/backpack pickup, throwable pickup, PrimaryA/B swap, drop 수동 체크리스트 수행
- active weapon 변경과 unequip은 Inventory UI가 아닌 WeaponSystem input action 수동 체크리스트에서 확인

Dependencies: #6 Widget 표시 연결

Files likely touched:

- 필요 시 검증 기록 문서

Scope: S

### #8 문서 갱신과 후속 정리

Description: 구현 결과와 검증 결과를 Spec/Plan에 반영하고, UI preview용 Preview Transform authoring 및 drag/drop 동작 결과를 갱신한다.

Acceptance:

- 완료된 task별 검증 기록이 Plan에 추가됨
- 변경된 preview key 구성이나 Widget binding 방식이 Spec에 반영됨
- 미구현 skin/attachment runtime preview 확장 항목은 현재 범위 밖으로 유지됨

Verification:

- `p4 status`에서 의도한 C++/Content/Docs 변경만 확인
- 문서 내 파일 경로와 구현 파일 경로 일치 확인

Dependencies: #7 Dedicated Server UI 검증

Files likely touched:

- `Docs/Spec-InventoryEquipmentUI.md`
- `Docs/Plan-InventoryEquipmentUI.md`

Scope: XS

## 테스트 체크리스트

### ViewModel

- [ ] character binding 후 `GetEquipmentSlots()` map에 Weapon, Throwable, Helmet, Vest, Backpack slot 모두 포함
- [ ] `GetEquipmentSlotData(Slot)`가 고정 slot Widget용 데이터를 반환
- [ ] empty slot은 placeholder 데이터
- [ ] weapon slot에 equipped actor `LoadedAmmo` 표시
- [ ] `MagazineSize`, `ReserveAmmo` dummy 표시
- [ ] active slot flag 갱신
- [ ] PrimaryA/B weapon slot drag flag 갱신
- [ ] 장착 weapon slot drop flag 갱신
- [ ] armor slot에 durability/max/level 표시
- [ ] backpack slot에 level/weight bonus 표시
- [ ] throwable slot에 quantity 표시

### Runtime Preview

- [ ] PreviewKey가 weapon slot별로 안정적으로 생성
- [ ] 같은 PreviewKey는 cache hit
- [ ] equipment 변경 시 관련 PreviewKey invalidation
- [ ] render target 생성 성공
- [ ] `EquippedWeaponClass` preview actor 사용
- [ ] weapon Blueprint Preview Transform 적용
- [ ] mesh/class 누락 시 static icon fallback
- [ ] UI close 상태에서 recapture 없음
- [ ] `EndPlay`에서 spawned preview actor 정리

### Widget

- [ ] Inventory open 시 전체 데이터 refresh
- [ ] 일반 인벤토리와 장비 슬롯 동시 표시
- [ ] weapon slot runtime preview 표시
- [ ] static icon fallback 표시
- [ ] active weapon visual state 표시
- [ ] ammo text가 slot 영역에서 겹치지 않음
- [ ] durability/level/bonus text가 slot 영역에서 겹치지 않음
- [ ] weapon slot 클릭으로 active weapon이 바뀌지 않음
- [ ] PrimaryA/B drag/drop으로 두 슬롯이 교체
- [ ] weapon slot을 drop 영역으로 drag/drop하면 world item drop
- [ ] drag/drop command가 ViewModel API만 호출

### Network

- [ ] owner client만 inventory quantity 확인
- [ ] owner client reserve ammo dummy 표시 확인
- [ ] owner client만 armor durability 확인
- [ ] remote client는 public equipped weapon visual만 확인
- [ ] failure reason은 요청 client에만 표시
- [ ] PrimaryA/B swap 결과가 owner UI와 public equipped visual에 반영

## 남은 리스크

- Widget Blueprint pin 갱신 후 수동 rebind 필요 가능
- Runtime preview actor가 equipped weapon Blueprint의 gameplay 초기화에 의존하면 preview 구성 실패 가능
- transparent render target alpha 설정은 프로젝트 UI material 정책에 따라 별도 조정 필요
