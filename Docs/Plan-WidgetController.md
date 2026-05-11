# ViewModel 기반 HUD 재설계

## 배경

기존 `UBG_PlayerHealthWidgetConnector`는 모든 attribute를 하나의 `FBG_PlayerHealthWidgetData`로 묶어 처리한다.
attribute가 추가될 때마다 구조체, Connector, Widget 전부 수정해야 한다.

**목표**: UI Widget을 attribute별로 분리 + 단일 `UBG_HealthViewModel`(데이터 브릿지)를 통해 각 Widget이 독립적으로 데이터를 수신.

---

## 제약

- Tick / Timer / Property Binding 기반 polling 금지
- Widget이 PlayerState를 직접 조회하지 않음
- ViewModel는 Widget 생성을 담당하지 않음 (순수 데이터 브릿지)
- 현재 실제 연결: `ABG_PlayerState`의 `CurrentHP`, `MaxHP`, `bIsDead`
- 나머지 attribute는 더미 값

---

## 구조

```
                                    [ViewModel]
[ABG_PlayerState] ──delegate──>      (on PlayerController)
                                    순수 데이터 브릿지
                                           │
                    ┌──────────┬───────────┼───────────┬──────────┐
                    │          │           │           │          │
               OnHealth   OnBoost     OnDBNO     OnArmor    OnBreath
               Changed    Changed     Changed    Changed    Changed
                    │          │           │           │          │
                    ▼          ▼           ▼           ▼          ▼
         [WBP_PlayerHUD] ── 부모 Widget ─────────────────────────────
              ├── WBP_HealthGauge      (HealthPercent, bIsDead)
              ├── WBP_BoostGauge       (BoostPercent)
              ├── WBP_DBNOGauge        (DBNOPercent, bIsDBNO)
              ├── WBP_HelmetStatus     (HelmetLevel)
              ├── WBP_VestStatus       (VestLevel)
              ├── WBP_BackpackStatus   (BackpackLevel)
              ├── WBP_BoostRunStatus   (bHasBoostRun)
              ├── WBP_RegenStatus      (bHasRegeneration)
              └── WBP_BreathGauge      (BreathPercent, bShouldShowBreath)
```

---

## 핵심 원칙

1. **ViewModel**: PlayerController에 부착. PlayerState delegate 구독 → attribute별 typed delegate 재방송. Widget 생성 책임 없음
2. **부모 WBP (`WBP_PlayerHUD`)**: sub-widget들을 레이아웃으로 조합. 외부에서 생성하여 Viewport에 추가
3. **Sub-widget**: 각각 하나의 attribute만 담당. `NativeConstruct`에서 ViewModel를 찾아 해당 delegate만 구독
4. **Polling 금지**: 모든 갱신은 push (delegate) 기반

---

## 역할 분담

| 클래스                      | 책임                                                    | 위치                     |
| --------------------------- | ------------------------------------------------------- | ------------------------ |
| `ABG_PlayerState`           | HP/사망 변경 시 delegate broadcast                      | WON (기존, 변경 없음)    |
| `UBG_HealthViewModel` | PlayerState 구독 → attribute별 delegate 재방송 + Getter | GEONU (신규)             |
| `WBP_PlayerHUD`             | sub-widget 레이아웃 조합                                | Content/GEONU (신규, BP) |
| `WBP_HealthGauge`           | HealthPercent, bIsDead 시각화                           | Content/GEONU (신규, BP) |
| `WBP_BoostGauge`            | BoostPercent 시각화                                     | Content/GEONU (신규, BP) |
| `WBP_DBNOGauge`             | DBNOPercent, bIsDBNO 시각화                             | Content/GEONU (신규, BP) |
| `WBP_HelmetStatus`          | HelmetLevel 시각화                                      | Content/GEONU (신규, BP) |
| `WBP_VestStatus`            | VestLevel 시각화                                        | Content/GEONU (신규, BP) |
| `WBP_BackpackStatus`        | BackpackLevel 시각화                                    | Content/GEONU (신규, BP) |
| `WBP_BoostRunStatus`        | bHasBoostRun 시각화                                     | Content/GEONU (신규, BP) |
| `WBP_RegenStatus`           | bHasRegeneration 시각화                                 | Content/GEONU (신규, BP) |
| `WBP_BreathGauge`           | BreathPercent, bShouldShowBreath 시각화                 | Content/GEONU (신규, BP) |

---

## ViewModel 설계

ViewModel는 attribute별로 **delegate + Getter** 쌍을 노출한다.
내부에 attribute 데이터를 캐시하여 diff 비교 후 변경 시에만 broadcast.

### Delegate 목록

| Delegate                | 파라미터                                      | 트리거                 |
| ----------------------- | --------------------------------------------- | ---------------------- |
| `OnHealthChanged`       | `float HealthPercent, bool bIsDead`           | HP 또는 사망 상태 변경 |
| `OnBoostChanged`        | `float BoostPercent`                          | Boost 변경             |
| `OnDBNOChanged`         | `float DBNOPercent, bool bIsDBNO`             | DBNO 변경              |
| `OnHelmetChanged`       | `int32 HelmetLevel`                           | 헬멧 변경              |
| `OnVestChanged`         | `int32 VestLevel`                             | 조끼 변경              |
| `OnBackpackChanged`     | `int32 BackpackLevel`                         | 배낭 변경              |
| `OnBoostRunChanged`     | `bool bHasBoostRun`                           | 부스트 달리기 변경     |
| `OnRegenerationChanged` | `bool bHasRegeneration`                       | 재생 변경              |
| `OnBreathChanged`       | `float BreathPercent, bool bShouldShowBreath` | 숨 변경                |

