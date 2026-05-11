# 무기 시스템 명세

## 목표

Dedicated Server 멀티플레이 환경에서 PUBG 스타일 무기, 탄약, 근접 무기 흐름을 인벤토리/장비 시스템 위에 완성한다.

성공 기준:

- 월드에 떨어진 무기, 탄약, 근접 무기를 서버 검증으로 줍기
- 탄약은 일반 인벤토리에 stack으로 저장하고, 장전 시 active weapon의 탄약 tag와 수량을 서버에서 검증해 소모
- 무기 장착, 무기 교체, 손에서 내리기 동작은 `InputAction` 기반으로 처리
- 무기별 fire spec은 C++ 하드코딩이 아니라 DataTable row 기준으로 조회
- 총알이 나가는 위치, 장착 mesh, socket/transform은 각 Equipped Weapon Blueprint에서 authoring
- 바닥 pickup/drop mesh와 display transform은 각 World Item Blueprint에서 authoring
- 원본 PUBG 구성에 맞춘 초기 대표 무기만 구현: AR 1개, SR 1개, Pistol 1개, Melee 1개, 대응 탄약
- 기존 `UBG_InventoryComponent`, `UBG_EquipmentComponent`, `ABG_WorldItemBase` 구조를 가능한 한 유지
- 기존 WON `UBG_WeaponFireComponent`는 유지하되 책임을 DataTable, equipped weapon actor, inventory ammo 기반으로 재정의

비목표:

- 모든 PUBG 무기, 부착물, 조준경, 탄속/낙차/관통/반동 밸런스 전체 구현
- SaveGame persistence
- GAS 도입
- Widget에서 gameplay state 직접 변경
- 기존 Health, DBNO, Parkour, BlueZone 대형 리팩터

## 정의

- Weapon item: `EBG_ItemType::Weapon`과 `Item.Weapon.*` GameplayTag로 식별되는 보유/장비 데이터
- World item: 바닥 pickup/drop 상태의 아이템 Actor. 무기와 비무기 모두 기존 `ABG_WorldItemBase` 흐름으로 표현
- Equipped weapon actor: 캐릭터 socket에 붙는 `ABG_EquippedWeaponBase` 계열 Actor. 장착 mesh, muzzle, fire FX 위치와 무기 인스턴스 런타임 상태를 표현
- Fire spec: 무기별 damage, range, fire mode, cooldown, spread, pellet, ammo cost, trace/projectile 정책을 담은 DataTable row
- Active weapon slot: 현재 손에 들고 입력을 받는 `EBG_EquipmentSlot`
- Unequip: 장비 슬롯에서 제거하지 않고 active slot만 `None`으로 바꿔 손에서 내리는 동작
- Drop: 장비/인벤토리에서 제거하고 월드 아이템으로 스폰하는 동작

## 현재 저장소 분석

분석 기준: 2026-05-04, `d:\Dev\P4\PUBG_HotMode`

| 영역 | 현재 구현 상태 | 요구사항 대비 판정 |
| --- | --- | --- |
| Item tag | `Config/Tags/BG_InventoryItems.ini`에 `Item.Ammo.556`, `Item.Ammo.762`, `Item.Weapon.AR`, `Item.Weapon.SR`, `Item.Weapon.Pistol`, `Item.Weapon.Melee` 등록 | 예시 수준. leaf tag가 실제 PUBG 무기명이 아니며 Pistol 대응 탄약도 부정확 |
| Item DataTable | `Content/GEONU/Data/Source/Items_Weapon.csv`, `Items_Ammo.csv` 존재 | 장비/탄약 기본 필드는 있음. fire spec, equipped actor class, muzzle 정보 없음 |
| Registry | `UBG_ItemDataRegistry`와 `UBG_ItemDataRegistrySubsystem`가 DataTable lookup 제공 | 재사용 가능 |
| Inventory | `UBG_InventoryComponent`가 owner-only FastArray, stack, weight, drop 처리 | 탄약 보관 기반 구현됨 |
| Equipment | `UBG_EquipmentComponent`가 PrimaryA/B, Pistol, Melee, Throwable, Armor, Backpack, active slot 관리 | 장비 골격 구현됨. active slot 변경용 client request/InputAction 흐름 부족 |
| World item | `ABG_WorldItemBase`가 pickup/drop, range, registry 검증, equipment 교체 처리 | 골격 구현됨. 무기/탄약 WorldItem Blueprint authoring은 대부분 없음 |
| Weapon fire | WON `UBG_WeaponFireComponent`가 server line trace, semi/full auto, ammo state, reload timer 처리 | 유지 대상. fire spec 하드코딩, eye view trace, reload fallback, ammo state 소유 책임을 재정의해야 함 |
| Reload | active weapon row의 `AmmoItemTag`, `MagazineSize`, `ReloadDuration`를 읽고 Inventory ammo를 제거 | 기본 연결 있음. 단, 탄약 없음/제거 실패 시 code-only reload fallback 존재 |
| Input | Attack/Reload/Interact/Inventory는 InputAction으로 연결 | 무기 교체/해제는 `One`, `Two`, `Zero` raw key가 `SetWeaponState` 직접 호출 |
| Character adapter | `UBG_EquipmentComponent::ApplyActiveWeaponStateToCharacter()`가 active weapon을 `EBGWeaponPoseType`로 반영 | 재사용 가능. 단, 임시 입력이 adapter를 우회함 |
| Melee | 미장착 상태에서 barehand melee sweep 공격 존재 | Melee weapon item은 장착 가능하지만 active 공격/mesh/spec이 없음 |
| Blueprint mesh | `BP_WorldItem_Bandage`만 명시적 world item BP 확인 | 무기/탄약/근접무기 BP authoring 필요 |

