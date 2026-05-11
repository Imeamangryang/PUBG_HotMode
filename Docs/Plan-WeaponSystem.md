# 무기 시스템 구현 계획

## 문서 역할

- 원본 요구사항과 확정 결정은 `Docs/Spec-WeaponSystem.md`
- 이 문서는 Spec을 구현 가능한 작업 순서, 의존성, 검증 기준으로 분해
- 구현 중 Spec 변경이 필요한 판단이 생기면 Spec을 먼저 갱신한 뒤 Plan 갱신
- 구현 파일은 이 Plan 단계에서 수정하지 않음

## 스펙 검토 결과

- 구현을 차단하는 미해결 질문 없음
- Pan 투척, projectile 예측 범위, world item/equipped actor 분리 구조, DataTable 수치 출처 정책 모두 결정됨
- Plan 작성 가능 상태로 판단

계획에서 고정한 세부 결정:

| 항목 | 결정 |
| --- | --- |
| World item class | 공통 `FBG_ItemDataRow::WorldItemClass`를 무기 포함 모든 pickup/drop actor class로 사용 |
| Equipped weapon class | `FBG_WeaponItemDataRow::EquippedWeaponClass`를 추가하고 장착 actor class로 사용 |
| Equipped actor ownership | `UBG_EquipmentComponent`가 slot별 `ABG_EquippedWeaponBase` actor ref를 관리. tag와 loaded ammo는 기존 Equipment state가 source of truth |
| Pickup actor 처리 | `ABG_WorldItemBase` pickup 성공 시 world item을 consume/destroy하고 `EquippedWeaponClass`로 equipped actor를 spawn/attach |
| Drop actor 처리 | slot equipped actor를 destroy/detach하고 `WorldItemClass`로 loaded ammo가 보존된 world item spawn |
| Projectile ammo consume | projectile spawn 성공 후 loaded ammo 차감. spawn 실패 시 발사 실패로 처리하고 ammo 유지 |
| Hitscan ammo consume | server validation 통과 후 trace 전에 loaded ammo 차감. damage는 server hit 결과로만 적용 |
| 예측 구조 | owner client visual-only prediction. server confirm/reject는 `ShotPredictionId`만으로 매칭 |
| Combat component | 기존 WON `UBG_WeaponFireComponent`는 폐기 대상으로 보고 참고만 함. 신규 `UBG_WeaponCombatComponent`를 `GEONU/Combat`에 작성 |
| M416 우선순위 | M416 end-to-end가 첫 전투 검증 checkpoint. 이후 P92, Kar98k, Pan 확장 |
| Mesh asset 부족 | M416/Kar98k/Pan mesh asset이 없으면 임시 mesh로 Blueprint를 만들고 art asset 교체는 별도 content 작업으로 처리 |

## 구현 분석

### 핵심 의존성

| 축 | 먼저 고정해야 하는 이유 | 후속 작업 |
| --- | --- | --- |
| GameplayTag, item row, fire spec row | pickup, equip, reload, fire가 같은 weapon identity를 사용해야 함 | actor, equipment, fire, UI |
| `ABG_EquippedWeaponBase` | muzzle transform, equipped mesh, attach transform, fire FX hook의 기반 | equipment actor lifecycle, fire |
| Equipment slot actor lifecycle | actor ref와 기존 tag/ammo state가 어긋나면 visual, reload, drop이 깨짐 | input, fire, pickup/drop |
| InputAction weapon request | raw key가 EquipmentComponent를 우회하는 현재 문제를 먼저 제거해야 함 | reload cancel, fire sync |
| Fire spec lookup | 신규 combat component가 DataTable 기반으로 projectile/hitscan/Pan을 분기해야 함 | M416, P92, Kar98k, Pan |
| Projectile prediction | M416 full-auto 체감과 server 권위 사이의 계약 | P92 projectile, Pan throw |

### 위험 우선순위

