# Plan - Ammo Box Pickup

## 기준

- 원본 Spec: `Docs/Spec-AmmoBoxPickup.md`
- Spec 검토 결과: 구현 차단 이슈 없음
- 구현 원칙: 기존 `ABG_WorldItemBase`, `UBG_InventoryComponent`, `FBG_InventoryList`, `UBG_WeaponFireComponent`, `ABG_EquippedWeaponBase` 계약 재사용
- 구현 전제: 탄약 상자 기본 수량 30발, ammo slot 최대 100개, 9mm world pickup 미구현, 디버그 무한탄 기본 off, `PUBG.Give <itemtag> <count>` item supply와 `PUBG.InfAmmo <bool>` debug toggle 도입

## 기술 설계

데이터 흐름:

```text
BP_WorldItem_Ammo556/762
  -> ABG_WorldItemBase::TryPickup
  -> UBG_InventoryComponent::Auth_AddItem
  -> FBG_InventoryList::AddQuantity(MaxStackSize=100)
  -> owner-only inventory replication

IA_BG_Reload
  -> UBG_WeaponFireComponent::TryStartReload
  -> inventory reserve ammo validation
  -> CompleteReload
  -> UBG_InventoryComponent::Auth_RemoveItem
  -> ABG_EquippedWeaponBase::FinishWeaponReload

Console Command: PUBG.Give <itemtag> <count>
  -> registered UE console command handler
  -> GameplayTag + item registry validation
  -> ABG_Character server request
  -> UBG_InventoryComponent::Auth_AddItem
  -> weight-limited stack item add
  -> normal strict reload consumes supplied ammo

Console Command: PUBG.InfAmmo <bool>
  -> registered UE console command handler
  -> bool argument validation
  -> ABG_Character server request
  -> active ABG_EquippedWeaponBase::SetUseInfiniteDebugAmmo
  -> UBG_WeaponFireComponent reserve ammo refresh
```

핵심 계약:

- `FBG_InventoryList::AddQuantity`는 이미 `MaxStackSize` 단위 기존 stack 채우기와 새 stack 생성을 지원
- 일반 pickup은 `UBG_InventoryComponent::Auth_AddItem`과 무게 제한 유지
- `PUBG.Give` item supply는 실제 world item 존재 여부만 생략하고 `Auth_AddItem` 무게 제한 유지
- strict reload는 `bUseInfiniteDebugAmmo=true` debug option을 제외하면 inventory ammo만 source of truth로 사용
- `PUBG.InfAmmo`는 active equipped weapon actor의 `bUseInfiniteDebugAmmo`를 서버 권위로 런타임 변경
- world item class 선택은 `Items_Ammo.csv`의 `WorldItemClass` soft class override 기준

의존성:

```text
Task 1 Data contract
  -> Task 2 Stack split regression
  -> Task 3 Inventory add contract
  -> Task 5 Console command
  -> Task 6 Tests
  -> Task 8 Final verification

Task 1 Data contract
  -> Task 4 Strict reload
  -> Task 6 Tests
  -> Task 8 Final verification

Task 1 Data contract
  -> Task 7 Blueprint/DataTable authoring
  -> Task 8 Final verification
```

체크포인트:

- Checkpoint A after Task 3: CSV, stack split, `Auth_AddItem` 직접 공급 계약 확정
- Checkpoint B after Task 5: strict reload와 console command supply가 같은 inventory source를 사용
- Checkpoint C after Task 7: runtime asset/data 연결 완료
- Checkpoint D after Task 8: Dedicated Server PIE 수용 기준 확인

## Task 1: Ammo 데이터 계약 갱신

Description: `Items_Ammo.csv`를 Spec 값으로 갱신하고 DataTable import 기준을 고정한다. 5.56/7.62는 icon과 world item class를 연결하고, 9mm은 world item 없이 tag/weapon 계약만 유지한다.

Acceptance:

- `Item.Ammo.556` icon이 `/Game/GEONU/Textures/T_Bullet_556`로 지정됨
- `Item.Ammo.762` icon이 `/Game/GEONU/Textures/T_Bullet_762`로 지정됨
- 5.56/7.62 `WorldItemClass`가 각각 `BP_WorldItem_Ammo556`, `BP_WorldItem_Ammo762`로 지정됨
- 모든 ammo row의 `MaxStackSize=100`
- 9mm row의 `WorldItemClass`는 빈 값

Verification:

```powershell
rg -n "Item.Ammo.(556|762|9mm)" Data/GEONU/Items_Ammo.csv Config/Tags/BG_InventoryItems.ini
```

