# Spec - Weapon Ammo UI

## 목표

장착 무기와 HUD에서 현재 탄창 상태와 인벤토리 예비 탄약 수량을 표시한다.

성공 기준:

- Inventory 화면의 장착 총 슬롯마다 `탄창 잔탄/탄창 크기` 표시
- HealthBar 중앙에 현재 활성화된 총의 탄창 잔탄 표시
- HealthBar 중앙 오른쪽에 현재 활성화된 총의 예비 탄약 수량 또는 무한탄 `∞` 표시
- 발사, 장전, 무기 교체, 탄약 줍기, `PUBG.Give` 이후 표시 갱신
- `PUBG.InfAmmo true` 상태에서 HealthBar 예비 탄약은 `∞` 표시
- Dedicated Server + 2 Clients PIE에서 owner HUD만 owner inventory 수량 표시

초기 범위:

- 장착 총기 슬롯: `PrimaryA`, `PrimaryB`, `Pistol`
- 활성 총 HUD 표시: 현재 `UBG_EquipmentComponent::GetActiveWeaponSlot()` 기준
- 기본 예비 탄약 표시: `UBG_InventoryComponent::GetQuantity(Ammo, ActiveWeaponAmmoItemTag)` 기준
- 기존 `WBP_GameHUD`, `WBP_InventoryHUD`, `UBG_HealthViewModel`, `UBG_InventoryViewModel` 흐름 재사용

제외:

- 탄약 아이콘 HUD 표시
- 총기별 탄약 경고 색상/애니메이션
- 장전 진행률 표시
- 장착하지 않은 인벤토리 무기의 탄창 표시
- 원격 플레이어 탄약 HUD 표시
- spectator HUD 전환 고도화
- Weapon attachment가 magazine size를 바꾸는 기능

## 정의

- Loaded ammo: `ABG_EquippedWeaponBase::GetLoadedAmmo()`가 반환하는 현재 탄창 잔탄
- Magazine size: `ABG_EquippedWeaponBase::GetMagazineCapacity()` 또는 weapon row `MagazineSize`
- Reserve ammo: 활성 총의 `AmmoItemTag`와 일치하는 `EBG_ItemType::Ammo` 인벤토리 총 수량
- Active weapon: `UBG_EquipmentComponent::GetActiveWeaponSlot()`에 들어 있는 현재 입력 대상 무기
- Gun slot: `Weapon` slot 중 `MagazineSize > 0`이고 `AmmoItemTag`가 유효한 슬롯
- HealthBar ammo display: `WBP_GameHUD` 또는 `WBP_HealthGauge`에서 HealthBar 기준 중앙/중앙 오른쪽에 배치되는 텍스트 표시

제약:

- 탄약 수량 변경의 source of truth는 서버 권위 상태
- Inventory 수량은 owner-only replication 대상
- HUD reserve ammo 수량은 `UBG_WeaponFireComponent::CurrentReserveAmmo`를 source로 사용하지 않음
- `CurrentReserveAmmo`는 `bUseInfiniteDebugAmmo=true`일 때 debug 표시값 `999`가 될 수 있음
- HealthBar `∞` 표시 여부는 active weapon actor의 `UsesInfiniteDebugAmmo()` 값을 ViewModel이 공개해 결정
- Widget Blueprint는 gameplay component를 직접 변경하지 않음
- null check 실패는 기존 규칙대로 명시적 error log

전제:

- `WBP_GameHUD`가 local player screen에 항상 생성됨
- `ABG_PlayerController::GetHUDViewModel()`은 현재 `UBG_HealthViewModel`을 반환함
- `UBG_InventoryViewModel::FBGEquipmentSlotRenderData`에는 이미 `LoadedAmmo`, `MagazineSize`, `ReserveAmmo` 필드가 있음
- 현재 장착 weapon actor가 없는 경우 equipment owner state와 weapon row를 fallback으로 사용 가능

## 분석

현재 구조:

- `ABG_PlayerController`가 `HealthViewModel`, `InventoryViewModel`, `WeaponIconCaptureComponent`를 subobject로 소유
- `WBP_GameHUD` 기본 class는 `/Game/GEONU/Blueprints/Widgets/WBP_GameHUD`
- `WBP_InventoryHUD` 기본 class는 `/Game/GEONU/Blueprints/Widgets/WBP_InventoryHUD`
- `UBG_HealthViewModel`은 health/boost/armor HUD delegate를 제공하지만 weapon ammo delegate는 없음
- `UBG_InventoryViewModel`은 장착 슬롯 render data를 제공하고 `LoadedAmmo`, `MagazineSize`, `ReserveAmmo` 필드를 이미 노출
- `UBG_InventoryViewModel::BuildEquipmentSlotRenderData`는 현재 weapon slot `LoadedAmmo`만 채우고 `MagazineSize`, `ReserveAmmo`는 채우지 않음
- `UBG_WeaponFireComponent`는 replicated ammo state와 `OnAmmoChanged(CurrentAmmo, MaxAmmo)` delegate를 제공
- `UBG_InventoryComponent`는 `OnInventoryChanged`, `OnInventoryItemQuantityChanged` delegate를 제공
- `UBG_EquipmentComponent`는 `OnEquipmentChanged`, `OnActiveWeaponSlotChanged` delegate를 제공