- 기존 WON `UBG_WeaponFireComponent`는 eye view trace, hardcoded settings, reload fallback에 강하게 묶여 있어 수정 비용이 큼. 최종 구현에서는 참고만 하고 신규 component로 대체
- world item actor와 equipped actor를 분리하므로 pickup/drop actor lifecycle과 equipped actor lifecycle이 어긋나지 않도록 EquipmentComponent에서 순서를 통제해야 함
- Equipment state는 tag/ammo를 이미 복제하므로 actor ref 복제를 추가할 때 source of truth 중복 위험 있음
- raw key weapon 변경이 `ABG_Character::SetWeaponState`를 직접 호출하므로 input 정리 전 fire/reload 검증이 왜곡될 수 있음
- weapon Blueprint/DataTable asset authoring은 C++ build로 검증되지 않으므로 Dedicated Server PIE 수동 검증 필요
- projectile prediction은 초깃값 보정만 제공. lag compensation/server rewind는 이번 범위에서 제외

### 구현 경계

- 신규 C++ 중심 위치: `Source/PUBG_HotMode/GEONU/Inventory`, `Source/PUBG_HotMode/GEONU/Combat`
- 기존 Character, PlayerController 변경은 input routing과 component 연결에 필요한 최소 범위
- 신규 weapon gameplay 책임은 `ABG_WorldItemBase`, `ABG_EquippedWeaponBase`, `UBG_EquipmentComponent`, `UBG_WeaponCombatComponent`, projectile actor로 분산
- `Source/PUBG_HotMode/WON/Public/Combat/BG_WeaponFireComponent.*`는 legacy/reference 코드로만 사용하고 직접 확장하지 않음
- Widget은 `UBG_InventoryViewModel` 또는 Character request API만 호출
- null/invalid state는 silent fail 없이 error log

## 기술 설계 요약

### 런타임 계약

- Item identity: `EBG_ItemType::Weapon` + concrete `FGameplayTag`
- Item row: display data, inventory data, `WorldItemClass`
- Weapon item row: equip slot, pose category, ammo tag, reload durations, `EquippedWeaponClass`
- Fire spec row: fire mode, implementation, damage, cooldown, projectile class, throw settings
- Equipment source of truth: equipped weapon tag, active slot, loaded ammo
- World item source of truth: pickup/drop visual, collision, quantity, loaded ammo
- Equipped weapon actor source of truth: equipped visual, mesh root, muzzle transform, attach transform
- Damage source of truth: server-only `UBG_DamageSystem`

### 데이터 흐름

```text
World weapon item pickup
  -> ABG_WorldItemBase::TryPickup
  -> UBG_EquipmentComponent::TryEquipWeapon
  -> world item consume/destroy
  -> EquippedWeaponClass spawn
  -> slot tag/ammo state update
  -> equipped actor attach
  -> ApplyActiveWeaponStateToCharacter
```

```text
Attack input
  -> ABG_PlayerController InputAction
  -> ABG_Character::StartPrimaryActionFromInput
  -> UBG_WeaponCombatComponent
  -> active slot + weapon item row + fire spec row
  -> active equipped weapon actor muzzle transform
  -> server projectile/hitscan/melee/throw
  -> equipment loaded ammo update
  -> owner confirm/reject + remote replicated visual
```

```text
Reload input
  -> UBG_WeaponCombatComponent::TryStartReload
  -> active weapon item row + inventory ammo validation
  -> reload timer
  -> completion revalidation
  -> inventory ammo consume
  -> equipment loaded ammo update
```

### 의존성 그래프

```text
#1 데이터 계약과 태그
  -> #2 Equipped weapon actor base
    -> #3 Equipment equipped actor lifecycle
      -> #4 InputAction weapon routing
      -> #5 New combat component and strict reload
        -> #6 M416 projectile + prediction
          -> #7 P92/Kar98k variants
          -> #8 Pan melee + throw
            -> #9 UI/ViewModel feedback
              -> #10 최종 검증
```

### 병렬 가능성

| 시점 | 병렬 가능한 작업 | 조건 |
| --- | --- | --- |
| #1 완료 후 | #2 C++ base와 CSV/DataTable 초안 | row field 이름 고정 필요 |
| #3 완료 후 | #4 input routing과 #5 신규 combat component 일부 | Equipment request API가 먼저 고정되어야 함 |
| #6 완료 후 | #7 P92/Kar98k와 #9 UI 표시 일부 | M416 projectile/prediction contract가 안정화되어야 함 |
| #8 완료 후 | content Blueprint polish와 final PIE checklist | C++ build가 먼저 통과해야 함 |

