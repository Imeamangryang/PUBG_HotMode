# BG_PlayerHealthHUD 설계

## 제약

- Tick / Timer / Property Binding 기반 polling 금지
- Widget이 PlayerState를 직접 조회하지 않음
- 결합도 최소화, 의존 방향 단방향
- 현재 실제 연결: `ABG_PlayerState`의 `CurrentHP`, `MaxHP`, `bIsDead`
- 나머지(Boost, DBNO, 장비, 숨참기)는 더미 값
- 수정 범위: GEONU 중심 + WON 최소 변경

---

## 핵심 판단

- `ABG_PlayerState`는 `CurrentHP`, `bIsDead`를 복제하지만 UI용 delegate가 없음
- polling 없이 자동 갱신하려면 외부에서 이벤트를 발행해야 함
- **결정: `ABG_PlayerState`의 빈 `OnRep` 함수에 delegate broadcast 추가 (최소 침습)**

---

## 구조

```
[ABG_PlayerState]  ──delegate──>  [UBG_HealthHUDConnector]
                                       │
                                  RenderData 구성
                                       │
                                       ▼
                              [UBG_PlayerHealthWidget]
                                       │
                                       ▼
                              [WBP_PlayerHealthHUD (BP)]
```

Presenter 레이어 없이, Connector가 RenderData를 직접 구성하여 Widget에 push.
현재 변환 로직이 `CurrentHP / MaxHP` 나눗셈 수준이므로 별도 Presenter는 과잉.
복잡도가 증가하면 그때 추출.

---

## 역할 분담

| 클래스 | 책임 | 위치 |
|--------|------|------|
| `ABG_PlayerState` | HP/사망 변경 시 delegate broadcast | WON (수정) |
| `UBG_HealthHUDConnector` | PlayerState delegate 구독 → RenderData 구성 → Widget push | GEONU (신규) |
| `UBG_PlayerHealthWidget` | RenderData 캐시, `BP_OnRenderDataUpdated` hook 노출 | GEONU (신규) |
| `WBP_PlayerHealthHUD` | UMG 레이아웃, visual 갱신 | Content/GEONU (신규) |

---

## 설계 원칙

- **Widget**: passive view — gameplay 객체 참조 금지, RenderData만 소비
- **Connector**: 유일한 gameplay 접점 — delegate 구독, RenderData 구성, diff 비교 후 Widget push
- **UI 갱신**: pull이 아닌 push (`BP_OnRenderDataUpdated` 단일 진입점)

---

## 데이터 흐름

### Render Data (Connector → Widget)

Widget이 직접 소비하는 가공 데이터. 4개 영역으로 분리:

| 영역 | 주요 필드 |
|------|-----------|
| MainBars | `HealthPercent`, `BoostPercent`, `DBNOPercent`, `bIsDead`, `bIsDBNO` |
| Armor | `HelmetLevel`, `VestLevel`, `BackpackLevel` |
| StatusEffects | `bHasBoostRun`, `bHasRegeneration` |
| Breath | `BreathPercent`, `bShouldShowBreath` |

---

## 이벤트 흐름

```
[서버] ApplyDamage() → CurrentHP 변경 → OnRep_CurrentHP() → OnHealthChanged.Broadcast()
[클라] 복제 수신     →                   OnRep_CurrentHP() → OnHealthChanged.Broadcast()
  ↓
Connector 수신 → RenderData 구성 → diff 비교 → Widget.ApplyRenderData()
  → BP_OnRenderDataUpdated()
```

서버에서 `ApplyDamage()` 내부에서 `OnRep`을 직접 호출하므로 서버/클라 양쪽 동작.

---

## 외부 수정 사항 (WON — 합의 필요)

| 파일 | 변경 내용 |
|------|-----------|
| `BG_PlayerState.h` | `FOnBGHealthChanged`, `FOnBGDeathStateChanged` delegate 선언 + 멤버 추가 |
| `BG_PlayerState.cpp` | `OnRep_CurrentHP()`에서 `OnHealthChanged.Broadcast(CurrentHP)` |
|  | `OnRep_IsDead()`에서 `OnDeathStateChanged.Broadcast(bIsDead)` |

기존 로직 변경 없음. 비어있던 OnRep 함수에 broadcast 1줄 추가.

---

## Connector 초기화 흐름

```
BeginPlay()
├── IsLocalController() 확인
├── Widget 생성 → AddToViewport
├── PlayerState delegate 구독 (지연 바인딩 대응)
└── 초기 RenderData push
```

---

## 파일 목록

| 경로 | 역할 | 상태 |
|------|------|------|
| `GEONU/BG_PlayerHealthTypes.h` | RenderData 구조체 | 신규 |
| `GEONU/BG_PlayerHealthWidget.{h,cpp}` | Widget (C++) | 신규 |
| `GEONU/BG_HealthHUDConnector.{h,cpp}` | 이벤트 브릿지 | 신규 |
| `Content/GEONU/WBP_PlayerHealthHUD` | Widget Blueprint | 신규 |
| `WON/.../BG_PlayerState.{h,cpp}` | delegate 추가 | 수정 |

---

## 구현 순서

1. `ABG_PlayerState`에 delegate 추가 (WON 합의 후)
2. `BG_PlayerHealthTypes.h` — 구조체 정의
3. `UBG_PlayerHealthWidget` — RenderData 수신 + BP hook
4. `UBG_HealthHUDConnector` — 브릿지 (PlayerState → Widget)
5. `WBP_PlayerHealthHUD` — UMG 레이아웃
6. PlayerController에 Connector 부착

---

## 향후 확장

| 시스템 추가 시 | 변경 위치 |
|----------------|-----------|
| `UBGHealthComponent` | Connector — Component delegate로 전환 |
| `UBGBoostComponent` | Connector — Boost 바인딩 추가 |
| `UBGDBNOComponent` | Connector — DBNO 바인딩 추가 |
| 인벤토리 (장비) | Connector — 장비 변경 바인딩 추가 |
| 수중/숨참기 | Connector — Breath 바인딩 추가 |

모든 확장은 **Connector에 바인딩 추가 + RenderData 필드 채우기**로 귀결.
Widget은 이미 전체 필드를 처리하므로 변경 불필요.