선택지:

| 선택지 | 내용 | 장점 | 한계 | 결정 |
| --- | --- | --- | --- | --- |
| `WBP_GameHUD`가 component 직접 조회 | Widget에서 Character/Inventory/WeaponFire를 직접 읽음 | C++ 변경 적음 | Widget이 gameplay 구조에 강결합 | 제외 |
| `UBG_WeaponFireComponent::OnAmmoChanged` 확장 | delegate에 reserve ammo/infinite flag 추가 | 기존 ammo event 재사용 | BP delegate signature 변경 위험, InfAmmo debug `999` 혼동 | 제외 |
| `UBG_HealthViewModel` 확장 | 기존 HUD ViewModel에 ammo display data 추가 | GameHUD 바인딩 경로 유지, 책임 집중 | 이름이 Health 중심으로 남음 | 채택 |
| 신규 `UBG_HUDViewModel` 생성 | Health/Ammo 전체 HUD VM 분리 | 명명 정확 | PlayerController/BP 교체 범위 증가 | 보류 |

결정:

- HUD 탄약 표시는 `UBG_HealthViewModel`에 HUD용 ammo render data와 delegate를 추가해 처리
- Inventory 장착 총 슬롯 표시는 기존 `FBGEquipmentSlotRenderData` 필드를 채워 처리
- 예비 탄약 수량은 항상 inventory에서 직접 계산
- HealthBar infinite reserve 표시는 `FBGActiveWeaponAmmoRenderData::bUsesInfiniteDebugAmmo`로 분기
- `UBG_WeaponFireComponent::OnAmmoChanged` 기존 signature는 유지
- 발사/장전 표시 갱신은 `OnAmmoChanged`, 무기 교체는 `OnEquipmentChanged`/`OnActiveWeaponSlotChanged`, 탄약 줍기/Give는 `OnInventoryItemQuantityChanged`로 처리
- UI가 보유 탄약을 예측하지 않고 복제/서버 응답 이후 값만 표시

## 명세

### Inventory 장착 총 슬롯

표시 대상:

- `PrimaryA`
- `PrimaryB`
- `Pistol`

표시 형식:

```text
<LoadedAmmo>/<MagazineSize>
```

예시:

```text
12/30
5/5
0/15
```

표시 규칙:

- 슬롯이 비어 있으면 ammo text 숨김
- melee slot은 ammo text 숨김
- `AmmoItemTag` invalid 또는 `MagazineSize <= 0`이면 ammo text 숨김
- `LoadedAmmo`는 `ABG_EquippedWeaponBase::GetLoadedAmmo()` 우선
- weapon actor가 없으면 `UBG_EquipmentComponent::GetLoadedAmmo(Slot)` fallback
- `MagazineSize`는 `ABG_EquippedWeaponBase::GetMagazineCapacity()` 우선
- weapon actor가 없거나 capacity가 0이면 `FBG_WeaponItemDataRow::MagazineSize` fallback
- 값은 0 이상으로 clamp

`FBGEquipmentSlotRenderData` 목표:

| Field | Source |
| --- | --- |
| `LoadedAmmo` | equipped weapon actor loaded ammo, fallback owner state |
| `MagazineSize` | equipped weapon actor magazine capacity, fallback weapon row |
| `ReserveAmmo` | 해당 weapon ammo tag의 inventory quantity |

Inventory 갱신 이벤트:

- `UBG_EquipmentComponent::OnEquipmentChanged`
- `UBG_InventoryComponent::OnInventoryChanged`
- `UBG_InventoryComponent::OnInventoryItemQuantityChanged`

### HUD ammo render data

추가 render data:

```cpp
USTRUCT(BlueprintType)
struct FBGActiveWeaponAmmoRenderData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo")
	bool bHasActiveGun = false;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo")
	EBG_EquipmentSlot ActiveWeaponSlot = EBG_EquipmentSlot::None;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo", meta=(Categories="Item.Weapon"))
	FGameplayTag ActiveWeaponItemTag;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo", meta=(Categories="Item.Ammo"))
	FGameplayTag AmmoItemTag;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo", meta=(ClampMin="0"))
	int32 LoadedAmmo = 0;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo", meta=(ClampMin="0"))
	int32 MagazineSize = 0;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo", meta=(ClampMin="0"))
	int32 ReserveAmmo = 0;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo")
	bool bUsesInfiniteDebugAmmo = false;
};
```

