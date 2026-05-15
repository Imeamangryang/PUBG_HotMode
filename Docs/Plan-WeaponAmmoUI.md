# Plan - Weapon Ammo UI

## 기준

- 원본 Spec: `Docs/Spec-WeaponAmmoUI.md`
- Spec 검토 결과: 구현 차단 이슈 없음
- Spec 정리 사항: `PUBG.InfAmmo true` 시 HealthBar 오른쪽은 `∞`, `ReserveAmmo` 데이터 값은 실제 inventory 수량 유지
- 구현 원칙: 기존 `UBG_HealthViewModel`, `UBG_InventoryViewModel`, `UBG_EquipmentComponent`, `UBG_InventoryComponent`, `UBG_WeaponFireComponent`, `ABG_EquippedWeaponBase` 계약 재사용
- 구현 경계: gameplay mutation 없음, UI 표시 데이터와 Widget 표시만 확장

## 기술 설계

HUD 데이터 흐름:

```text
ABG_PlayerController
  -> UBG_HealthViewModel::NotifyPossessedCharacterReady
  -> bind Health/Inventory/Equipment/WeaponFire components
  -> Build FBGActiveWeaponAmmoRenderData from active weapon
  -> OnActiveWeaponAmmoChanged
  -> WBP_GameHUD or WBP_HealthGauge text update
```

Inventory 장착 슬롯 데이터 흐름:

```text
UBG_InventoryViewModel::RefreshEquipmentRenderData
  -> BuildEquipmentSlotRenderData(Slot)
  -> equipped weapon actor loaded/capacity
  -> weapon row AmmoItemTag/MagazineSize fallback
  -> inventory ammo quantity
  -> WBP_EquippedWeaponSlot displays LoadedAmmo/MagazineSize
```

핵심 계약:

- HUD `ReserveAmmo`는 항상 `InventoryComponent->GetQuantity(Ammo, AmmoItemTag)` 기준
- HUD `bUsesInfiniteDebugAmmo`는 active weapon actor의 `UsesInfiniteDebugAmmo()` 기준
- HealthBar 오른쪽 text는 `bUsesInfiniteDebugAmmo ? "∞" : ReserveAmmo`
- `UBG_WeaponFireComponent::OnAmmoChanged` signature는 유지
- `UBG_HealthViewModel`은 `OnAmmoChanged`, `OnInventoryChanged`, `OnInventoryItemQuantityChanged`, `OnEquipmentChanged`, `OnActiveWeaponSlotChanged`를 모두 ammo 표시 갱신 트리거로 사용
- Inventory slot ammo text는 무한탄 여부와 무관하게 `LoadedAmmo/MagazineSize`만 표시

의존성:

```text
Task 1 HealthViewModel ammo contract
  -> Task 2 HealthViewModel binding/calculation
  -> Task 4 HUD Widget binding
  -> Task 6 Integration verification

Task 3 Inventory slot ammo render data
  -> Task 5 Inventory Widget binding
  -> Task 6 Integration verification

Task 1,2,3
  -> Task 7 Final build/test/manual verification
```

체크포인트:

- Checkpoint A after Task 3: C++ ViewModel 데이터 계약 완료
- Checkpoint B after Task 5: Widget 표시 연결 완료
- Checkpoint C after Task 7: Dedicated Server PIE 수용 기준 확인

## Task 1: HUD Ammo Render Data 계약 추가

Description: `UBG_HealthViewModel`에 활성 무기 탄약 표시용 `FBGActiveWeaponAmmoRenderData`, delegate, getter를 추가한다.

Acceptance:

- `FBGActiveWeaponAmmoRenderData`가 BlueprintType으로 추가됨
- `bHasActiveGun`, `ActiveWeaponSlot`, `ActiveWeaponItemTag`, `AmmoItemTag`, `LoadedAmmo`, `MagazineSize`, `ReserveAmmo`, `bUsesInfiniteDebugAmmo` 노출
- `OnActiveWeaponAmmoChanged(FBGActiveWeaponAmmoRenderData)` delegate 추가
- `GetActiveWeaponAmmoData()` BlueprintPure 추가
- `ForceUpdateAllAttributes()`가 ammo delegate도 broadcast
- default render data는 active gun false, slot none, quantity 0, infinite false