Dependencies: none

Files likely touched:

- `Data/GEONU/Items_Ammo.csv`
- `Content/GEONU/Data/DT_Items_Ammo.uasset`
- `Content/GEONU/Data/DA_ItemDataRegistry.uasset`

Scope: S

## Task 2: Ammo stack split 회귀 테스트

Description: ammo slot 100개 제한이 `FBG_InventoryList::AddQuantity`의 기존 stack 채우기와 새 stack 생성으로 유지되는지 자동화 테스트로 고정한다.

Acceptance:

- 100개 미만 ammo stack에 추가 시 기존 stack을 먼저 채움
- 기존 stack이 100개에 도달하면 같은 ammo의 새 stack 생성
- 230개 추가 시 `100/100/30` 형태로 분할 가능
- `GetQuantity`는 여러 stack의 합산 수량 반환

Verification:

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.Inventory
```

Dependencies: Task 1

Files likely touched:

- `Source/PUBG_HotMode/GEONU/Inventory/Tests/BG_InventoryAmmoTests.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryTypes.cpp` only if test exposes a defect
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryTypes.h` only if test helper access is needed

Scope: S

## Task 3: Console command 공급용 inventory add 계약 정리

Description: `PUBG.Give` command 공급이 실제 world item 존재 여부만 생략하고 기존 서버 `Auth_AddItem` 경로를 사용하도록 inventory add 계약을 고정한다. `Auth_AddItem`은 무게 제한을 유지하고, row lookup이 중복되지 않도록 내부 검증을 공통화한다.

Acceptance:

- `Auth_AddItem`은 authority가 없으면 error log 후 실패
- `Auth_AddItem`은 item row와 `MaxStackSize`를 registry로 검증
- `Auth_AddItem`은 `Ammo`, `Heal`, `Boost`, `Throwable`만 허용
- `Auth_AddItem`은 `Weapon`, `Armor`, `Backpack`을 거부
- `Auth_AddItem`은 요청 수량 중 가방 여유 무게로 가능한 수량만 추가
- `Auth_AddItem`은 실제 world item actor 존재 여부를 검증하지 않음
- `Auth_AddItem`의 row lookup은 add 호출당 1회로 유지
- invalid item/tag/quantity는 명시적 error log 후 실패

Verification:

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.Inventory
```

Dependencies: Task 1, Task 2

Files likely touched:

- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryComponent.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/Tests/BG_InventoryAmmoTests.cpp`

Scope: M

## Task 4: Strict reload 전환

Description: code-only refill 경로를 제거하고, 일반 reload 시작/완료가 inventory reserve ammo를 기준으로 동작하도록 정리한다. `bUseInfiniteDebugAmmo=false`가 기본 strict reload이며, true일 때만 inventory 소모 없는 debug reload를 허용한다.

Acceptance:

- `ABG_EquippedWeaponBase`의 `bUseInfiniteDebugAmmo` 기본값은 false
- `bUseInfiniteDebugAmmo=true`이면 inventory ammo가 없어도 missing magazine만큼 reload 가능
- `bUseInfiniteDebugAmmo`는 runtime 변경 시 client reload 판단과 맞도록 replicated state
- `bUseInfiniteDebugAmmo=false`이면 `RefreshReserveAmmo`가 inventory quantity만 reserve ammo로 반영
- `bUseInfiniteDebugAmmo=false`이고 reserve ammo 0개이면 reload timer가 시작되지 않거나 완료 시 `LoadedAmmo`가 증가하지 않음
- `bUseInfiniteDebugAmmo=false`이면 `CompleteReload`가 `Auth_RemoveItem(Ammo, AmmoItemTag, AmmoToLoad)` 성공 후 `FinishWeaponReload`
- `bUseInfiniteDebugAmmo=true`이면 `CompleteReload`가 inventory ammo 제거를 건너뛰고 `FinishWeaponReload`
- code-only refill warning 경로 제거