`UBG_HealthViewModel` 추가 API:

- `OnActiveWeaponAmmoChanged(FBGActiveWeaponAmmoRenderData AmmoData)`
- `GetActiveWeaponAmmoData()`
- `ForceUpdateAllAttributes()`에서 ammo delegate도 broadcast

바인딩 대상:

- `UBG_HealthComponent`
- `UBG_InventoryComponent`
- `UBG_EquipmentComponent`
- `UBG_WeaponFireComponent`

갱신 트리거:

- possession ready/cleared
- health view model initial bind
- `UBG_WeaponFireComponent::OnAmmoChanged`
- `UBG_InventoryComponent::OnInventoryChanged`
- `UBG_InventoryComponent::OnInventoryItemQuantityChanged`
- `UBG_EquipmentComponent::OnEquipmentChanged`
- `UBG_EquipmentComponent::OnActiveWeaponSlotChanged`

계산 규칙:

- active slot이 `None`이면 `bHasActiveGun=false`
- active slot weapon tag가 invalid이면 `bHasActiveGun=false`
- active weapon row가 없으면 `bHasActiveGun=false`
- active weapon row `AmmoItemTag`가 invalid이면 `bHasActiveGun=false`
- active weapon actor가 있으면 `LoadedAmmo`, `MagazineSize`, `AmmoItemTag`는 actor runtime state 우선
- active weapon actor가 있으면 `bUsesInfiniteDebugAmmo`는 `UsesInfiniteDebugAmmo()` 값 사용
- active weapon actor가 없으면 equipment owner state와 weapon row fallback
- `ReserveAmmo`는 `InventoryComponent->GetQuantity(EBG_ItemType::Ammo, AmmoItemTag)`
- `bUsesInfiniteDebugAmmo=true`여도 `ReserveAmmo` 값은 실제 inventory quantity 유지
- 표시 데이터가 이전 값과 다를 때만 `OnActiveWeaponAmmoChanged` broadcast

### HealthBar 표시

배치:

- HealthBar 중앙: `LoadedAmmo`
- HealthBar 중앙 오른쪽: `bUsesInfiniteDebugAmmo=true`이면 `∞`, 아니면 `ReserveAmmo`
- 텍스트는 HealthBar 입력/마우스 hit test를 막지 않는 visibility 사용

표시 형식:

| 상태 | 중앙 | 중앙 오른쪽 |
| --- | --- | --- |
| 활성 총 있음, 무한탄 false | `<LoadedAmmo>` | `<ReserveAmmo>` |
| 활성 총 있음, 무한탄 true | `<LoadedAmmo>` | `∞` |
| 활성 총 없음 | 숨김 | 숨김 |
| melee 활성 | 숨김 | 숨김 |
| weapon row 없음 | 숨김 | 숨김 |
| inventory component 없음, 무한탄 false | `<LoadedAmmo>` | `0` |
| inventory component 없음, 무한탄 true | `<LoadedAmmo>` | `∞` |

예시:

```text
30    90
12    ∞
0     0
```

UMG 작업:

- `WBP_GameHUD`에서 `GetOwningPlayer -> Cast ABG_PlayerController -> GetHUDViewModel` 사용
- HealthBar가 `WBP_HealthGauge` 내부에 있으면 해당 widget에 ammo text binding/event를 전달
- `OnActiveWeaponAmmoChanged` 바인딩 후 text/visibility 갱신
- Construct 시 `GetActiveWeaponAmmoData()`로 초기값 적용
- `bUsesInfiniteDebugAmmo=true`이면 HealthBar 중앙 오른쪽 text를 `∞`로 설정
- 숨김 상태는 `Collapsed` 또는 `Hidden`; layout 흔들림이 있으면 `Hidden`

### 갱신 시나리오

발사:

```text
RequestFire
-> server ConsumeLoadedAmmo
-> Equipment Auth_LoadAmmo
-> WeaponFire SyncAmmoFromEquipment
-> OnAmmoChanged
-> HUD LoadedAmmo 갱신
```

장전 완료:

```text
CompleteReload
-> inventory ammo remove
-> weapon FinishWeaponReload
-> Equipment Auth_LoadAmmo
-> Inventory quantity delegate
-> WeaponFire SyncAmmoFromEquipment
-> HUD LoadedAmmo/ReserveAmmo 갱신
```

탄약 줍기 또는 `PUBG.Give`:

```text
Auth_AddItem
-> Inventory quantity delegate
-> HUD ReserveAmmo 갱신
```

무기 교체:

```text
Auth_ActivateWeapon
-> OnActiveWeaponSlotChanged
-> OnEquipmentChanged
-> HUD LoadedAmmo/MagazineSize/ReserveAmmo 갱신
```

무기 장착/해제:

```text
Auth_EquipWeapon/Auth_UnequipWeapon
-> OnEquipmentChanged
-> HUD visibility와 ammo data 갱신
```

무한탄 토글:

```text
PUBG.InfAmmo true/false
-> active weapon SetUseInfiniteDebugAmmo
-> WeaponFire RefreshAmmoStateFromEquipment
-> OnAmmoChanged
-> HUD bUsesInfiniteDebugAmmo와 reserve 표시 갱신
```

## 수용 기준

- Inventory 화면에서 M416 슬롯이 `30/30`, 발사 후 `29/30`으로 표시
- Inventory 화면에서 Kar98k 슬롯이 `5/5`, 발사 후 `4/5`로 표시
- Inventory 화면에서 P92 슬롯이 `15/15` 또는 현재 runtime magazine 기준으로 표시
- melee slot은 탄약 텍스트 미표시
- HealthBar 중앙에 활성 M416 loaded ammo만 표시
- 무한탄 false 상태에서 HealthBar 중앙 오른쪽에 `Item.Ammo.556` inventory 총 수량 표시
- 활성 무기를 Kar98k로 바꾸면 HealthBar 중앙 오른쪽이 `Item.Ammo.762` inventory 총 수량으로 변경
- 5.56mm 탄약 상자 pickup 후 M416 활성 상태에서 HealthBar 중앙 오른쪽 수량 증가
- `PUBG.Give Item.Ammo.556 300` 후 M416 활성 상태에서 HealthBar 중앙 오른쪽 수량 증가
- M416 장전 완료 후 HealthBar 중앙 증가, 중앙 오른쪽 감소
- `PUBG.InfAmmo true` 상태에서 HealthBar 중앙 오른쪽은 `∞` 표시
- `PUBG.InfAmmo false` 전환 시 HealthBar 중앙 오른쪽은 실제 inventory 수량 표시
- `PUBG.InfAmmo true` 상태에서도 `FBGActiveWeaponAmmoRenderData::ReserveAmmo` 값은 실제 inventory 수량 유지
- 활성 weapon이 없거나 melee만 활성화되면 HealthBar 탄약 표시 숨김
- Dedicated Server + 2 Clients PIE에서 owner client만 자신의 inventory reserve ammo 확인

## 프로젝트 맥락

관련 문서:

- `Docs/Spec-AmmoBoxPickup.md`
- `Docs/Spec-InventoryEquipmentUI.md`
- `Docs/Spec-WeaponSystem.md`

관련 C++:

- `Source/PUBG_HotMode/GEONU/UI/BG_HealthViewModel.h`
- `Source/PUBG_HotMode/GEONU/UI/BG_HealthViewModel.cpp`
- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.h`
- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.h`
- `Source/PUBG_HotMode/WON/Public/Combat/BG_WeaponFireComponent.h`
- `Source/PUBG_HotMode/WON/Private/Combat/BG_WeaponFireComponent.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_PlayerController.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_PlayerController.cpp`

관련 Content:

- `Content/GEONU/Blueprints/Widgets/WBP_GameHUD.uasset`
- `Content/GEONU/Blueprints/Widgets/WBP_InventoryHUD.uasset`
- `Content/GEONU/Blueprints/Widgets/Components/WBP_HealthGauge.uasset`
- `Content/GEONU/Blueprints/Widgets/Components/WBP_EquippedWeaponSlot.uasset`

검증 명령:

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.UI
.\RunTests.ps1 PUBG_HotMode.GEONU.Inventory
.\RunTests.ps1 PUBG_HotMode.GEONU.Combat
& 'C:\Program Files\Epic Games\UE_5.7_Source\Engine\Build\BatchFiles\Build.bat' PUBG_HotModeEditor Win64 Development -Project='C:\Users\me\source\UE5\PUBG_HotMode\PUBG_HotMode.uproject' -Module=PUBG_HotMode -NoEngineChanges -NoHotReloadFromIDE -WaitMutex
```

수동 검증:

- Dedicated Server + 2 Clients PIE
- owner client: M416 장착, 발사, 장전, 5.56 pickup/Give
- owner client: Kar98k 장착, 발사, 장전, 7.62 pickup/Give
- owner client: `PUBG.InfAmmo true/false` 상태의 HealthBar `∞`/실제 reserve ammo 표시
- owner client: Inventory open 상태에서 장착 슬롯 ammo text 확인
- remote client: 다른 플레이어 inventory reserve ammo가 노출되지 않음

## 열린 질문

- 없음