Verification:

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.UI
```

Dependencies: none

Files likely touched:

- `Source/PUBG_HotMode/GEONU/UI/BG_HealthViewModel.h`
- `Source/PUBG_HotMode/GEONU/UI/BG_HealthViewModel.cpp`
- `Source/PUBG_HotMode/GEONU/UI/Tests/BG_HealthViewModelAmmoTests.cpp`

Scope: S

## Task 2: HUD Ammo Binding과 계산 구현

Description: `UBG_HealthViewModel`이 possessed character의 inventory, equipment, weapon fire component에 bind하고 active weapon 기준 ammo data를 계산한다.

Acceptance:

- possession ready 시 Health/Inventory/Equipment/WeaponFire component bind
- possession cleared/end play 시 모든 delegate unbind
- active slot none, invalid weapon tag, invalid ammo tag, weapon row 없음이면 `bHasActiveGun=false`
- active weapon actor가 있으면 loaded ammo, magazine size, ammo tag, infinite flag를 actor runtime state에서 읽음
- active weapon actor가 없으면 equipment loaded ammo와 weapon row magazine/ammo tag fallback 사용
- reserve ammo는 inventory quantity 직접 조회
- `bUsesInfiniteDebugAmmo=true`여도 `ReserveAmmo` 값은 실제 inventory 수량 유지
- ammo data가 변경된 경우에만 `OnActiveWeaponAmmoChanged` broadcast
- `OnAmmoChanged`, inventory quantity 변경, equipment 변경, active slot 변경마다 ammo data refresh
- component 누락은 명시적 error log 후 safe default

Verification:

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.UI
.\RunTests.ps1 PUBG_HotMode.GEONU.Combat
```

Dependencies: Task 1

Files likely touched:

- `Source/PUBG_HotMode/GEONU/UI/BG_HealthViewModel.h`
- `Source/PUBG_HotMode/GEONU/UI/BG_HealthViewModel.cpp`
- `Source/PUBG_HotMode/GEONU/UI/Tests/BG_HealthViewModelAmmoTests.cpp`

Scope: M

## Task 3: Inventory 장착 슬롯 Ammo 데이터 채우기

Description: `UBG_InventoryViewModel::BuildEquipmentSlotRenderData`가 weapon slot의 `LoadedAmmo`, `MagazineSize`, `ReserveAmmo`를 실제 데이터로 채운다.

Acceptance:

- PrimaryA/PrimaryB/Pistol weapon slot은 `LoadedAmmo`를 equipped actor에서 우선 조회
- weapon actor가 없으면 `EquipmentComponent->GetLoadedAmmo(Slot)` fallback
- `MagazineSize`는 equipped actor capacity 우선, weapon row `MagazineSize` fallback
- `ReserveAmmo`는 weapon row 또는 actor ammo tag 기준 inventory ammo quantity
- melee slot은 ammo item tag invalid 또는 magazine size 0으로 ammo text 숨김 가능 상태
- empty slot numeric 값 0 유지
- row lookup 실패 시 fallback display data 유지와 error log

Verification:

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.UI
```

Dependencies: none

Files likely touched:

- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.cpp`
- `Source/PUBG_HotMode/GEONU/UI/Tests/BG_InventoryViewModelRenderDataTests.cpp`

Scope: S

## Checkpoint A: ViewModel Contract

- `FBGActiveWeaponAmmoRenderData` Blueprint 노출 확인
- `GetActiveWeaponAmmoData()` default/unbound 값 확인
- `FBGEquipmentSlotRenderData` weapon ammo fields가 실제 값으로 채워지는지 자동화 테스트 또는 PIE log로 확인

## Task 4: HealthBar Ammo Widget 연결

Description: `WBP_GameHUD` 또는 `WBP_HealthGauge`에 ammo text 2개를 배치하고 `UBG_HealthViewModel`의 ammo delegate에 연결한다.

Implementation note:

- Native `UUserWidget` subclass로 HealthGauge layout을 고정하지 않음
- `WBP_HealthGauge`는 `UserWidget` parent 유지
- text 배치, font, visibility, delegate bind는 Blueprint graph에서 수동 구성
- 자동 Python BP graph 생성은 pin/node variant 위험이 있어 사용하지 않음

