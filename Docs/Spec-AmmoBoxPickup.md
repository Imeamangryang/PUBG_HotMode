# Spec - Ammo Box Pickup

## 목표

탄약 상자 월드 아이템을 주우면 서버 검증을 거쳐 소유 캐릭터 인벤토리에 예비 탄약이 추가되고, 장전 완료 시 해당 예비 탄약을 소모해 장착 무기의 `LoadedAmmo`에 반영한다.

성공 기준:

- Dedicated Server + 2 Clients PIE에서 5.56mm/7.62mm 탄약 상자 줍기 성공
- Owner client 인벤토리에 `Item.Ammo.556` 또는 `Item.Ammo.762` 수량 증가
- 탄약 상자 월드 아이템 수량 감소 또는 0개 시 destroy
- 인벤토리 여유 무게가 부족하면 가능한 수량만 부분 줍기, 0개 가능 시 `Overweight` 실패
- M416 장전 시 `Item.Ammo.556` 소모 후 `LoadedAmmo` 증가
- Kar98k 장전 시 `Item.Ammo.762` 소모 후 `LoadedAmmo` 증가
- 예비 탄약이 없으면 장전 실패, code-only refill로 장탄 증가 금지
- `bUseInfiniteDebugAmmo=true`인 weapon actor는 debug option으로 inventory ammo 없이 장전 가능
- 개발/테스트용 탄약 공급은 Unreal Editor Console Command가 인벤토리에 탄약을 추가하는 방식을 기본으로 처리
- `PUBG.InfAmmo <bool>` Console Command로 현재 장착 무기의 `bUseInfiniteDebugAmmo`를 런타임 토글
- 원격 클라이언트에서 탄약 상자 pickup/drop world item 상태가 일관되게 보임

초기 범위:

- 5.56mm 탄약 상자: `Item.Ammo.556`
- 7.62mm 탄약 상자: `Item.Ammo.762`
- 기존 inventory, world item, reload C++ 계약 재사용
- 탄약 상자 world item Blueprint와 CSV/DataTable 연결

제외:

- 9mm 탄약 상자와 9mm world pickup 구현
- 탄약 분할 줍기 UI
- 탄약 버리기 UI 신규 작업
- 탄피 배출, projectile visual, bullet mesh 기반 VFX
- 탄약 타입별 사운드, outline, 근처 아이템 정렬 UI 고도화

## 정의

- Ammo item: `EBG_ItemType::Ammo`와 `Item.Ammo.*` GameplayTag로 식별되는 일반 인벤토리 스택
- Ammo box world item: `ABG_WorldItemBase` Blueprint subclass가 표시하는 줍기 가능한 월드 액터
- Reserve ammo: `UBG_InventoryComponent`에 저장된 탄약 수량
- Loaded ammo: `ABG_EquippedWeaponBase`가 소유하고 복제하는 현재 탄창 수량
- Ammo inventory slot: 탄약 한 스택을 표시하는 inventory entry 또는 UI slot
- Ammo item row: `Data/GEONU/Items_Ammo.csv`와 `DT_Items_Ammo`의 `FBG_AmmoItemDataRow`

제약:

- 모든 pickup, inventory mutation, reload ammo consume은 서버 권위
- 일반 인벤토리는 owner-only 복제
- World item actor는 `ABG_WorldItemBase` 공통 계약 사용
- 카테고리별 C++ child class 추가 금지
- null check 후 silent fail 금지, 기존 규칙대로 명시적 error log
- `WorldItemClass`는 row soft class override로만 선택
- 탄약 `MaxStackSize`는 한 inventory slot 기준 100
- 인벤토리 총 탄약량은 100개 단위 여러 스택과 무게 제한으로 결정

전제:

- 기본 탄약 상자 수량은 배치 액터 또는 Blueprint default `Quantity=30`으로 확정
- 5.56mm/7.62mm 월드 아이템만 이번 범위에서 authoring
- 9mm 탄약 상자와 world pickup source는 구현하지 않음
- P92 strict reload는 `PUBG.Give Item.Ammo.9mm <count>`로 9mm을 공급하지 않는 한 `MissingAmmo`가 정상 결과
- `Content/GEONU/Models/Bullet`의 총알 mesh는 이번 pickup 수용 기준에 포함하지 않고, 발사체/VFX/프리뷰 후속 범위에서 사용