주요 결론:

- 인벤토리, 장비, 월드 아이템, ViewModel 기반은 이미 무기 시스템의 기반으로 사용 가능
- 요구한 “무기별 DataTable fire spec”은 아직 미구현. 현재 WON `UBG_WeaponFireComponent`의 `PistolFireSettings`, `RifleFireSettings`, `ShotgunFireSettings`가 생성자에서 하드코딩됨
- 요구한 “총알이 나가는 위치 등 mesh 관련 설정은 각 Actor Blueprint”는 아직 미구현. 현재 발사는 캐릭터 eye view에서 라인트레이스함
- 요구한 “InputAction 기반 무기 교체와 장착 해제”는 아직 미구현. 현재 숫자 raw key가 EquipmentComponent를 우회함
- 요구한 “인벤토리 탄약 소모 장전”은 부분 구현됨. 다만 fallback 때문에 탄약 없이도 장전될 수 있어 최종 요구사항과 충돌
- World item과 equipped weapon actor는 분리하는 편이 DataTable과 lifecycle 관리에 더 적합함. pickup/drop, 수량, drop 상태의 loaded ammo, 서버 검증은 `ABG_WorldItemBase`가 담당하고, equipped 상태의 loaded ammo, muzzle/attach, fire FX는 `ABG_EquippedWeaponBase`가 담당
- 무기 전용 interface는 당장 필요하지 않음. equipped weapon actor는 상속 기반 virtual function과 Blueprint event로 확장하고, World item은 기존 `ABG_WorldItemBase` 계약을 재사용
- Projectile 예측은 GAS의 Local Predicted 개념만 차용함. 클라이언트는 즉시 시각 결과를 예측하고 서버가 승인/거절하지만, GAS 의존성과 복잡한 prediction key stack은 도입하지 않음
- 초깃값은 PUBG Wiki에 정리된 분석 수치를 그대로 seed로 사용함. DataTable row마다 별도 출처/조정 메타데이터는 두지 않고, 테스트에서 어색한 일부 수치만 직접 조정
- 신규 projectile actor와 fire spec 타입은 GEONU 영역에 작성하되, 캐릭터 전투 입력/발사 adapter는 기존 WON `UBG_WeaponFireComponent`를 유지해 개조함

## 확정 결정

- Item identity는 기존처럼 `EBG_ItemType` + `FGameplayTag` 유지
- 기존 category tag는 category로 유지하고, DataTable row는 실제 구현 무기 leaf tag 사용
- 초기 대표 무기는 `M416`, `Kar98k`, `P92`, `Pan`으로 확정
- M416 end-to-end 구현을 최우선 개발 slice로 둠
- 초기 대표 무기 tag:

```text
Item.Weapon.AR.M416
Item.Weapon.SR.Kar98k
Item.Weapon.Pistol.P92
Item.Weapon.Melee.Pan
Item.Ammo.556
Item.Ammo.762
Item.Ammo.9mm
```