## 공통 검증

Build:

```powershell
& "<UE_ROOT>\Engine\Build\BatchFiles\Build.bat" PUBG_HotModeEditor Win64 Development -Project="D:\Dev\P4\PUBG_HotMode\PUBG_HotMode.uproject" -WaitMutex
```

수동 검증:

- Dedicated Server + 2 Clients PIE
- owner client: weapon/ammo pickup, switch, unequip, reload, M416 fire prediction, P92 fire, Kar98k fire, Pan melee/throw/recover
- remote client: equipped weapon visual, server fire FX/projectile, pickup/drop actor replication
- no ammo, empty slot, invalid actor class, too far pickup, reload cancel 실패 경로 확인

## 구현 단위

### #1 데이터 계약과 태그

Description: concrete weapon/ammo tag, weapon item row 확장, fire spec row, DataTable source를 먼저 고정한다. 기존 generic weapon row는 분류 tag로만 남기고 runtime row는 leaf tag를 사용한다.

Acceptance:
- `Item.Weapon.AR.M416`, `Item.Weapon.SR.Kar98k`, `Item.Weapon.Pistol.P92`, `Item.Weapon.Melee.Pan`, `Item.Ammo.556`, `Item.Ammo.762`, `Item.Ammo.9mm` tag 등록
- `FBG_WeaponItemDataRow`에 `EquippedWeaponClass`, tactical reload, Kar98k partial reload duration 필드 추가
- `GEONU/Combat`에 `FBG_WeaponFireSpecRow`와 fire mode/implementation enum 추가
- `UBG_ItemDataRegistry` 또는 registry subsystem에서 `DT_WeaponFireSpecs` lookup 가능
- weapon CSV source의 각 weapon row가 `WorldItemClass`와 `EquippedWeaponClass`를 모두 가짐
- CSV source에 M416, Kar98k, P92, Pan, 9mm ammo, fire spec seed row 추가

Verification:
- Build 성공
- PIE 시작 로그에서 registry validation 성공
- RowName과 `WeaponItemTag.GetTagName()` 불일치 row가 error log로 잡힘

Dependencies: 없음

Files likely touched:
- `Config/Tags/BG_InventoryItems.ini`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemTypes.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemDataRow.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponFireTypes.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemDataRegistry.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemDataRegistry.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemDataRegistrySubsystem.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemDataRegistrySubsystem.cpp`
- `Content/GEONU/Data/Source/Items_Weapon.csv`
- `Content/GEONU/Data/Source/Items_Ammo.csv`
- `Content/GEONU/Data/Source/WeaponFireSpecs.csv`

Scope: M

### #2 Equipped Weapon Actor Base

Description: `GEONU/Combat`에 `ABG_EquippedWeaponBase`를 추가해 장착 mesh, muzzle transform, attach transform, Blueprint hook을 제공한다. World pickup/drop 표시는 `ABG_WorldItemBase`와 `WorldItemClass`가 담당한다.

Acceptance:
- `ABG_EquippedWeaponBase : AActor` 추가
- equipped mesh root와 optional FX/audio component root 제공
- `GetMuzzleTransform`, hand/back attach transform, owner character 설정 API 제공
- weapon-specific interface 없이 virtual function과 Blueprint event로 확장 가능
- pickup collision, quantity, loaded ammo world state는 equipped actor가 소유하지 않음

Verification:
- Build 성공
- Editor에서 `BP_Weapon_*` equipped weapon parent로 선택 가능
- muzzle component 미지정 시 fire 시작 전에 명시적 error log

Dependencies: #1 데이터 계약과 태그

Files likely touched:
- `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.cpp`

Scope: M

### #3 Equipment Equipped Actor Lifecycle

Description: EquipmentComponent가 weapon slot별 equipped actor ref를 관리하고, pickup/drop 시 `EquippedWeaponClass` spawn/attach와 destroy를 처리한다. 기존 tag/ammo state는 source of truth로 유지하고 world pickup/drop actor는 `WorldItemClass`로 처리한다.

Acceptance:
- PrimaryA, PrimaryB, Pistol, Melee slot별 replicated equipped actor ref 또는 equivalent visual ref 추가
- weapon pickup 성공 시 world item은 consume/destroy되고 `EquippedWeaponClass` actor가 character socket에 attach
- `ABG_WorldItemBase` weapon pickup 시 `EquippedWeaponClass`로 equipped actor spawn
- weapon drop 시 equipped actor는 destroy/detach되고 loaded ammo를 보존한 `WorldItemClass` actor spawn
- actor spawn/attach 실패 시 equipment tag/ammo state가 변하지 않음

Verification:
- Build 성공
- Dedicated Server + 2 Clients PIE에서 weapon pickup/drop world item과 equipped actor가 owner/remote 모두 보임
- 교체 pickup 시 기존 weapon이 loaded ammo를 가진 world item actor로 남음

Dependencies: #2 Equipped Weapon Actor Base

Files likely touched:
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_WorldItemBase.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`