## 분석

현재 구조:

- `Data/GEONU/Items_Ammo.csv`에 `Item.Ammo.556`, `Item.Ammo.762`, `Item.Ammo.9mm` row 존재
- `Config/Tags/BG_InventoryItems.ini`에 `Item.Ammo.556`, `Item.Ammo.762`, `Item.Ammo.9mm` 등록됨
- `ABG_WorldItemBase::TryPickup`은 `Ammo`를 stack item으로 라우팅
- `UBG_InventoryComponent::IsRegularInventoryItemType`은 `Ammo`를 일반 인벤토리 대상으로 허용
- `UBG_InventoryComponent::Auth_AddItem`은 무게 제한에 따라 accepted quantity만 추가 가능
- `UBG_WeaponFireComponent::CompleteReload`는 기본적으로 `AmmoItemTag`가 유효하고 inventory ammo 제거가 성공한 경우에만 `LoadedAmmo`를 증가
- `ABG_EquippedWeaponBase`의 `bUseInfiniteDebugAmmo` 기본값은 false이며, true일 때만 inventory ammo 제거 없이 debug reload 가능

선택지:

| 선택지 | 내용 | 장점 | 한계 | 결정 |
| --- | --- | --- | --- | --- |
| Data/BP만 추가 | Ammo row에 icon/world class를 연결하고 BP 생성 | pickup 최소 구현 빠름 | infinite debug ammo가 켜져 있으면 장전 소모 검증 불가 | 부분 채택 |
| Data/BP + strict reload | BP 연결과 함께 debug ammo 기본 비활성화 | 요구사항 전체 충족 | 기존 디버그 장전 의존 테스트/플레이 영향 가능 | 채택 |
| `PUBG.Give` console command | editor console command로 inventory item 추가 | strict reload 계약 유지와 테스트 편의 동시 확보. Ammo 외 일반 인벤토리 아이템 테스트로 확장 가능 | console command 경로도 서버 권위 검증 필요 | 채택 |
| `PUBG.InfAmmo` console command | 현재 장착 무기의 `bUseInfiniteDebugAmmo` runtime flag 토글 | strict reload 기본값을 유지하면서 디버그 무한탄을 필요 시 명시적으로 켬 | active weapon이 없으면 실패해야 함 | 채택 |
| Ammo 전용 C++ actor | 탄약 상자 전용 subclass 추가 | 타입별 확장 쉬움 | 기존 Spec의 공통 WorldItem 계약과 중복 | 보류 |

결정:

- 탄약 상자는 `ABG_WorldItemBase` Blueprint subclass로 구현
- `Items_Ammo.csv`의 5.56/7.62 row에 전용 icon과 `WorldItemClass` 지정
- `DT_Items_Ammo`와 `DA_ItemDataRegistry`가 5.56/7.62 row를 런타임에서 조회 가능해야 함
- strict reload 전환 시 디버그 무한탄은 기본 비활성화
- 개발/테스트 탄약은 Unreal Editor Console Command가 서버 권위 `Auth_AddItem` 경로로 inventory ammo를 추가하는 방식으로 공급
- `PUBG.InfAmmo <bool>`는 현재 장착 무기의 `bUseInfiniteDebugAmmo`를 서버 권위로 변경하고 ammo 표시 상태를 갱신
- 9mm world pickup은 후속 범위가 아니라 미구현 범위로 유지

## 명세

### 데이터

`Data/GEONU/Items_Ammo.csv` 목표 row:

| Name | ItemType | ItemTag | Icon | WorldItemClass | UnitWeight | bStackable | MaxStackSize |
| --- | --- | --- | --- | --- | ---: | --- | ---: |
| `Item.Ammo.556` | `Ammo` | `Item.Ammo.556` | `Texture2D'/Game/GEONU/Textures/T_Bullet_556.T_Bullet_556'` | `Class'/Game/GEONU/Blueprints/Items/BP_WorldItem_Ammo556.BP_WorldItem_Ammo556_C'` | 0.5 | true | 100 |
| `Item.Ammo.762` | `Ammo` | `Item.Ammo.762` | `Texture2D'/Game/GEONU/Textures/T_Bullet_762.T_Bullet_762'` | `Class'/Game/GEONU/Blueprints/Items/BP_WorldItem_Ammo762.BP_WorldItem_Ammo762_C'` | 0.7 | true | 100 |
| `Item.Ammo.9mm` | `Ammo` | `Item.Ammo.9mm` | 기존 값 유지 | 빈 값 유지 | 0.375 | true | 100 |