- 기존 `Item.Weapon.AR`, `Item.Weapon.SR`, `Item.Weapon.Pistol`, `Item.Weapon.Melee`는 분류 tag로만 사용
- Fire spec은 별도 DataTable `DT_WeaponFireSpecs`로 분리하고 RowName은 weapon item tag와 동일
- Weapon item row는 장비/인벤토리 계약을, Fire spec row는 발사 계약을 담당
- 기존 WON `UBG_WeaponFireComponent`는 Character의 전투 adapter로 유지
- `UBG_WeaponFireComponent`는 active equipment slot, `ABG_EquippedWeaponBase`, weapon item row, fire spec row를 조회하는 책임으로 재정의
- `UBG_WeaponFireComponent`의 기존 hardcoded fire settings와 code-only reload fallback은 제거 또는 fallback 전용 debug path로 격리
- Character의 Attack/Reload/SetWeaponState/OnRep/Death 경로는 `UBG_WeaponFireComponent`를 호출하되, EquipmentComponent와 equipped actor state를 우회하지 않아야 함
- World item class는 공통 item row의 `WorldItemClass`로 관리
- Equipped weapon class는 weapon item row의 `EquippedWeaponClass`로 관리
- Equipped weapon actor base는 `AActor`를 상속한 `ABG_EquippedWeaponBase`로 구성
- Equipped weapon actor가 equipped 상태의 무기 인스턴스 런타임 상태 source of truth를 소유
- `UBG_EquipmentComponent`는 weapon slot, active slot, equipped actor lifecycle, item tag mapping을 관리하고 최종 구조에서 loaded ammo를 소유하지 않음
- `FBG_WeaponItemDataRow::MagazineSize`, `AmmoItemTag`는 기본 authoring data이고, equip 시 `ABG_EquippedWeaponBase` 런타임 상태에 캐시
- `ABG_WorldItemBase::WeaponLoadedAmmo`는 drop/pickup 이동 중 loaded ammo 보존용이며, equip 완료 후 source of truth가 아님
- loaded ammo owner-only replication 여부는 현 단계에서 설계 제약으로 두지 않음. 정확한 탄 수 비공개가 필요해지면 별도 네트워크 hardening 범위에서 처리
- Weapon-specific interface는 도입하지 않음
- 각 무기는 world item Blueprint와 equipped weapon Blueprint를 분리
- 각 world item Blueprint는 world mesh, pickup collision, world display transform을 소유
- 각 equipped weapon Blueprint는 equipped mesh, muzzle component/socket, attach transform을 소유
- Inventory UI preview용 transform/key는 WeaponSystem 전투 로직이 아니라 `InventoryEquipmentUI` 구현 범위에서 추가
- 발사는 서버 권위로 처리하고, muzzle transform은 active equipped weapon actor에서 가져옴
- M416, P92는 projectile actor 방식으로 구현
- Kar98k는 hitscan line trace 방식으로 구현
- Pan은 일반 공격 시 melee weapon으로 동작하고, 조준 상태에서 공격 입력 시 투척 weapon으로 동작
- Pan 투척 시 장비/인벤토리에서 제거되고, 투척 projectile이 정지하면 `WorldItemClass` 기준 회수 가능한 world item으로 전환
- 맨손 전투는 무기 active 없음 상태에서 유지
- 무기 초깃값은 PUBG Wiki 분석 수치를 적용하고, 테스트에서 어색한 일부 수치만 DataTable에서 조정
- DataTable에는 별도 출처/조정 메타데이터 필드를 두지 않음
- Projectile 예측은 단순 `ShotPredictionId` 기반 구조로 구현
- 클라이언트는 총구 FX, 탄 궤적, 임시 피격 FX를 예측하고 데미지, 체력, 킬, 확정 hit marker는 예측하지 않음
- 장전은 Inventory ammo가 없으면 실패해야 하며 code-only fallback은 제거 대상
- 무기 손해제는 active slot만 `None`으로 만들고 장비 슬롯의 equipped weapon actor와 actor 내부 loaded ammo는 유지
- Drop은 기존 `RequestDropEquipment` 흐름을 유지해 월드 아이템으로 스폰

## 기능 명세

### 아이템과 태그

- GameplayTag는 category와 concrete item을 분리
- DataTable row는 concrete item tag만 사용
- `Items_Weapon.csv`는 generic `Item.Weapon.AR` row를 제거하거나 migration row로만 두고 실제 row를 leaf tag로 대체
- Pistol row의 `AmmoItemTag`는 `Item.Ammo.9mm`처럼 실제 무기 탄종과 일치
- Melee row는 `EquipSlot=Melee`, `WeaponPoseCategory=None`을 유지하되 별도 melee spec/actor로 공격 가능해야 함

### 무기 데이터

Weapon item row 필수 필드:

- `ItemType=Weapon`
- `ItemTag`
- `DisplayName`
- `WorldItemClass`
- `EquipSlot`
- `WeaponPoseCategory`
- `AmmoItemTag`
- `MagazineSize`
- `ReloadDuration`
- `TacticalReloadDuration`
- `SingleBulletInitialReloadDuration`
- `SingleBulletRepeatReloadDuration`
- `EquippedWeaponClass`

Fire spec row 필수 필드:

- `WeaponItemTag`
- `FireMode`: `SemiAuto`, `FullAuto`, `BoltAction`, `Melee`
- `FireImplementation`: `Projectile`, `Hitscan`, `Melee`
- `Damage`
- `Range`
- `FireCooldown`
- `AmmoCost`
- `PelletCount`
- `SpreadAngleDegrees`
- `TraceChannel`
- `ProjectileClass`
- `ProjectileInitialSpeed`
- `ProjectileGravityScale`
- `bCanThrow`
- `ThrowProjectileClass`
- `ThrowMaxDistance`
- `ThrowDamageMax`
- `ThrowDamageMin`
- `ThrowDamageFalloffStartDistance`
- `DebugDrawDuration`

검증 규칙:

- Fire spec RowName은 `WeaponItemTag.GetTagName()`과 일치
- Weapon item row가 있으면 Fire spec row도 있어야 함
- `AmmoCost > 0`인 무기는 `AmmoItemTag`가 유효해야 함
- Melee fire spec은 `AmmoCost=0`, `FireMode=Melee`
- M416과 P92는 `FireImplementation=Projectile`
- Kar98k는 `FireImplementation=Hitscan`
- Pan은 `FireImplementation=Melee`, `bCanThrow=true`
- `MagazineSize > 0`인 무기는 reload 가능
- DataTable은 PUBG Wiki 분석 수치를 초기값으로 넣고 별도 출처/조정 메타데이터 필드는 두지 않음

초기 DataTable seed:

| Weapon | Item tag | Ammo | Fire | Impl | Damage | Mag | Cooldown | Reload | Projectile speed | Throw |
| --- | --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| M416 | `Item.Weapon.AR.M416` | `Item.Ammo.556` | FullAuto | Projectile | 40 | 30 | 0.086s | 3.1s | 880m/s | 없음 |
| Kar98k | `Item.Weapon.SR.Kar98k` | `Item.Ammo.762` | BoltAction | Hitscan | 79 | 5 | 1.900s | 4.0s | 시각 탄도 760m/s | 없음 |
| P92 | `Item.Weapon.Pistol.P92` | `Item.Ammo.9mm` | SemiAuto | Projectile | 34 | 15 | 0.100s | 2.0s | 380m/s | 없음 |
| Pan | `Item.Weapon.Melee.Pan` | 없음 | Melee | Melee | body 80, head 120 | 0 | 0.75s | 없음 | 없음 | 30m, 90 to 40 damage |

보조 수치:

- M416 tactical reload: 2.4s
- P92 tactical reload: 1.7s
- Kar98k partial reload: initial 1.690s, repeat 0.750s
- Pan throw damage falloff: 15m까지 최대 피해, 15m부터 30m까지 90에서 40으로 감소
- UE 단위 저장 시 거리와 탄속은 cm 기준으로 변환

### World Item Blueprint와 Equipped Weapon Blueprint

World item base가 제공해야 하는 기능:

- `ABG_WorldItemBase`의 item state, pickup validation, drop flow 재사용
- world mesh root
- pickup 가능 상태 조회

World item Blueprint 책임:

- world mesh 지정
- pickup collision 지정
- 바닥 표시용 relative transform 지정
- 무기, 탄약, 소모품 모두 동일한 `WorldItemClass` 계약 사용

Equipped weapon base가 제공해야 하는 기능:

- equipped mesh root
- muzzle transform 조회
- hand/back attach transform 조회
- weapon item tag, ammo item tag, current loaded ammo, max magazine size 런타임 상태 조회
- 서버 권위 loaded ammo 설정/소모 API
- owner character 설정
- fire FX hook
- reload/equip hook
- 무기 전용 interface 없이 C++ virtual function과 BlueprintImplementableEvent로 무기별 차이 확장

Equipped weapon Blueprint 책임:

- equipped mesh 지정
- muzzle socket/component 지정
- 총구, 탄피, fire VFX/SFX 위치 지정
- 손 장착 transform과 등/홀스터 보관 transform 지정

실패 처리:

- active equipped weapon actor가 없으면 fire/reload 시작 실패
- muzzle transform을 얻을 수 없으면 fire 실패와 error log
- Blueprint class 로드 실패 시 pickup/equip 실패 또는 fallback 여부를 명시적으로 로그

### 월드 아이템과 줍기

흐름:

```text
Interact InputAction
  -> ABG_Character::InteractFromInput
  -> pickup actor validation
  -> Inventory 또는 Equipment
  -> successful pickup montage
  -> replicated state / failure RPC
```

규칙:

- Ammo pickup은 `UBG_InventoryComponent::TryAddItem`로 처리
- Weapon pickup은 `UBG_EquipmentComponent::TryEquipWeapon`로 처리
- 비무기 stack item pickup은 기존 `ABG_WorldItemBase::TryPickup` 흐름을 유지
- weapon pickup은 `ABG_WorldItemBase`가 동일한 서버 검증 계약을 제공하고, 성공 시 `EquippedWeaponClass`로 장착 actor를 생성
- Primary weapon은 빈 PrimaryA, 빈 PrimaryB, active primary, PrimaryA 순으로 장착 대상 선택
- 장착 중인 무기 교체 시 기존 무기는 loaded ammo를 보존해 해당 item row의 `WorldItemClass`로 drop
- Melee weapon pickup은 Melee slot에 장착
- pickup 성공 시 world item 수량 차감 또는 destroy

### 장착, 교체, 손해제

입력은 raw key가 아니라 `InputAction`으로만 처리한다.

필수 액션:

- `IA_BG_WeaponPrimaryA`
- `IA_BG_WeaponPrimaryB`
- `IA_BG_WeaponPistol`
- `IA_BG_WeaponMelee`
- `IA_BG_UnequipWeapon`
- 기존 `IA_BG_Reload`
- 기존 `IA_BG_Attack`
- 기존 `IA_BG_AimAction`
- 기존 `IA_BG_Interact`

흐름:

```text
InputAction
  -> ABG_PlayerController
  -> ABG_Character request method
  -> UBG_EquipmentComponent server request
  -> TryActivateWeapon(slot) 또는 TryActivateWeapon(None)
  -> ApplyActiveWeaponStateToCharacter
  -> UBG_WeaponFireComponent sync
```

규칙:

- slot이 비어 있으면 실패와 owning client failure feedback
- `Unequip`은 equipped item을 제거하지 않고 active slot만 `None`
- weapon switch 중이면 reload와 automatic fire를 취소
- 기존 `One`, `Two`, `Zero` 직접 key bind는 제거 대상
- `ABG_Character::SetWeaponState` 직접 호출은 Equipment adapter 내부로만 제한

### 발사

흐름:

```text
Attack Started/Completed
  -> UBG_WeaponFireComponent
  -> active slot + weapon item row + fire spec row
  -> equipped weapon actor muzzle transform
  -> ammo check
  -> server trace/projectile
  -> damage
  -> active equipped weapon actor loaded ammo update
  -> fire animation/FX replication
```

규칙:

- firing 가능 여부는 Character state, active weapon, fire spec, ammo, cooldown을 모두 검증
- `FireMode=SemiAuto`는 Started에서 1회 발사
- `FireMode=FullAuto`는 Started부터 cooldown 주기로 반복, Completed에서 정지
- `FireMode=BoltAction`은 1회 발사 후 cooldown/rechamber 시간 동안 재발사 불가
- `FireMode=Melee`는 ammo 없이 melee trace/sweep
- active weapon이 Pan이고 조준 중이면 `IA_BG_Attack`은 melee swing 대신 throw를 실행
- `FireImplementation=Projectile`이면 서버가 projectile actor를 muzzle transform에서 spawn
- `FireImplementation=Hitscan`이면 서버가 muzzle transform에서 line trace 수행
- M416은 full-auto projectile actor를 우선 구현
- P92는 semi-auto projectile actor로 구현
- Kar98k는 bolt-action hitscan으로 구현
- Pan은 non-aim attack에서 melee swing, aim + attack에서 thrown melee projectile로 구현
- 총기 loaded ammo는 active `ABG_EquippedWeaponBase` runtime state가 source of truth
- 발사 성공 시 active equipped weapon actor의 서버 권위 ammo consume API로 장탄 수 갱신
- Magazine empty면 fire 실패. 자동 장전은 하지 않음

### Projectile

설계 원칙:

- GAS의 Local Predicted 철학을 차용하되 GAS dependency는 추가하지 않음
- 초심자도 추적 가능한 `ShotPredictionId` 기반 단순 구조 사용
- edge case 보정, 복잡한 rewind, prediction key chain은 초기 범위에서 제외

클라이언트 예측 흐름:

```text
Local Attack
  -> ShotPredictionId 증가
  -> active equipped weapon actor muzzle transform 조회
  -> predicted visual projectile / tracer / muzzle FX 생성
  -> ServerRequestFire(slot, ShotPredictionId, muzzle transform, aim direction, random seed)
  -> server validation
  -> authoritative projectile 또는 hitscan 처리
  -> owning client confirm/reject
  -> predicted visual 정리 또는 보정
```

`ShotPredictionId` 역할:

- owning client가 발사 입력마다 1씩 증가시키는 local serial number
- client가 만든 predicted visual projectile/tracer/FX와 server 응답을 짝짓는 영수증 번호
- server는 요청을 처리한 뒤 같은 `ShotPredictionId`로 confirm 또는 reject를 돌려줌
- client는 confirm을 받으면 해당 predicted visual을 server 결과에 맞춰 제거하거나 자연스럽게 유지 후 종료
- client는 reject를 받으면 해당 predicted visual을 제거하고 ammo UI를 server state로 되돌림
- `ShotPredictionId`는 damage, hit 판정, 보안 검증의 근거로 사용하지 않음
- 예시: client가 `ShotPredictionId=41`로 M416 탄 시각 효과를 먼저 만들면, server reject `41` 수신 시 그 탄 효과만 제거하고 `42`, `43` 효과는 건드리지 않음

예측 대상:

- owner client 총구 FX, 발사음, recoil/camera shake
- M416/P92 predicted visual projectile와 궤적
- Kar98k hitscan tracer와 임시 impact FX
- Pan throw predicted visual projectile와 trajectory arc
- 임시 impact decal/particle. 서버 결과 수신 전까지 confirmed hit marker로 표현하지 않음
- owner UI의 장탄 수는 즉시 감소 표시 가능하되 active weapon actor runtime state로 최종 보정

예측하지 않는 대상:

- 데미지 계산과 적용
- 체력, 사망, DBNO, kill feed
- 확정 hit marker
- inventory reserve ammo 소모
- 영구 decal, loot 생성, item 회수 가능 상태

서버 권위 규칙:

- projectile actor는 서버에서 spawn하고 replicated movement 또는 hit result replication으로 원격 클라이언트에 표시
- projectile damage는 서버 hit/collision에서만 적용
- projectile owner, instigator, ignored actor는 발사 캐릭터로 설정
- projectile class, initial speed, gravity scale은 Fire spec에서 조회
- server request는 active slot, weapon tag, ammo, cooldown, reload state, death state, muzzle transform 허용 오차를 검증
- projectile spawn 실패 시 ammo는 rollback하거나 발사 자체를 실패로 처리
- server confirm은 `ShotPredictionId`, final muzzle transform, accepted loaded ammo count를 owning client에 반환
- server reject는 `ShotPredictionId`와 실패 사유를 owning client에 반환하고 predicted visual을 제거
- M416 projectile을 최초 기준 구현으로 삼고, P92와 Pan throw는 같은 prediction/confirm 구조를 재사용

### 장전

흐름:

```text
Reload InputAction
  -> active weapon validation
  -> weapon item row AmmoItemTag
  -> Inventory ammo quantity
  -> reload timer
  -> completion revalidation
  -> Inventory ammo consume
  -> active equipped weapon actor loaded ammo update
```

규칙:

- active weapon 없음: 실패
- magazine full: 실패
- `AmmoItemTag` invalid: 실패
- Inventory ammo 없음: `MissingAmmo` 실패
- 완료 시 필요한 탄약과 보유 탄약 중 작은 값만 장전
- 완료 시 `TryRemoveItem(Ammo, AmmoItemTag, AmmoToLoad)`가 실패하면 장전 실패
- 장전 완료 시 active equipped weapon actor의 서버 권위 ammo set API로 loaded ammo 갱신
- code-only refill fallback 금지
- reload 중 fire, weapon switch, unequip, death, item use interrupt 시 reload 취소

### 근접 무기

규칙:

- Melee slot에 `Item.Weapon.Melee.Pan`을 장착 가능
- active slot이 Melee이면 barehand 공격이 아니라 melee weapon fire spec 사용
- melee equipped weapon Blueprint가 mesh와 손 attach transform을 제공
- damage/range/cooldown은 Fire spec row에서 조회
- 장착 무기 없음 상태에서는 기존 barehand melee 유지
- Pan 장착 중 조준하지 않은 `IA_BG_Attack`은 melee sweep으로 처리
- Pan 장착 중 `IA_BG_AimAction` 유지 상태의 `IA_BG_Attack`은 throw로 처리
- Pan throw는 장비 slot과 inventory possession에서 Pan을 제거한 뒤 server authoritative thrown projectile을 spawn
- Pan throw projectile은 30m 이내에서 유효하고 15m까지 최대 피해, 15m 이후 30m까지 피해 감소
- Pan throw projectile이 벽, 바닥, 캐릭터에 맞거나 속도가 충분히 낮아지면 Pan item row의 `WorldItemClass`로 회수 가능한 world item을 spawn
- 회수는 일반 weapon pickup과 동일하게 `ABG_WorldItemBase` pickup flow를 사용
- Pan 방탄/도탄 기능은 초기 범위에서 제외

### UI와 ViewModel

규칙:

- Inventory UI는 `UBG_InventoryViewModel` 경유 request만 호출
- ViewModel은 equipment slot 표시, active slot, loaded ammo, nearby world item 표시를 제공
- ViewModel의 loaded ammo는 equipped weapon actor runtime state 또는 이를 감싼 read-only query API에서 읽음
- reserve ammo와 magazine size의 정확한 무기 로직은 현 단계에서 보류하고 Inventory UI에서는 더미 표시를 허용
- Inventory UI weapon slot 클릭은 active weapon 변경 request를 보내지 않음
- active weapon 변경과 unequip은 WeaponSystem input action, Character request API, EquipmentComponent request API 경로에서 처리
- Inventory UI drag/drop은 PrimaryA/B weapon slot 상호 교체와 장착 weapon 바닥 drop을 지원
- Inventory UI weapon preview는 `EquippedWeaponClass` preview actor를 local-only로 capture하며 preview transform/key는 `InventoryEquipmentUI` 구현 범위에서 추가
- 현 단계에서 preview용 스킨/부착물 runtime state는 없는 것으로 간주
- Widget은 `UBG_EquipmentComponent` state를 직접 변경하지 않음

### 네트워크

- 모든 장비, 장전, 발사, pickup, drop state mutation은 서버 권위
- client RPC는 request 전용
- owner client의 fire/throw는 visual-only prediction을 허용
- `ShotPredictionId`는 owning client와 server 사이의 confirm/reject 매칭에만 사용
- 원격 클라이언트는 public equipment state와 equipped weapon actor visual만 봄
- 원격 클라이언트는 predicted projectile이 아니라 server replicated projectile/fire event만 봄
- owner 클라이언트는 자신의 inventory quantity와 UI 표시용 weapon ammo 값을 봄
- 데미지, health, kill, item spawn/회수 가능 상태는 서버 결과만 반영
- failure는 owning client RPC로 전달
- null/invalid state는 silent fail 금지, 명시적 error log

## 수용 기준

- Dedicated Server + 2 Clients PIE에서 탄약 world item pickup 후 owner inventory 수량 갱신
- 무기 world item pickup 후 적절한 Equipment slot에 장착되고 원격 클라이언트에 무기 visual 표시
- `IA_BG_WeaponPrimaryA/B/Pistol/Melee`로 active slot 변경
- `IA_BG_UnequipWeapon`으로 장비는 유지한 채 손에서 무기 내림
- Inventory UI weapon slot 클릭으로 active slot이 변경되지 않음
- Inventory UI PrimaryA/B drag/drop으로 두 weapon slot이 교체되고 바닥/drop 영역 drag/drop으로 weapon world item이 생성됨
- 장전 시 Inventory ammo가 감소하고 active equipped weapon actor loaded ammo가 증가
- Inventory ammo가 없으면 장전 실패하며 active equipped weapon actor loaded ammo가 증가하지 않음
- M416은 projectile actor 기반 hold fire로 동작
- M416 owner client는 발사 즉시 predicted projectile/FX를 보고 서버 confirm/reject로 정리
- P92는 projectile actor 기반 single fire로 동작
- Kar98k는 hitscan 기반 single/bolt-action 정책으로 동작
- fire spec 수치를 C++ 재컴파일 없이 DataTable 수정으로 바꿀 수 있음
- 발사 trace/projectile 시작 위치가 캐릭터 eye view가 아니라 active equipped weapon actor muzzle transform 기준
- Melee weapon active 상태에서 primary action이 melee weapon damage/range/cooldown 사용
- Pan 장착 후 조준하지 않고 공격하면 melee, 조준 중 공격하면 투척
- Pan 투척 시 장비/인벤토리에서 제거되고, 착지 후 world item으로 회수 가능
- 기존 barehand melee는 무기 active 없음 상태에서 유지
- 기존 Inventory pickup/drop/use 흐름이 regression 없이 동작