Scope: L

### Checkpoint A: World/Equipped Actor Loop

- #1, #2, #3 완료 후 weapon DataTable row, world item pickup, equipped attach, drop world item spawn을 먼저 검증
- 이 시점부터 fire/reload는 actor muzzle과 Equipment active slot만 사용
- actor ref와 equipment tag/ammo state가 어긋나지 않는지 로그와 PIE로 확인

### #4 InputAction Weapon Routing

Description: raw key weapon switching을 제거하고 Enhanced Input Action 기반으로 active weapon slot 변경과 unequip을 EquipmentComponent request로 라우팅한다.

Acceptance:
- `FBGPlayerInputConfig`에 PrimaryA, PrimaryB, Pistol, Melee, Unequip action 추가
- PlayerController weapon input은 Character request method만 호출
- Character request는 `UBG_EquipmentComponent` server request로 active slot 변경
- 기존 `One`, `Two`, `Zero` weapon raw key bind 제거
- weapon switch/unequip 시 reload와 automatic fire가 취소됨

Verification:
- Build 성공
- PIE에서 `IA_BG_WeaponPrimaryA/B/Pistol/Melee`와 `IA_BG_UnequipWeapon` 동작 확인
- empty slot 입력 시 owning client failure feedback

Dependencies: #3 Equipment Actor Lifecycle

Files likely touched:
- `Source/PUBG_HotMode/WON/Public/Player/BG_PlayerController.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_PlayerController.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.cpp`
- `Content/WON/Input/IA_BG_WeaponPrimaryA.uasset`
- `Content/WON/Input/IA_BG_WeaponPrimaryB.uasset`
- `Content/WON/Input/IA_BG_WeaponPistol.uasset`
- `Content/WON/Input/IA_BG_WeaponMelee.uasset`
- `Content/WON/Input/IA_BG_UnequipWeapon.uasset`
- `Content/WON/Input/IMC_BG.uasset`

Scope: M

### #5 New Combat Component And Strict Reload

Description: 기존 WON `UBG_WeaponFireComponent`를 수정하지 않고 `GEONU/Combat`에 `UBG_WeaponCombatComponent`를 새로 작성한다. 신규 component는 active weapon tag 기준으로 item row와 fire spec row를 조회하고, strict inventory ammo reload를 담당한다.

Acceptance:
- `UBG_WeaponCombatComponent`가 Character에 추가되고 Attack/Reload 입력 경로가 이 component로 연결됨
- 기존 WON `UBG_WeaponFireComponent`는 신규 무기 시스템 경로에서 호출되지 않음
- Character constructor에서 기존 `WeaponFireComponent` native subobject 생성 제거
- `ABG_Character`의 Attack, Reload, SetWeaponState, OnRep, Death 경로가 `UBG_WeaponCombatComponent`만 호출
- C++에서 `GetWeaponFireComponent`, `WeaponFireComponent` UPROPERTY, `BG_WeaponFireComponent` include 의존 제거
- `ResolveFireSettings(EBGWeaponPoseType)` 대신 active weapon tag 기반 `ResolveFireSpec`
- fire mode: `SemiAuto`, `FullAuto`, `BoltAction`, `Melee`
- fire implementation: `Projectile`, `Hitscan`, `Melee`
- reload 시작 전과 완료 시 Inventory ammo 재검증
- ammo 없음, `TryRemoveItem` 실패, invalid ammo tag는 reload 실패이며 loaded ammo 증가 없음