Verification:

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.Combat
```

Dependencies: Task 1

Files likely touched:

- `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.cpp`
- `Source/PUBG_HotMode/WON/Public/Combat/BG_WeaponFireComponent.h`
- `Source/PUBG_HotMode/WON/Private/Combat/BG_WeaponFireComponent.cpp`
- `Source/PUBG_HotMode/GEONU/Combat/Tests/BG_EquippedWeaponBaseTests.cpp`

Scope: M

## Task 5: Console command 경로

Description: Unreal Editor Console Command에서 `PUBG.Give <itemtag> <count>`는 일반 인벤토리 item을 공급하고, `PUBG.InfAmmo <bool>`는 현재 장착 무기의 `bUseInfiniteDebugAmmo`를 런타임 토글한다. Console command 경로는 서버 권위로 처리하고, shipping 또는 운영 환경에서 비활성화한다.

Acceptance:

- `PUBG.Give` UE console command 등록
- command args는 `<itemtag> <count>` 순서로 파싱
- `<itemtag>`는 GameplayTag와 registry row로 검증
- `<count>`는 양수 정수로 검증
- controlled `ABG_Character`가 server request를 통해 item grant 요청
- item supply가 Task 3의 `Auth_AddItem` 경로를 사용
- item supply는 가방 여유 무게만큼만 추가
- invalid tag, unsupported item type, invalid count는 명시적 error log 후 실패
- console command 공급 후 일반 strict reload가 inventory ammo를 소모
- `PUBG.InfAmmo` UE console command 등록
- `PUBG.InfAmmo` command args는 `<bool>` 1개만 허용
- bool parser는 `true/false`, `1/0`, `on/off`, `yes/no` 허용
- controlled `ABG_Character`가 server request를 통해 active weapon debug flag 변경
- active weapon이 없으면 명시적 error log 후 실패
- 토글 후 `UBG_WeaponFireComponent::RefreshAmmoStateFromEquipment` 호출
- `PUBG.InfAmmo true`이면 inventory ammo 없이 reload 가능
- `PUBG.InfAmmo false`이면 strict reload로 복귀

Verification:

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.Inventory
.\RunTests.ps1 PUBG_HotMode.GEONU.Combat
```

Manual check:

- PIE owner client console에서 `PUBG.Give Item.Ammo.556 300` 입력
- `Item.Ammo.556` 수량이 가방 여유 무게만큼 증가
- PIE owner client console에서 `PUBG.Give Item.Heal.Bandage 5` 입력
- `Item.Heal.Bandage` 수량이 가방 여유 무게만큼 증가
- PIE owner client console에서 `PUBG.InfAmmo true` 입력 후 inventory ammo 0개 상태 reload 성공
- PIE owner client console에서 `PUBG.InfAmmo false` 입력 후 inventory ammo 0개 상태 reload 실패

Dependencies: Task 3, Task 4

Files likely touched:

- `Source/PUBG_HotMode/GEONU/Combat/BG_CombatConsoleCommands.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`

Scope: M

## Task 6: 자동화 테스트 통합

Description: 데이터, stack split, strict reload, `PUBG.Give` console command supply의 회귀를 자동화 테스트로 묶어 실행 가능한 검증 단위로 만든다. asset compile/PIE가 필요한 BP 동작은 Task 7/8에서 수동 검증으로 분리한다.

Acceptance:

- ammo stack 100개 초과 시 새 stack 생성 테스트
- 일반 add는 무게 제한으로 partial add 되는 테스트
- `PUBG.Give`가 호출하는 add 경로는 실제 world item 없이도 무게 제한 수량만 추가되는 테스트
- `PUBG.Give`는 invalid tag, non-positive count, 장비 item type을 거부하는 테스트
- strict reload는 reserve ammo를 제거하고 loaded ammo를 증가시키는 테스트
- reserve ammo 0개 reload 실패 테스트