## 프로젝트 맥락

관련 코드:

- `Source/PUBG_HotMode/WON/Public/Combat/BG_WeaponFireComponent.h`
- `Source/PUBG_HotMode/WON/Private/Combat/BG_WeaponFireComponent.cpp`
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponProjectileBase.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponProjectileBase.cpp`
- `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.h`
- `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.cpp`
- `Source/PUBG_HotMode/GEONU/Combat/BG_WeaponFireTypes.h`
- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_PlayerController.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_PlayerController.cpp`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemDataRow.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_ItemDataRegistry.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_InventoryComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_EquipmentComponent.h`
- `Source/PUBG_HotMode/GEONU/Inventory/BG_WorldItemBase.h`
- `Source/PUBG_HotMode/GEONU/UI/BG_InventoryViewModel.h`

참고용 legacy 코드:

- `Source/PUBG_HotMode/WON/Public/Combat/BG_WeaponFireComponent.h`
- `Source/PUBG_HotMode/WON/Private/Combat/BG_WeaponFireComponent.cpp`

관련 content/data:

- `Config/Tags/BG_InventoryItems.ini`
- `Content/GEONU/Data/Source/Items_Weapon.csv`
- `Content/GEONU/Data/Source/Items_Ammo.csv`
- `Content/GEONU/Data/Source/WeaponFireSpecs.csv`
- `Content/GEONU/Data/DT_Items_Weapon.uasset`
- `Content/GEONU/Data/DT_Items_Ammo.uasset`
- `Content/GEONU/Data/DT_WeaponFireSpecs.uasset`
- `Content/GEONU/Data/DA_ItemDataRegistry.uasset`
- `Content/WON/Input/IA_BG_Attack.uasset`
- `Content/WON/Input/IA_BG_AimAction.uasset`
- `Content/WON/Input/IA_BG_Reload.uasset`
- `Content/WON/Input/IA_BG_Interact.uasset`
- `Content/WON/Input/IA_BG_WeaponPrimaryA.uasset`
- `Content/WON/Input/IA_BG_WeaponPrimaryB.uasset`
- `Content/WON/Input/IA_BG_WeaponPistol.uasset`
- `Content/WON/Input/IA_BG_WeaponMelee.uasset`
- `Content/WON/Input/IA_BG_UnequipWeapon.uasset`
- `Content/WON/Input/IMC_BG.uasset`

검증 명령:

```powershell
& "<UE_ROOT>\Engine\Build\BatchFiles\Build.bat" PUBG_HotModeEditor Win64 Development -Project="D:\Dev\P4\PUBG_HotMode\PUBG_HotMode.uproject" -WaitMutex
```

수동 검증:

- Dedicated Server + 2 Clients PIE
- Owner client: pickup, switch, unequip, reload, fire prediction, melee, Pan throw/recover
- Remote client: equipped weapon visual, server fire animation/FX, pickup/drop world item replication

## 후속 조정 포인트

- DataTable 초깃값은 PUBG Wiki 분석 수치를 그대로 넣고, PIE 테스트에서 체감이 어색한 일부 수치만 조정
- lag compensation/server rewind는 초기 범위 제외. 명중 판정 불만이 확인되면 별도 spec으로 작성
- skins, attachments, recoil pattern, armor별 damage multiplier, Pan bullet block은 초기 범위 제외

## 참고 자료

- PUBG Wiki M416: `https://pubg.wiki.gg/wiki/M416`
- PUBG Wiki Kar98k: `https://pubg.wiki.gg/wiki/Karabiner_98_Kurz`
- PUBG Wiki P92: `https://pubg.wiki.gg/wiki/P92`
- PUBG Wiki Pan: `https://pubg.wiki.gg/wiki/Pan`
- PUBG Console Update 5.2 melee throw notes: `https://www.pubg.com/en/news/1723`
- Unreal Gameplay Abilities Local Predicted 개념: `https://dev.epicgames.com/documentation/unreal-engine/using-gameplay-abilities-in-unreal-engine`
- Unreal ability activation prediction states: `https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/GameplayAbilities/EGameplayAbilityActivationMode__-`