Verification:
- Build 성공
- PIE에서 no ammo reload reject, partial reload, full magazine reject 확인
- generic `Item.Weapon.AR` row 없이 M416 leaf row로 fire/reload 동작
- `rg \"WeaponFireComponent|BG_WeaponFireComponent|GetWeaponFireComponent\" Source/PUBG_HotMode/WON Source/PUBG_HotMode/GEONU` 결과가 legacy component 파일과 참고 문서 외에는 없음
- `BP_BG_Character_WON` Blueprint compile/resave 후 stale component/reference warning 없음

Dependencies: #4 InputAction Weapon Routing

Files likely touched:
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponCombatComponent.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponCombatComponent.cpp`
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponFireTypes.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryComponent.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`

Scope: M

### #6 M416 Projectile And Prediction

Description: M416을 첫 end-to-end slice로 구현한다. Server authoritative projectile과 owner client visual-only prediction을 연결하고, server confirm/reject로 predicted visual을 정리한다.

Acceptance:
- replicated `ABG_WeaponProjectileBase` 또는 동등한 projectile actor 추가
- M416 fire spec의 projectile class, speed, gravity 적용
- owner client는 발사 입력마다 `ShotPredictionId`를 증가시키고 predicted visual projectile/FX를 즉시 생성
- `ShotPredictionId`는 predicted visual과 server confirm/reject를 매칭하는 local serial number로만 사용
- server는 active slot, ammo, cooldown, reload/death state, muzzle transform을 검증
- projectile spawn 성공 후 loaded ammo 차감, spawn 실패 시 ammo 유지
- confirm/reject RPC가 `ShotPredictionId`로 owner predicted visual을 정리

Verification:
- Build 성공
- Dedicated Server + 2 Clients PIE에서 M416 hold fire 동작
- owner는 즉시 projectile/FX를 보고 remote는 server replicated projectile/FX만 봄
- reject 조건에서 owner predicted visual 제거와 ammo 보정 확인

Dependencies: #5 New Combat Component And Strict Reload

Files likely touched:
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponCombatComponent.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponCombatComponent.cpp`
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponProjectileBase.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponProjectileBase.cpp`
- `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.cpp`
- `Content/GEONU/Blueprints/Items/BP_Weapon_M416.uasset`

Scope: L

### Checkpoint B: M416 End-To-End

- M416 pickup, switch, reload, full-auto projectile, prediction confirm/reject를 먼저 고정
- 이후 P92와 Pan projectile 계열은 M416 projectile/prediction 계약 재사용
- Kar98k는 같은 fire spec lookup과 muzzle transform 계약만 재사용

### #7 P92 And Kar98k Variants

Description: M416에서 만든 fire spec, muzzle, prediction/replication 기반을 재사용해 P92 projectile semi-auto와 Kar98k hitscan bolt-action을 추가한다.

Acceptance:
- P92는 9mm ammo, 15발 magazine, semi-auto projectile로 동작
- Kar98k는 7.62mm ammo, 5발 magazine, bolt-action hitscan으로 동작
- Kar98k fire cooldown/rechamber 동안 재발사 불가
- Kar98k hitscan은 owner tracer/impact FX만 예측하고 damage는 server만 적용
- P92/Kar98k Blueprint가 muzzle component와 equipped/world mesh state를 제공

Verification:
- Build 성공
- Dedicated Server + 2 Clients PIE에서 P92 single fire, Kar98k single/bolt-action 확인
- DataTable fire spec 수정만으로 damage/cooldown 변경 확인

Dependencies: #6 M416 Projectile And Prediction

