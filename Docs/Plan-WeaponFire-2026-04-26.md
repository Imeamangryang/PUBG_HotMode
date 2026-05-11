# Weapon Fire System Plan

## 목표

- `ABG_Character`가 총을 장착한 상태에서 서있음, 앉음, 엎드림, 점프 중에도 발사할 수 있게 만든다.
- 총기 발사 판정은 서버 권한으로 수행하고, 결과는 모든 클라이언트가 디버그로 확인할 수 있게 만든다.
- 이후 인벤토리/총기 액터/탄약 시스템이 붙어도 구조를 크게 바꾸지 않도록 발사 로직을 컴포넌트로 분리한다.

## 범위

- 캐릭터 입력의 `PrimaryAction`에서 무기 장착 상태면 발사를 시도한다.
- 발사 전용 `ActorComponent`를 추가해 라인 트레이스, 데미지 적용, 디버그 표시를 담당시킨다.
- 총 종류는 `Pistol`, `Rifle`, `Shotgun`을 기준으로 파라미터를 분리한다.
- 디버그용으로 데미지, 사거리, 탄퍼짐, 발사 간격, 디버그 스피어 크기를 조절 가능하게 한다.

## 구현 원칙

- 데미지는 서버에서만 실제 적용한다.
- 클라이언트는 발사 요청만 보내고, 결과는 멀티캐스트 디버그로 본다.
- 총 발사 상태 판단은 캐릭터의 기존 상태 플래그(`bCanFire`, `bIsWeaponEquipped`, `EquippedWeaponPoseType`)를 재사용한다.
- 자세별 발사 제한은 두지 않는다. 총을 들고 있으면 어떤 자세에서도 발사 가능하게 둔다.

## 파일 변경 계획

- `Source/PUBG_HotMode/WON/Public/Combat/BG_WeaponFireComponent.h`
- `Source/PUBG_HotMode/WON/Private/Combat/BG_WeaponFireComponent.cpp`
- `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`
- `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`

## 구현 순서

1. 발사 전용 컴포넌트 클래스 추가
2. 총 종류별 발사 설정 구조체 추가
3. 서버 권한 발사 처리와 멀티캐스트 디버그 구현
4. 캐릭터의 `Req_PrimaryAction()`에서 무기 장착 여부에 따라 근접 공격 또는 총 발사로 분기
5. 로그와 디버그 스피어로 클라이언트/서버 양쪽에서 결과가 보이도록 확인

## 검증 기준

- 무기 미장착 상태에서는 기존 근접 공격이 그대로 동작한다.
- 무기 장착 상태에서는 이동/점프/자세와 관계없이 발사가 된다.
- 서버에서만 데미지가 적용되고, 클라이언트는 디버그만 본다.
- 히트 시 빨간색 디버그 스피어와 로그가 보인다.
- 미스 시도 디버그도 남아서 사거리와 조준 방향을 확인할 수 있다.