Verification:

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.Inventory
.\RunTests.ps1 PUBG_HotMode.GEONU.Combat
```

Dependencies: Task 2, Task 3, Task 4, Task 5

Files likely touched:

- `Source/PUBG_HotMode/GEONU/Inventory/Tests/BG_InventoryAmmoTests.cpp`
- `Source/PUBG_HotMode/GEONU/Combat/Tests/BG_EquippedWeaponBaseTests.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/Tests/BG_ItemDataRegistryWeaponFireSpecTests.cpp`

Scope: M

## Task 7: 탄약 상자 Blueprint와 DataTable authoring

Description: 5.56/7.62 탄약 상자 BP를 만들고 `DT_Items_Ammo`를 재임포트해 runtime row와 world item class가 실제 asset으로 연결되도록 한다. 9mm BP는 생성하지 않는다.

Acceptance:

- `/Game/GEONU/Blueprints/Items/BP_WorldItem_Ammo556` 생성
- `/Game/GEONU/Blueprints/Items/BP_WorldItem_Ammo762` 생성
- 두 BP parent class가 `ABG_WorldItemBase`
- 두 BP default `ItemType=Ammo`, `Quantity=30`
- 5.56 BP mesh가 `SM_AmmoBox556`
- 7.62 BP mesh가 `SM_AmmoBox762`
- `DT_Items_Ammo` 재임포트 후 5.56/7.62 row가 icon/world class를 로드

Verification:

- Editor에서 두 BP compile
- Editor에서 `DT_Items_Ammo` row soft object/class 경로 확인
- PIE에서 world item overlap 감지 확인

Dependencies: Task 1

Files likely touched:

- `Content/GEONU/Blueprints/Items/BP_WorldItem_Ammo556.uasset`
- `Content/GEONU/Blueprints/Items/BP_WorldItem_Ammo762.uasset`
- `Content/GEONU/Data/DT_Items_Ammo.uasset`
- `Content/GEONU/Data/DA_ItemDataRegistry.uasset`

Scope: M

## Task 8: End-to-end 검증과 정리

Description: Dedicated Server + 2 Clients PIE에서 Spec 수용 기준을 확인하고, 실패 로그와 남은 regression 위험을 정리한다.

Acceptance:

- 5.56mm 탄약 상자 30발 pickup 후 owner inventory 수량 30 증가
- 7.62mm 탄약 상자 30발 pickup 후 owner inventory 수량 30 증가
- 100개 ammo stack 보유 상태에서 30발 pickup 시 새 30개 stack 생성
- 여유 무게 10발분 상태에서 10발만 pickup, world item 수량 20 유지
- 여유 무게 0 상태에서 pickup 실패와 world item 수량 유지
- M416 reload가 `Item.Ammo.556`을 소모하고 `LoadedAmmo` 증가
- Kar98k reload가 `Item.Ammo.762`를 소모하고 `LoadedAmmo` 증가
- reserve ammo 0 상태에서 reload로 `LoadedAmmo` 증가 없음
- 가방 여유 무게 확보 상태에서 `PUBG.Give Item.Ammo.556 300` console command supply 후 strict reload 성공
- remote client에서 world item destroy/drop 상태 확인

Verification:

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.Inventory
.\RunTests.ps1 PUBG_HotMode.GEONU.Combat
```

Manual check:

- Dedicated Server + 2 Clients PIE
- Owner inventory 수량, ammo stack, reload 전후 reserve/loaded ammo 기록
- Remote world item visibility 기록

Dependencies: Task 1, Task 2, Task 3, Task 4, Task 5, Task 6, Task 7

Files likely touched:

- `Docs/Plan-AmmoBoxPickup.md`
- `Docs/Spec-AmmoBoxPickup.md` only if implementation decisions change

Scope: S

## 병렬화

- Task 2와 Task 4는 Task 1 이후 병렬 가능
- Task 7은 Task 1 이후 코드 작업과 병렬 가능
- Task 3은 Task 2 이후 진행
- Task 5는 Task 3/4 이후 진행
- Task 6은 Task 2/3/4/5 이후 진행
- Task 8은 전체 통합 후 진행

## 위험과 대응

| 위험 | 영향 | 대응 |
| --- | --- | --- |
| console command가 실제 world item 없이 item을 추가 | 테스트용 공급 경로가 플레이 규칙과 혼동 | `Auth_AddItem` 재사용으로 무게 제한 유지, shipping 비활성화 |
| `PUBG.Give`가 장비 item까지 추가 | 장비/월드 드롭 정책 우회 | 초기 허용 타입을 `Ammo/Heal/Boost/Throwable`로 제한 |
| 디버그 무한탄이 기본 활성화 | reload consume 검증 불가 | `bUseInfiniteDebugAmmo=false` 기본값과 strict/debug reload 테스트 |
| BP soft class 경로 오타 | fallback actor spawn 또는 표시 누락 | CSV/DataTable import 후 runtime row 로드 확인 |
| DataTable 재임포트 누락 | CSV와 runtime 불일치 | `DT_Items_Ammo` row 수동 확인과 registry test |
| 9mm world pickup 미구현 혼동 | P92 reload 테스트 실패 | 9mm은 `PUBG.Give Item.Ammo.9mm <count>` 전용 검증으로 제한 |
| binary asset checkout 누락 | BP/DataTable 저장 실패 | 수정 전 `p4 edit/add` 확인 |

## 최종 검증 명령

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.Inventory
.\RunTests.ps1 PUBG_HotMode.GEONU.Combat
```

## 최종 수동 검증

- Dedicated Server + 2 Clients PIE
- Owner client: 5.56/7.62 pickup, partial pickup, overweight failure, 100개 stack 초과 split, strict reload, `PUBG.Give Item.Ammo.556 300` console command supply
- Remote client: ammo box 수량 감소, destroy, dropped item 표시