Files likely touched:
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponCombatComponent.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponCombatComponent.cpp`
- `Content/GEONU/Data/Source/Items_Weapon.csv`
- `Content/GEONU/Data/Source/WeaponFireSpecs.csv`
- `Content/GEONU/Blueprints/Items/BP_Weapon_P92.uasset`
- `Content/GEONU/Blueprints/Items/BP_Weapon_Kar98k.uasset`

Scope: M

### #8 Pan Melee And Throw

Description: Pan을 active melee weapon으로 구현하고, 조준 상태의 attack을 recoverable thrown melee projectile로 처리한다. 맨손 전투는 active weapon 없음 상태에서 유지한다.

Acceptance:
- active Melee slot의 Pan attack은 barehand가 아니라 Pan fire spec damage/range/cooldown 사용
- active weapon 없음 상태의 barehand melee는 기존 동작 유지
- Pan 장착 중 `IA_BG_AimAction` 유지 + `IA_BG_Attack`은 throw 실행
- throw 시작 시 Melee slot에서 Pan 제거, active slot 정규화, actor possession 해제
- thrown projectile은 30m, 15m부터 damage falloff, server-only damage 적용
- thrown projectile 정지/충돌 후 Pan `WorldItemClass` actor를 spawn해 회수 가능

Verification:
- Build 성공
- PIE에서 Pan melee body/head damage, aim+attack throw, world recovery 확인
- throw 후 inventory/equipment에 Pan이 남지 않고 pickup 시 다시 Melee slot 장착

Dependencies: #6 M416 Projectile And Prediction, #7 P92 And Kar98k Variants는 병렬 가능

Files likely touched:
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponCombatComponent.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponCombatComponent.cpp`
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponProjectileBase.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponProjectileBase.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.cpp`
- `Content/GEONU/Blueprints/Items/BP_Weapon_Pan.uasset`

Scope: L

### #9 UI/ViewModel Feedback

Description: Inventory/ViewModel이 active weapon, loaded ammo, reserve ammo, failure reason, nearby world item 상태를 표시하고 request API를 제공한다.

Acceptance:
- active slot, equipped weapon tag, loaded ammo, reserve ammo render data 갱신
- weapon switch/unequip request가 ViewModel 또는 Character API를 경유
- reload/fire/pickup/drop failure가 owning client에만 전달
- widget이 EquipmentComponent state를 직접 변경하지 않음

Verification:
- Build 성공
- PIE에서 ammo pickup/reload/fire 후 owner UI ammo 갱신 확인
- remote client에 owner-only inventory/reserve ammo가 노출되지 않음

Dependencies: #5 New Combat Component And Strict Reload, #8 Pan Melee And Throw

Files likely touched:
- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.h`
- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_PlayerController.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_PlayerController.cpp`
- `Content/GEONU/Blueprints/Widgets/WBP_Inventory.uasset`

Scope: M

### #10 최종 검증과 Content 정리

Description: 모든 weapon/ammo/world actor Blueprint와 DataTable asset을 reimport/연결하고 Dedicated Server + 2 Clients 기준으로 수용 기준을 검증한다.

Acceptance:
- `DT_Items_Weapon`, `DT_Items_Ammo`, `DT_WeaponFireSpecs`, `DA_ItemDataRegistry`가 최신 CSV/source와 일치
- `BP_Weapon_M416`, `BP_Weapon_Kar98k`, `BP_Weapon_P92`, `BP_Weapon_Pan`에 equipped mesh와 muzzle/attach transform 설정
- `BP_WorldItem_Weapon_M416`, `BP_WorldItem_Weapon_Kar98k`, `BP_WorldItem_Weapon_P92`, `BP_WorldItem_Weapon_Pan`에 world mesh와 pickup/display transform 설정
- `IA_BG_WeaponPrimaryA/B/Pistol/Melee/Unequip`가 `IMC_BG`에 매핑
- M416, P92, Kar98k, Pan, 5.56mm, 7.62mm, 9mm world pickup actor가 배치 또는 spawn 가능
- Spec의 수용 기준 전체 통과 또는 미통과 항목이 검증 기록에 남음

Verification:
- Build 성공
- Dedicated Server + 2 Clients PIE 수동 체크리스트 완료
- `p4 status`에서 의도한 C++/Config/CSV/uasset 변경만 확인

Dependencies: #1부터 #9

Files likely touched:
- `Content/GEONU/Data/DT_Items_Weapon.uasset`
- `Content/GEONU/Data/DT_Items_Ammo.uasset`
- `Content/GEONU/Data/DT_WeaponFireSpecs.uasset`
- `Content/GEONU/Data/DA_ItemDataRegistry.uasset`
- `Content/GEONU/Blueprints/Items/BP_Weapon_M416.uasset`
- `Content/GEONU/Blueprints/Items/BP_Weapon_Kar98k.uasset`
- `Content/GEONU/Blueprints/Items/BP_Weapon_P92.uasset`
- `Content/GEONU/Blueprints/Items/BP_Weapon_Pan.uasset`
- `Content/GEONU/Blueprints/Items/BP_WorldItem_Weapon_M416.uasset`
- `Content/GEONU/Blueprints/Items/BP_WorldItem_Weapon_Kar98k.uasset`
- `Content/GEONU/Blueprints/Items/BP_WorldItem_Weapon_P92.uasset`
- `Content/GEONU/Blueprints/Items/BP_WorldItem_Weapon_Pan.uasset`
- `Content/WON/Input/IMC_BG.uasset`
- 필요 시 검증 기록 문서

Scope: M

### Checkpoint C: Final Go/No-Go

- #10 완료 후 code build, DataTable validation, Dedicated Server PIE 수동 검증을 모두 기록
- M416 end-to-end가 불안정하면 P92/Kar98k/Pan 확장을 멈추고 M416 slice를 우선 안정화
- mesh/animation 품질 이슈는 gameplay go/no-go와 분리해 content polish backlog로 기록

## 테스트 체크리스트

### 데이터

- [ ] concrete weapon/ammo GameplayTag 등록
- [ ] `Items_Weapon.csv` leaf weapon row import
- [ ] `Items_Ammo.csv` 9mm row import
- [ ] `WeaponFireSpecs.csv` RowName과 `WeaponItemTag` 일치
- [ ] invalid weapon row lookup error log

### Pickup/Drop

- [ ] M416 world pickup 후 PrimaryA 또는 PrimaryB 장착
- [ ] P92 world pickup 후 Pistol slot 장착
- [ ] Pan world pickup 후 Melee slot 장착
- [ ] 무기 교체 pickup 시 기존 무기 world item drop
- [ ] drop weapon이 loaded ammo 보존

### Input

- [ ] PrimaryA/B/Pistol/Melee InputAction으로 active slot 변경
- [ ] Unequip InputAction으로 active slot `None`
- [ ] empty slot 입력 실패 feedback
- [ ] 기존 weapon raw key가 Equipment state를 우회하지 않음

### Reload

- [ ] 5.56mm pickup 후 M416 reload
- [ ] 9mm pickup 후 P92 reload
- [ ] 7.62mm pickup 후 Kar98k reload
- [ ] ammo 없음 reload 실패
- [ ] reload 중 switch/unequip/fire/death 취소

### Fire

- [ ] M416 full-auto projectile
- [ ] M416 owner predicted visual과 server confirm/reject
- [ ] P92 semi-auto projectile
- [ ] Kar98k hitscan bolt-action
- [ ] muzzle transform 기준 spawn/trace
- [ ] damage는 server에서만 적용

### Melee/Throw

- [ ] active weapon 없음 상태 barehand melee 유지
- [ ] Pan non-aim attack melee damage
- [ ] Pan aim+attack throw
- [ ] throw 후 Pan 장비/소유 제거
- [ ] Pan 착지 후 world item 회수

### Network/UI

- [ ] owner loaded/reserve ammo UI 갱신
- [ ] remote client equipped weapon visual 확인
- [ ] remote client가 owner inventory/reserve ammo를 보지 않음
- [ ] failure reason은 owning client에만 표시

## 남은 리스크

- M416/Kar98k/Pan 실제 mesh asset이 없으면 gameplay 검증은 placeholder mesh로 진행될 수 있음
- Anim montage, recoil, ADS pose polish는 초기 수용 기준 밖이므로 fire feel이 단순할 수 있음
- projectile prediction은 단순 confirm/reject 구조라 고지연 환경의 피격 판정 불만은 남을 수 있음
- DataTable/uasset reimport와 Blueprint component 설정은 C++ build만으로 검증되지 않음
- 기존 inventory/world item 수동 검증 미완료 항목이 weapon pickup/drop 검증에도 영향을 줄 수 있음