### Getter 목록 (초기값 조회용)

각 delegate와 1:1 대응하는 `BlueprintPure` Getter 함수.
Sub-widget이 `NativeConstruct`에서 delegate 구독 전 현재 값을 가져갈 때 사용.

---

## 데이터 흐름

```
[서버] ApplyDamage() → CurrentHP 변경 → OnRep_CurrentHP() → PS.OnHealthChanged.Broadcast()
[클라] 복제 수신     →                   OnRep_CurrentHP() → PS.OnHealthChanged.Broadcast()
  ↓
ViewModel.HandleHealthChanged()
  → HealthPercent 계산, 캐시 비교
  → 변경 시 OnHealthChanged.Broadcast(HealthPercent, bIsDead)
  ↓
WBP_HealthGauge: OnHealthChanged 수신 → Health bar 시각 갱신
```

---

## Sub-widget ↔ ViewModel 바인딩

각 sub-widget은 `NativeConstruct`(또는 BP Event Construct)에서 ViewModel를 찾아 바인딩:

```
Sub-widget::NativeConstruct()
  → GetOwningPlayer() → FindComponentByClass<UBG_HealthViewModel>()
  → 해당 attribute delegate 구독
  → Getter로 초기값 적용
```

Sub-widget은 ViewModel 외의 gameplay 객체를 참조하지 않음.

---

## 기존 코드 처리

| 기존 파일                            | 처리                                                                                                                |
| ------------------------------------ | ------------------------------------------------------------------------------------------------------------------- |
| `GEONU/BG_PlayerHealthWidget.h/.cpp` | 삭제. `UBG_PlayerHealthWidget`, `UBG_PlayerHealthWidgetConnector`, `FBG_PlayerHealthWidgetData` 모두 새 구조로 대체 |
| `WON/.../BG_PlayerState.h/.cpp`      | 변경 없음 (이미 delegate 존재)                                                                                      |
| `WON/.../BG_PlayerController.h/.cpp` | ViewModel 부착 필요 (BP에서 추가 or C++ 생성자)                                                                     |

---

## 파일 목록

| 경로                                          | 역할                       | 상태      |
| --------------------------------------------- | -------------------------- | --------- |
| `GEONU/HUD/BG_ViewModel.h`                    | ViewModel 헤더             | 신규      |
| `GEONU/HUD/BG_ViewModel.cpp`                  | ViewModel 구현             | 신규      |
| `GEONU/BG_PlayerHealthWidget.h/.cpp`          | —                          | 삭제      |
| `Content/GEONU/Blueprints/WBP_PlayerHUD`      | 부모 HUD (sub-widget 조합) | 신규 (BP) |
| `Content/GEONU/Blueprints/WBP_HealthGauge`    | Health 시각화              | 신규 (BP) |
| `Content/GEONU/Blueprints/WBP_BoostGauge`     | Boost 시각화               | 신규 (BP) |
| `Content/GEONU/Blueprints/WBP_DBNOGauge`      | DBNO 시각화                | 신규 (BP) |
| `Content/GEONU/Blueprints/WBP_HelmetStatus`   | 헬멧 시각화                | 신규 (BP) |
| `Content/GEONU/Blueprints/WBP_VestStatus`     | 조끼 시각화                | 신규 (BP) |
| `Content/GEONU/Blueprints/WBP_BackpackStatus` | 배낭 시각화                | 신규 (BP) |
| `Content/GEONU/Blueprints/WBP_BoostRunStatus` | 부스트달리기 시각화        | 신규 (BP) |
| `Content/GEONU/Blueprints/WBP_RegenStatus`    | 재생 시각화                | 신규 (BP) |
| `Content/GEONU/Blueprints/WBP_BreathGauge`    | 숨 시각화                  | 신규 (BP) |

---

## 구현 순서

1. `UBG_HealthViewModel` — PlayerState 구독, attribute별 delegate + Getter
2. 기존 `BG_PlayerHealthWidget.h/.cpp` 삭제
3. PlayerController에 ViewModel 부착 (BP에서 Component 추가)
4. `WBP_PlayerHUD` + 각 sub-widget BP 생성
5. sub-widget에서 ViewModel 바인딩 (BP Event Construct)

---

## 향후 확장

| 시스템 추가 시                  | 변경 위치                                                    |
| ------------------------------- | ------------------------------------------------------------ |
| `UBGHealthComponent` (gameplay) | ViewModel — Component delegate로 전환                        |
| 새 attribute (예: Stamina)      | ViewModel에 delegate/Getter 추가 + 새 sub-widget BP          |
| 기존 attribute 삭제             | 해당 delegate 제거 + sub-widget 삭제 (다른 widget 영향 없음) |

모든 확장은 **ViewModel에 delegate 추가 + 새 sub-widget BP 생성**으로 귀결.
기존 sub-widget은 변경 불필요.