DataTable import 규칙:

- RowName은 `ItemTag.GetTagName()`과 동일
- `ItemType=Ammo`
- `bStackable=true`
- `MaxStackSize=100`
- `UnitWeight >= 0`
- `WorldItemClass`가 지정된 row는 `ABG_WorldItemBase` child class로 로드 가능
- 9mm row는 기존 tag/weapon 계약 보존용이며 world item class를 지정하지 않음

### 월드 아이템 Blueprint

생성 대상:

- `/Game/GEONU/Blueprints/Items/BP_WorldItem_Ammo556`
- `/Game/GEONU/Blueprints/Items/BP_WorldItem_Ammo762`

공통 설정:

- Parent class: `ABG_WorldItemBase`
- `ItemType=Ammo`
- `Quantity=30` 기본값
- 한 상자 pickup 수량 30발은 한 ammo slot의 최대 100개보다 작으므로 빈 인벤토리에서는 단일 스택에 들어감
- Replicates enabled
- World mesh component가 pickup overlap에 잡히도록 Query collision 활성
- `ABG_Character::IsWeaponInteractableActor`가 `ABG_WorldItemBase`를 우선 허용하므로 별도 actor class는 불필요

5.56mm 설정:

- `ItemTag=Item.Ammo.556`
- Static mesh: `/Game/GEONU/Models/AmmoBox556/StaticMeshes/SM_AmmoBox556`
- Icon: `/Game/GEONU/Textures/T_Bullet_556`

7.62mm 설정:

- `ItemTag=Item.Ammo.762`
- Static mesh: `/Game/GEONU/Models/AmmoBox762/StaticMeshes/SM_AmmoBox762`
- Icon: `/Game/GEONU/Textures/T_Bullet_762`

### 줍기 흐름

```text
IA_BG_Interact
  -> ABG_Character::InteractFromInput
  -> ABG_Character::TryInteractWithCurrentTarget
  -> ABG_Character::PickupWorldItem(WorldItem, WorldItem->GetQuantity())
  -> Server_PickupWorldItem
  -> ABG_WorldItemBase::TryPickup
  -> TryPickupStackItem
  -> UBG_InventoryComponent::Auth_AddItem(Ammo, AmmoItemTag, PickupQuantity)
  -> ConsumePickedUpQuantity(AddedQuantity)
  -> NotifySuccessfulPickup(Ammo)
```

검증:

- World item valid
- 요청 캐릭터 valid
- 요청 캐릭터 controller valid
- `RequestedQuantity > 0`
- world item `Quantity > 0`
- `ItemType=Ammo`
- `ItemTag` valid
- registry row 조회 성공
- `PickupInteractionRange` 이내
- inventory weight room 존재

부분 줍기:

- `CanAddItem`이 무게 기반으로 accepted quantity를 계산
- accepted quantity가 1 이상이면 해당 수량만 인벤토리에 추가
- world item은 추가된 수량만 감소
- world item 수량이 0 이하가 되면 destroy
- accepted quantity가 0이면 `Overweight` 실패
- 기존 ammo slot이 100개에 도달하면 `FBG_InventoryList::AddQuantity`가 같은 탄약의 새 stack entry를 생성

실패 전달:

- invalid row: `InvalidItem`
- invalid quantity: `InvalidQuantity`
- range 초과: `TooFar`
- 무게 초과: `Overweight`
- 기타 서버 검증 실패: `ServerRejected`

### 장전 흐름

```text
IA_BG_Reload
  -> ABG_Character::ReloadFromInput
  -> UBG_WeaponFireComponent::RequestReload
  -> active weapon context 조회
  -> weapon row AmmoItemTag 조회
  -> inventory reserve ammo 조회
  -> reload timer 시작
  -> completion revalidation
  -> UBG_InventoryComponent::Auth_RemoveItem(Ammo, AmmoItemTag, AmmoToLoad)
  -> ABG_EquippedWeaponBase::FinishWeaponReload(ConsumedInventoryAmmo)
  -> UBG_EquipmentComponent::Auth_LoadAmmo
```

규칙:

- `AmmoItemTag`가 invalid이면 장전 실패
- `LoadedAmmo >= MagazineCapacity`이면 장전 실패
- inventory reserve ammo가 0이면 `MissingAmmo` 계열 실패로 처리하고 `LoadedAmmo` 유지
- 완료 시 장전량은 `min(MissingAmmo, InventoryAmmoCount)`
- 완료 직전 active weapon, weapon slot, inventory component를 재검증
- inventory ammo 제거 실패 시 reload 취소, `LoadedAmmo` 유지
- `bUseInfiniteDebugAmmo=true`이면 inventory ammo 제거를 건너뛰고 `MissingAmmo`만큼 장전
- code-only refill은 이번 기능 완료 기준에서 허용하지 않음

무기별 연결:

| Weapon | Ammo row | Magazine | 완료 기준 |
| --- | --- | ---: | --- |
| M416 | `Item.Ammo.556` | 30 | 5.56mm reserve ammo 소모 |
| Kar98k | `Item.Ammo.762` | 5 | 7.62mm reserve ammo 소모 |
| P92 | `Item.Ammo.9mm` | 15 | 9mm world pickup 없음. `PUBG.Give Item.Ammo.9mm <count>` 공급 시에만 strict reload 가능 |

### Console Command 아이템 공급

목적:

- 디버그 무한탄을 켜지 않은 strict reload 상태에서도 플레이테스트와 무기 검증을 빠르게 수행
- strict reload가 `bUseInfiniteDebugAmmo=true` debug option을 제외하면 inventory ammo를 source of truth로 사용하도록 유지
- ammo 외 일반 인벤토리 아이템도 같은 command 계약으로 테스트 가능하게 함

규칙:

- Unreal Editor Console Command 입력은 owning client world에서 시작하고 서버가 처리
- 명령 이름은 `PUBG.Give`
- 사용 형식은 `PUBG.Give <itemtag> <count>`
- 예시는 `PUBG.Give Item.Ammo.556 300`
- `<itemtag>`는 GameplayTag로 파싱하고 registry row 조회로 item type을 결정
- `<count>`는 양수 정수만 허용
- 초기 허용 item type은 일반 인벤토리 stack 대상인 `Ammo`, `Heal`, `Boost`, `Throwable`
- `Weapon`, `Armor`, `Backpack`은 장비/월드 드롭 정책이 달라 이번 command 범위에서 제외
- 서버는 대상 캐릭터의 `UBG_InventoryComponent::Auth_AddItem`으로 요청 item을 추가
- 수량은 테스트 편의용 큰 값으로 요청 가능하되, 실제 추가량은 가방 여유 무게로 제한
- 실제 저장은 row `MaxStackSize` 단위 stack entry로 분할
- console command 공급은 실제 월드 아이템 존재 여부만 검증하지 않음
- 일반 pickup과 console command 공급은 모두 inventory 무게 제한을 우회하지 않음
- console command 공급은 shipping 빌드 또는 대전 운영 환경에서 비활성화

### Console Command 무한탄 토글

목적:

- strict reload 기본 동작은 유지하면서, 필요할 때만 현재 장착 무기를 debug infinite ammo 상태로 전환
- 기존 weapon actor의 `bUseInfiniteDebugAmmo` 의미를 유지하고 런타임 확인/전환 수단 제공

규칙:

- 명령 이름은 `PUBG.InfAmmo`
- 사용 형식은 `PUBG.InfAmmo <bool>`
- `<bool>`은 `true/false`, `1/0`, `on/off`, `yes/no` 허용
- command target은 console world의 controlled `ABG_Character`; 없으면 world의 첫 `ABG_Character` fallback
- 서버는 현재 active equipped weapon actor의 `SetUseInfiniteDebugAmmo`를 호출
- `bUseInfiniteDebugAmmo`는 weapon actor runtime state로 replication되어 client-side reload 판단도 같은 값을 사용
- 토글 후 `UBG_WeaponFireComponent::RefreshAmmoStateFromEquipment`로 reserve ammo 표시를 즉시 갱신
- `true`이면 inventory ammo 제거 없이 missing magazine만큼 reload 가능하며 reserve ammo 표시는 debug 값으로 표시
- `false`이면 inventory ammo만 strict reload source로 사용
- active weapon이 없으면 명시적 error log 후 실패
- command는 shipping 빌드 또는 대전 운영 환경에서 비활성화