Acceptance:

- HealthBar 중앙 text가 `LoadedAmmo` 표시
- HealthBar 중앙 오른쪽 text가 `bUsesInfiniteDebugAmmo=false`이면 `ReserveAmmo` 표시
- HealthBar 중앙 오른쪽 text가 `bUsesInfiniteDebugAmmo=true`이면 `∞` 표시
- `bHasActiveGun=false`이면 두 text 숨김
- Construct 시 `GetHUDViewModel()->GetActiveWeaponAmmoData()`로 초기값 적용
- Construct 시 `OnActiveWeaponAmmoChanged` delegate bind
- text visibility는 gameplay input hit test를 막지 않음
- text가 HealthBar와 주변 UI를 가리지 않음

Verification:

- Editor에서 `WBP_GameHUD` compile
- Editor에서 `WBP_HealthGauge` compile if touched
- PIE에서 weapon 없음, M416, Kar98k, `PUBG.InfAmmo true/false` 표시 수동 확인

Dependencies: Task 1, Task 2

Files likely touched:

- `Content/GEONU/Blueprints/Widgets/Components/WBP_HealthGauge.uasset`

Scope: M

## Task 5: Inventory Weapon Slot Widget 연결

Description: `WBP_EquippedWeaponSlot` 또는 `WBP_InventoryHUD`에서 weapon slot ammo text를 `LoadedAmmo/MagazineSize` 형식으로 표시한다.

Acceptance:

- PrimaryA/PrimaryB/Pistol 슬롯에 weapon 장착 시 `<LoadedAmmo>/<MagazineSize>` 표시
- empty weapon slot은 ammo text 숨김
- melee slot은 ammo text 숨김
- `MagazineSize <= 0`이면 ammo text 숨김
- 발사 후 Inventory 화면 refresh 시 loaded ammo 감소 표시
- 장전 후 Inventory 화면 refresh 시 loaded ammo 증가 표시
- text가 weapon preview/icon/slot label과 겹치지 않음

Verification:

- Editor에서 `WBP_EquippedWeaponSlot` compile
- Editor에서 `WBP_InventoryHUD` compile if touched
- PIE에서 Inventory open 상태와 reopen 상태의 ammo text 확인

Dependencies: Task 3

Files likely touched:

- `Content/GEONU/Blueprints/Widgets/Components/WBP_EquippedWeaponSlot.uasset`
- `Content/GEONU/Blueprints/Widgets/WBP_InventoryHUD.uasset`

Scope: M

## Checkpoint B: Widget Integration

- HUD ammo와 Inventory slot ammo가 같은 gameplay state를 표시하는지 확인
- `PUBG.InfAmmo true`에서 HUD 오른쪽만 `∞`이고 Inventory slot은 `LoadedAmmo/MagazineSize` 유지 확인
- Widget BP compile error 없음 확인

## Task 6: 자동화 테스트 보강

Description: ViewModel 데이터 계약과 default behavior를 자동화 테스트로 고정한다. UMG 표시 자체는 수동 검증으로 처리한다.

Acceptance:

- `FBGActiveWeaponAmmoRenderData` default 값 테스트
- unbound `UBG_HealthViewModel::GetActiveWeaponAmmoData()` safe default 테스트
- `ForceUpdateAllAttributes()`가 ammo delegate도 broadcast하는 테스트
- `FBGEquipmentSlotRenderData` default ammo fields 유지 테스트
- 가능하면 inventory/equipment transient fixture로 reserve ammo 계산 테스트

