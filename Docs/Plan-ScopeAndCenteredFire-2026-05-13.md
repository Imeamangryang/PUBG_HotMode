# Scope / Centered Fire Implementation Plan

## 목표

- 3인칭 오른쪽 어깨 카메라에서도 총알이 화면 정중앙 조준점으로 정확히 날아가도록 발사 로직을 개선한다.
- 총기별 길이 차이와 무관하게 `Muzzle` 소켓을 기준으로 실제 발사 방향을 계산한다.
- 이후 스코프 모드에서 같은 조준 정합성을 그대로 재사용할 수 있게 구조를 정리한다.
- 스코프 시스템은 `BG_Scope` 부모 액터, `SceneCapture2D`, `RenderTarget`, `ScopeUI` 기반으로 확장 가능하도록 코드 기반을 마련한다.

## 구현 단위

### 1. 발사 로직 개선

- 현재 카메라 시점으로 화면 정중앙 라인트레이스를 먼저 수행한다.
- 그 결과 지점 또는 최대 사거리 지점을 `AimTarget`으로 잡는다.
- 실제 탄도/히트 판정은 `Muzzle` 소켓 위치에서 `AimTarget`을 향하도록 다시 계산한다.
- 이렇게 하면 카메라가 오른쪽 어깨에 있어도 화면 정중앙과 총알이 맞춰진다.

### 2. 총기 연출 확장 지점 추가

- `BG_WeaponFireComponent`에 아래 훅을 추가한다.
  - 착탄 이펙트 / 나이아가라
  - Muzzle 플래시
  - 발사 사운드
  - 스코프 발사 시 오버레이 흔들림용 확장 지점
- 실제 에셋 재생은 코드에서 호출하고, 자산 지정은 에디터에서 하도록 한다.

### 3. Scope 부모 액터 기반 마련

- `BG_Scope` C++ 부모 액터 생성
- 내부 구성
  - Root
  - Scope mesh 또는 plane attach point
  - SceneCaptureComponent2D
  - RenderTarget 참조
- 활성화 / 비활성화 API 제공

### 4. 총기와 스코프 연결

- `ABG_EquippedWeaponBase`에
  - `bHasScope`
  - `bIsScoping`
  - 장착된 scope actor 참조
  - scope attach socket / class / magnification 정보
  를 추가한다.

### 5. 캐릭터 상태 확장

- `EBGCharacterState`에 스코프 모드 상태를 추가한다.
- 스코프 상태에서도 점프, 크라우치, 엎드리기, 리로드는 허용한다.
- 단, 리로드 시작 시 스코프를 해제하고 리로드 완료 후 자동 복귀하는 흐름을 만든다.

### 6. UI / 에디터 작업 연동

- ScopeUI 위젯은 C++에서 표시/숨김 진입점만 제공하고,
- 실제 렌더 타겟 머티리얼, 마스크, 크로스헤어, 블러 연출은 에디터에서 설정한다.

## 이번 턴 구현 우선순위

1. `BG_WeaponFireComponent`의 화면 정중앙 기준 발사 보정
2. 발사 이펙트/나이아가라 확장 포인트 추가
3. `ABG_EquippedWeaponBase`의 scope 장착 상태 기반 추가
4. `BG_Scope` 부모 액터 생성
5. Scope 모드용 상태 진입 기반 추가
6. 에디터 작업 가이드 문서화

## 리스크

- ScopeUI 전체를 코드만으로 완결하려 하면 에셋/머티리얼 작업과 충돌할 수 있다.
- 따라서 이번 구현은 `코드 기반 + 에디터 연결 포인트`까지를 명확히 만들고,
- 시각 자산 세팅은 단계적으로 붙이는 것이 안전하다.