### UI와 표시

- Inventory UI는 registry row icon을 사용해 5.56/7.62 탄약 아이콘 표시
- Inventory UI에서 탄약 한 슬롯은 최대 100개 표시
- Inventory UI reserve ammo 수량은 owner inventory replication 이후 갱신
- Nearby world item 목록은 기존 `GetNearbyWorldItems()`와 ViewModel 흐름 재사용
- 3D bullet mesh preview는 이번 범위의 필수 UI가 아님

### 네트워크

- Client는 pickup/reload request만 보냄
- 서버만 inventory quantity, world item quantity, loaded ammo를 변경
- Inventory quantity는 owner-only로 복제
- World item quantity/destroy는 관련 클라이언트에 복제
- Equipped weapon `LoadedAmmo`는 replicated runtime state로 전파

## 수용 기준

- 5.56mm 탄약 상자 30발을 줍고 inventory `Item.Ammo.556` 수량이 30 증가
- 7.62mm 탄약 상자 30발을 줍고 inventory `Item.Ammo.762` 수량이 30 증가
- 5.56mm 탄약 100개 slot이 있는 상태에서 30발을 추가로 줍고 같은 탄약 새 stack이 30개로 생성
- 인벤토리 여유 무게가 10발분만 있으면 10발만 줍고 world item 수량 20 유지
- 인벤토리 여유 무게가 없으면 world item 수량 유지, owner client failure feedback 표시
- M416 empty magazine 상태에서 5.56mm 30발 보유 후 reload 완료 시 reserve ammo 30 감소, loaded ammo 30 증가
- Kar98k empty magazine 상태에서 7.62mm 3발 보유 후 reload 완료 시 reserve ammo 3 감소, loaded ammo 3 증가
- reserve ammo 0개 상태에서 reload 시 loaded ammo 증가 없음
- `PUBG.Give Item.Ammo.556 300` console command로 가방 여유 무게만큼 ammo를 inventory에 공급하고 strict reload 성공
- `PUBG.InfAmmo true` 입력 후 reserve ammo 0개 상태에서도 reload 가능, `PUBG.InfAmmo false` 입력 후 다시 strict reload 적용
- Dedicated Server + 2 Clients PIE에서 owner와 remote 모두 world item destroy/drop 상태 확인

## 프로젝트 맥락

관련 문서:

- `Docs/Spec-InventorySystem.md`
- `Docs/Spec-WeaponSystem.md`
- `Docs/Plan-InventorySystem.md`
- `Docs/Plan-WeaponSystem.md`

관련 코드:

- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemDataRow.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryComponent.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_WorldItemBase.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_WorldItemBase.cpp`
- `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.cpp`
- `Source/PUBG_HotMode/GEONU/Combat/BG_CombatConsoleCommands.cpp`
- `Source/PUBG_HotMode/WON/Public/Combat/BG_WeaponFireComponent.h`
- `Source/PUBG_HotMode/WON/Private/Combat/BG_WeaponFireComponent.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`

관련 데이터/에셋:

- `Data/GEONU/Items_Ammo.csv`
- `Data/GEONU/Items_Weapon.csv`
- `Content/GEONU/Data/DT_Items_Ammo.uasset`
- `Content/GEONU/Data/DA_ItemDataRegistry.uasset`
- `Content/GEONU/Models/AmmoBox556/StaticMeshes/SM_AmmoBox556.uasset`
- `Content/GEONU/Models/AmmoBox762/StaticMeshes/SM_AmmoBox762.uasset`
- `Content/GEONU/Models/Bullet/StaticMeshes/SM_Bullet_Bullet.uasset`
- `Content/GEONU/Textures/T_Bullet_556.uasset`
- `Content/GEONU/Textures/T_Bullet_762.uasset`

검증 명령:

```powershell
.\RunTests.ps1 PUBG_HotMode.GEONU.Inventory
.\RunTests.ps1 PUBG_HotMode.GEONU.Combat
```

수동 검증:

- Dedicated Server + 2 Clients PIE
- Owner client: 5.56/7.62 pickup, partial pickup, reload success/failure
- Owner client: ammo slot 100개 초과 시 stack 분리, `PUBG.Give Item.Ammo.556 300` console command item supply
- Remote client: world item 수량 감소, destroy, dropped item 표시