Verification:

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.UI
```

Dependencies: Task 1, Task 2, Task 3

Files likely touched:

- `Source/PUBG_HotMode/GEONU/UI/Tests/BG_HealthViewModelAmmoTests.cpp`
- `Source/PUBG_HotMode/GEONU/UI/Tests/BG_InventoryViewModelRenderDataTests.cpp`

Scope: S

## Task 7: 최종 통합 검증

Description: C++ build, UI automation tests, inventory/combat regression, Dedicated Server PIE 수동 검증을 수행한다.

Acceptance:

- `PUBG_HotMode` project module build 성공
- UI automation tests 성공
- Inventory regression tests 성공
- Combat regression tests 성공
- HealthBar 중앙 loaded ammo 표시
- HealthBar 오른쪽 reserve ammo 표시
- `PUBG.InfAmmo true` 후 HealthBar 오른쪽 `∞` 표시
- `PUBG.InfAmmo false` 후 HealthBar 오른쪽 실제 reserve ammo 표시
- Inventory weapon slot `LoadedAmmo/MagazineSize` 표시
- owner client 외 reserve ammo 노출 없음

Verification:

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.UI
.\RunTests.ps1 PUBG_HotMode.GEONU.Inventory
.\RunTests.ps1 PUBG_HotMode.GEONU.Combat
& 'C:\Program Files\Epic Games\UE_5.7_Source\Engine\Build\BatchFiles\Build.bat' PUBG_HotModeEditor Win64 Development -Project='C:\Users\me\source\UE5\PUBG_HotMode\PUBG_HotMode.uproject' -Module=PUBG_HotMode -NoEngineChanges -NoHotReloadFromIDE -WaitMutex
```

Manual check:

- Dedicated Server + 2 Clients PIE
- owner client: M416 equip, fire, reload, `PUBG.Give Item.Ammo.556 300`
- owner client: Kar98k equip, fire, reload, `PUBG.Give Item.Ammo.762 300`
- owner client: `PUBG.InfAmmo true`, reload, HealthBar `∞`
- owner client: `PUBG.InfAmmo false`, HealthBar reserve ammo number
- owner client: Inventory open, PrimaryA/PrimaryB/Pistol ammo text
- remote client: 다른 플레이어 reserve ammo 미노출

Dependencies: Task 1, Task 2, Task 3, Task 4, Task 5, Task 6

Files likely touched:

- `Docs/Plan-WeaponAmmoUI.md`
- `Docs/Spec-WeaponAmmoUI.md` only if decisions change

Scope: S

## 병렬화

- Task 1 완료 후 Task 2와 Task 3 병렬 가능
- Task 4는 Task 1/2 완료 후 진행
- Task 5는 Task 3 완료 후 진행
- Task 6은 Task 1/2/3 이후 진행
- Task 7은 전체 통합 후 진행

## 위험과 대응

| 위험 | 영향 | 대응 |
| --- | --- | --- |
| `UBG_HealthViewModel` 책임이 Health 밖으로 확장 | 명명과 역할 불일치 | Spec 결정대로 기존 HUD ViewModel으로 사용, 후속 `UBG_HUDViewModel` 분리는 보류 |
| `CurrentReserveAmmo=999`가 HUD에 노출 | 무한탄 표시와 실제 inventory 수량 혼동 | HUD는 inventory quantity와 `bUsesInfiniteDebugAmmo`를 직접 계산 |
| active weapon actor replication 지연 | HUD 초기값 0 또는 숨김 | equipment owner state와 weapon row fallback 사용 |
| Widget BP delegate pin 변경 | BP compile/bind 오류 | Task 4/5에서 Widget compile 필수 |
| `∞` 문자 표시 폰트 누락 | HealthBar 오른쪽 문자가 깨짐 | 폰트 확인, 필요 시 BP에서 `"INF"` fallback 논의 |
| Editor/Live Coding build lock | 검증 중 build 실패 | 프로젝트 규칙대로 실패 보고 후 중단 |

## 최종 검증 명령

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.UI
.\RunTests.ps1 PUBG_HotMode.GEONU.Inventory
.\RunTests.ps1 PUBG_HotMode.GEONU.Combat
& 'C:\Program Files\Epic Games\UE_5.7_Source\Engine\Build\BatchFiles\Build.bat' PUBG_HotModeEditor Win64 Development -Project='C:\Users\me\source\UE5\PUBG_HotMode\PUBG_HotMode.uproject' -Module=PUBG_HotMode -NoEngineChanges -NoHotReloadFromIDE -WaitMutex
```

## 최종 수동 검증

- Dedicated Server + 2 Clients PIE
- owner client: no weapon, M416, Kar98k, P92, melee 상태별 HUD 표시
- owner client: fire/reload/pickup/Give/InfAmmo true/InfAmmo false 표시 갱신
- owner client: Inventory 장착 총 slot `LoadedAmmo/MagazineSize`
- remote client: reserve ammo 미노출
