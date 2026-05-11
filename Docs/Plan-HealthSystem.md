# PUBG의 체력 시스템

## 분석

### HP

- 최대 100%
- 0 되면: 듀오/스쿼드 플레이 시 기절, 솔로 플레이 시 사망

### 부스트 게이지

- 최대 100%
- 30초당 10%씩 하락
- 높을수록 체력 회복량과 이동 속도, 모든 캐릭터 행동(정조준, 재장전, 앉았다 일어나기 등) 단계적 상승

  |                | 0~20% | 20~60% | 60~90% | 90~100% |
  | -------------- | ----- | ------ | ------ | ------- |
  | 8초당 HP 회복  | +1    | +2     | +3     | +4      |
  | 이동 속도 증가 |       |        | +2.5%  | +6.25%  |

### 회복 아이템

#### 치료제

- 일반적으로 75% 까지만 회복 가능

|             | 사용 시간 | 지속 시간 | 총 회복량 | 최대 회복 수치 |
| ----------- | --------- | --------- | --------- | -------------- |
| 붕대        | 4s        | 4.5s      | 10%       | 75%            |
| 구급 상자   | 6s        | 2s        | 75%       | 75%            |
| 의료용 키트 | 8s        | 즉시 발동 | 100%      | 100%           |

#### 부스터

|                   | 사용 시간 | 부스트 게이지 추가 |
| ----------------- | --------- | ------------------ |
| 에너지 드링크     | 4s        | +40%               |
| 진통제            | 6s        | +60%               |
| 아드레날린 주사기 | 6s        | +100%              |

---

## 확인된 사항

| #   | 질문        | 결정                                                                          |
| --- | ----------- | ----------------------------------------------------------------------------- |
| Q1  | DBNO 범위   | 원작 동일 구현. 별도 DBNO HP, 팀원 부활, 기어가기. 기본 HP와 최대한 분리 설계 |
| Q2  | 아이템 취소 | 피격/이동/무기 교체/수동 취소 모두 가능. 취소 시 아이템 미소모                |
| Q3  | 행동 속도   | 수치 미확정. 이동 속도 증가와 동일한 배율 적용 가정                           |
| Q4  | 회복 방식   | 원작 동일 — 지속 시간 동안 틱마다 점진적 회복                                 |
| Q5  | 75% 캡      | HP ≥ 75%이면 붕대/구급상자 사용 불가                                          |
| Q6  | 기존 코드   | 참고만 하고 새로 구현                                                         |
| Q7  | 인벤토리    | 별도 인벤토리 시스템과 연동 예정 (인터페이스로 분리)                          |

---

## 설계

### 네이밍 규칙

- 접두사 `BG` 사용 (언더바 생략). 예: `UBGHealthComponent`, `ABGCharacter`
- Blueprint에서 표시 이름은 동일하며, C++ 코드에서만 차이

### 설계 원칙

- **컴포넌트 기반**: 각 책임을 ActorComponent로 분리하여 독립적으로 테스트·교체 가능
- **델리게이트 결합**: 컴포넌트 간 직접 참조 대신 델리게이트(이벤트)로 연결
- **서버 권한**: 모든 HP/게이지 변경은 서버에서만 수행, 결과만 복제
- **데이터 드리븐**: 아이템 데이터는 DataTable로 관리, 수치는 에디터에서 조정 가능

### 아키텍처 개요

```
ABGCharacter
├── UBGHealthComponent       (신규 — HP 관리, 데미지/회복, 사망 판정 위임)
├── UBGBoostComponent        (신규 — 부스트 게이지, HP 자동 회복, 속도 버프)
├── UBGHealingComponent      (신규 — 회복 아이템 캐스팅 상태 머신)
├── UBGDBNOComponent         (신규 — 기절 상태, DBNO HP, 부활)
└── UBGDamageSystem          (기존 참고 — 데미지 적용 진입점)
```

**핵심 분리 전략 — HealthComponent는 DBNO를 모른다**:

```
HealthComponent                DBNOComponent
     │                              │
     ├─ HP가 0에 도달               │
     ├─ OnHealthDepleted 델리게이트 ─────> 수신
     │  (bOutShouldDie 참조 전달)    │     ├─ 솔로: bOutShouldDie = true (기본값)
     │                              │     └─ 스쿼드: bOutShouldDie = false → DBNO 진입
     ├─ bOutShouldDie 확인          │
     │  ├─ true  → Die()           │
     │  └─ false → HP=0 유지, 사망 안 함
     │                              │
```

이 구조 덕분에:
- **솔로 모드**: DBNOComponent를 붙이지 않거나 비활성화하면 HP 0 = 즉시 사망
- **스쿼드 모드**: DBNOComponent가 사망을 가로채고 DBNO 상태를 관리
- **HealthComponent 코드 변경 없이** DBNO 동작을 추가/제거 가능

### 1. `UBGHealthComponent` (ActorComponent)

HP의 보관과 변경을 담당하는 핵심 컴포넌트. DBNO, 부스트, 회복 아이템에 대해 알지 못한다.

**Owner**: `ABGCharacter`

**복제 프로퍼티**:

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthChanged, float, NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeath, AController*, InstigatorController);

// HP 0 도달 시: bOutShouldDie를 false로 설정하면 사망을 막을 수 있음
DECLARE_DELEGATE_OneParam(FOnHealthDepleted, bool& /* bOutShouldDie */);
```

```cpp
UPROPERTY(ReplicatedUsing = OnRep_CurrentHP, BlueprintReadOnly, Category = "Health")
float CurrentHP;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health")
float MaxHP = 100.f;

UPROPERTY(ReplicatedUsing = OnRep_bIsDead, BlueprintReadOnly, Category = "Health")
bool bIsDead = false;

// 델리게이트
UPROPERTY(BlueprintAssignable, Category = "Health")
FOnHealthChanged OnHealthChanged;     // HP 변경 시 (UI용)

UPROPERTY(BlueprintAssignable, Category = "Health")
FOnDeath OnDeath;                     // 최종 사망 시

FOnHealthDepleted OnHealthDepleted;   // HP 0 도달 시 (DBNO 판정용, C++ only)
```

**주요 함수**:

```cpp
// 서버 전용: 데미지 적용
UFUNCTION(BlueprintCallable, Category = "Health")
bool ApplyDamage(float DamageAmount, AController* InstigatorController);
// → CurrentHP 감소 → 0 이하 시 OnHealthDepleted 호출
// → bOutShouldDie가 true면 Die(), false면 HP=0 유지

// 서버 전용: 회복 적용
UFUNCTION(BlueprintCallable, Category = "Health")
bool ApplyHealing(float HealAmount, float MaxCap = 100.f);
// → Min(CurrentHP + HealAmount, MaxCap) 적용

// 사망 처리
void Die(AController* InstigatorController);
// → bIsDead = true → OnDeath 브로드캐스트

// DBNO에서 부활 시 호출
UFUNCTION(BlueprintCallable, Category = "Health")
void ReviveWithHealth(float NewHP);
// → CurrentHP = NewHP, bIsDead 유지 (DBNO 해제는 DBNOComponent가 관리)

float GetHealthPercent() const;
```

**ApplyDamage 흐름**:

```
ApplyDamage(Amount, Instigator)
├── Authority 검증 / bIsDead 검증
├── CurrentHP = FMath::Clamp(CurrentHP - Amount, 0, MaxHP)
├── OnRep_CurrentHP() → OnHealthChanged 브로드캐스트
├── if (CurrentHP <= 0)
│   ├── bool bShouldDie = true
│   ├── OnHealthDepleted.ExecuteIfBound(bShouldDie)   ← DBNOComponent가 여기서 가로챔
│   ├── if (bShouldDie) → Die(Instigator)
│   └── else → HP 0 유지, 사망하지 않음 (DBNO 상태)
└── return true
```

### 2. `UBGBoostComponent` (ActorComponent)

부스트 게이지를 관리하고, 게이지 수준에 따라 HP 자동 회복 및 속도 버프를 적용한다.

**Owner**: `ABGCharacter`

**부스트 단계 데이터**:

```cpp
USTRUCT(BlueprintType)
struct FBGBoostTier
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly) float MinGauge;          // 단계 시작 (0~100)
    UPROPERTY(EditDefaultsOnly) float MaxGauge;          // 단계 끝
    UPROPERTY(EditDefaultsOnly) float HPRecoveryPer8s;   // 8초당 HP 회복량
    UPROPERTY(EditDefaultsOnly) float SpeedMultiplier;   // 이동 속도 배율 (1.0 = 기본)
    UPROPERTY(EditDefaultsOnly) float ActionSpeedMultiplier; // 행동 속도 배율 (= SpeedMultiplier와 동일하게 설정)
};
```

**기본 4단계 설정**:

| 단계 | Min | Max | HP/8s | 속도 배율 |
| ---- | --- | --- | ----- | --------- |
| 1    | 0   | 20  | 1     | 1.0       |
| 2    | 20  | 60  | 2     | 1.0       |
| 3    | 60  | 90  | 3     | 1.025     |
| 4    | 90  | 100 | 4     | 1.0625    |

**복제 프로퍼티**:

```cpp
UPROPERTY(ReplicatedUsing = OnRep_BoostGauge, BlueprintReadOnly, Category = "Boost")
float BoostGauge = 0.f;

UPROPERTY(EditDefaultsOnly, Category = "Boost")
TArray<FBGBoostTier> BoostTiers;

UPROPERTY(EditDefaultsOnly, Category = "Boost")
float GaugeDecayRate = 0.333f; // 초당 감소량 (10% / 30초)

float BoostHealAccumulator = 0.f;  // 8초 주기 추적
float BaseWalkSpeed;                // BeginPlay에서 캐싱
```

**주요 함수**:

```cpp
// 부스터 아이템 사용 시 HealingComponent가 호출
UFUNCTION(BlueprintCallable, Category = "Boost")
void AddBoostGauge(float Amount);

// 현재 부스트 단계의 속도 배율 (외부에서 조회용)
UFUNCTION(BlueprintPure, Category = "Boost")
float GetCurrentSpeedMultiplier() const;

float GetCurrentActionSpeedMultiplier() const;
```

**서버 Tick 흐름**:

```
TickComponent(DeltaTime)  [서버 전용]
├── 게이지 감소: BoostGauge = FMath::Max(0, BoostGauge - GaugeDecayRate * DeltaTime)
├── 현재 Tier 결정
├── HP 자동 회복:
│   ├── BoostHealAccumulator += DeltaTime
│   └── if (BoostHealAccumulator >= 8.0)
│       ├── HealthComp->ApplyHealing(Tier.HPRecoveryPer8s)
│       └── BoostHealAccumulator -= 8.0
└── 이동 속도 갱신:
    └── CharacterMovement->MaxWalkSpeed = BaseWalkSpeed * Tier.SpeedMultiplier
```

### 3. `UBGHealingComponent` (ActorComponent)

회복 아이템 사용 캐스팅을 관리한다. 인벤토리 시스템과는 인터페이스로 분리한다.

**Owner**: `ABGCharacter`

**아이템 데이터 — DataTable 기반**:

아이템 종류를 열거형으로 고정하지 않고, DataTable의 RowName(`FName`)으로 식별한다.
새로운 회복 아이템을 추가할 때 C++ 재컴파일 없이 DataTable 행만 추가하면 된다.

```cpp
// 회복 아이템 DataTable Row 정의
USTRUCT(BlueprintType)
struct FBGHealItemRow : public FTableRowBase
{
    GENERATED_BODY()

    // 아이템 표시 이름 (UI용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;

    // 사용 시간 (초) — 캐스팅 타임
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float CastTime = 4.f;

    // 효과 지속 시간 (0 = 즉시 발동)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float Duration = 0.f;

    // 총 HP 회복량 (치료제용, 부스터는 0)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float TotalHealAmount = 0.f;

    // 최대 회복 수치 (75 = 붕대/구급상자, 100 = 의료용 키트)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxHealCap = 75.f;

    // 부스트 게이지 추가량 (부스터용, 치료제는 0)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float BoostAmount = 0.f;

    // true면 부스터, false면 치료제
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bIsBooster = false;
};
```

**DataTable 예시 (에디터에서 생성)**:

| RowName             | DisplayName       | CastTime | Duration | TotalHealAmount | MaxHealCap | BoostAmount | bIsBooster |
| ------------------- | ----------------- | -------- | -------- | --------------- | ---------- | ----------- | ---------- |
| `Bandage`           | 붕대              | 4.0      | 4.5      | 10              | 75         | 0           | false      |
| `FirstAidKit`       | 구급 상자         | 6.0      | 2.0      | 75              | 75         | 0           | false      |
| `MedKit`            | 의료용 키트       | 8.0      | 0        | 100             | 100        | 0           | false      |
| `EnergyDrink`       | 에너지 드링크     | 4.0      | 0        | 0               | 0          | 40          | true       |
| `Painkiller`        | 진통제            | 6.0      | 0        | 0               | 0          | 60          | true       |
| `AdrenalineSyringe` | 아드레날린 주사기 | 6.0      | 0        | 0               | 0          | 100         | true       |

**HealingComponent의 DataTable 참조**:

```cpp
// 에디터에서 DataTable 에셋 할당
UPROPERTY(EditDefaultsOnly, Category = "Healing")
TObjectPtr<UDataTable> HealItemDataTable;

// Row 조회 헬퍼
const FBGHealItemRow* FindHealItemRow(FName RowName) const;
```

**인벤토리 인터페이스 (추후 연동용)**:

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class UBGInventoryInterface : public UInterface { GENERATED_BODY() };

class IBGInventoryInterface
{
    GENERATED_BODY()
public:
    // 해당 아이템을 소지하고 있는지 확인 (RowName으로 식별)
    virtual bool HasHealItem(FName ItemRowName) const = 0;
    // 아이템 1개 소모
    virtual bool ConsumeHealItem(FName ItemRowName) = 0;
};
```

HealingComponent는 Owner가 `IBGInventoryInterface`를 구현하면 소지/소모를 체크하고, 구현하지 않으면 무제한 사용을 허용한다.

**상태 머신**:

```
                    ┌───────────────────────────────────┐
                    v                                   │
[Idle] ──UseItem()──> [Casting] ──CastTime 경과──> [Applying] ──Duration 경과──> [Idle]
  ^                      │             │
  │                      │ 취소 조건    │ Duration=0 (즉시 효과)
  │                      v             v
  └──────────────── [Cancelled]     [Idle]
```

**캐스팅 상태 (복제)**:

```cpp
UENUM(BlueprintType)
enum class EBGHealingState : uint8
{
    Idle,       // 대기
    Casting,    // 캐스팅 중 (사용 시간)
    Applying    // 효과 적용 중 (지속 회복)
};

UPROPERTY(ReplicatedUsing = OnRep_HealingState, BlueprintReadOnly, Category = "Healing")
EBGHealingState HealingState = EBGHealingState::Idle;

UPROPERTY(Replicated, BlueprintReadOnly, Category = "Healing")
float CastProgress = 0.f;   // 0~1, UI 프로그래스 바용

// 현재 사용 중인 아이템의 RowName (UI에서 아이콘/이름 표시용)
UPROPERTY(Replicated, BlueprintReadOnly, Category = "Healing")
FName CurrentItemRowName;
```

**취소 조건** (서버 Tick에서 판정):

| 조건      | 판정 방법                                                           |
| --------- | ------------------------------------------------------------------- |
| 피격      | `OnHealthChanged` 델리게이트 바인딩 → 캐스팅 시작 HP보다 감소 시    |
| 이동      | `GetOwner()->GetVelocity().SizeSquared() > Threshold`               |
| 무기 교체 | 무기 시스템의 `OnWeaponSwitchStarted` 델리게이트 바인딩 (추후 연동) |
| 수동 취소 | `Server_CancelHealing()` RPC                                        |
| DBNO 진입 | `UBGDBNOComponent::OnDBNOEntered` 델리게이트 바인딩                 |

**주요 함수**:

```cpp
// 클라이언트 → 서버: RowName으로 아이템 식별
UFUNCTION(Server, Reliable)
void Server_RequestUseItem(FName ItemRowName);

UFUNCTION(Server, Reliable)
void Server_CancelHealing();

bool CanUseItem(const FBGHealItemRow* ItemRow) const;
// → bIsDead 체크, DBNO 체크, 이미 캐스팅 중 체크
// → 치료제: HP >= MaxHealCap이면 사용 불가
// → DataTable에 해당 Row가 존재하는지 검증

void OnCastComplete();       // 캐스팅 완료 → 효과 적용 시작
void ProcessHealOverTime(float DeltaTime);  // 지속 회복 틱
void FinishHealing();        // 효과 적용 완료 → Idle 복귀
void CancelHealing();        // 취소 처리
```

### 4. `UBGDBNOComponent` (ActorComponent)

기절(DBNO) 상태를 관리한다. HealthComponent와 분리되어 있어, 이 컴포넌트가 없으면 HP 0 = 즉시 사망.

**Owner**: `ABGCharacter`

**원작 기준 DBNO 규칙**:

| 항목           | 수치                                                                        |
| -------------- | --------------------------------------------------------------------------- |
| DBNO HP        | 항상 100에서 시작 (회차 무관)                                               |
| 기절 생존 시간 | 1회: 90s, 2회: 30s, 3회: 30s, 4회: 10s, 5회: 7s, 6회: 5s, 7회: 4s, 8회+: 3s |
| 초당 감소 속도 | 100 / 생존시간 (1회: ~1.11/s, 2회: ~3.33/s, ...)                            |
| 정지 시 감소율 | 이동 시보다 느림 (비율 미확정)                                              |
| 부활 소요 시간 | 10초                                                                        |
| 부활 후 HP     | 10%                                                                         |
| 기어가기 속도  | 기본 이동 속도의 약 15%                                                     |

**복제 프로퍼티**:

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDBNOEntered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDBNORevived);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDBNODeath);

UPROPERTY(ReplicatedUsing = OnRep_bIsDBNO, BlueprintReadOnly, Category = "DBNO")
bool bIsDBNO = false;

UPROPERTY(ReplicatedUsing = OnRep_DBNOHP, BlueprintReadOnly, Category = "DBNO")
float DBNOHP = 0.f;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DBNO")
float MaxDBNOHP = 100.f;

// 회차별 생존 시간 배열 (인덱스 0 = 1회차). 배열 초과 시 마지막 값 사용
UPROPERTY(EditDefaultsOnly, Category = "DBNO")
TArray<float> KnockdownDurations; // {90, 30, 30, 10, 7, 5, 4, 3}

// 정지 시 감소 속도 배율 (1.0 미만이면 정지 시 더 느림)
UPROPERTY(EditDefaultsOnly, Category = "DBNO")
float IdleDecayMultiplier = 0.5f;

UPROPERTY(EditDefaultsOnly, Category = "DBNO")
float ReviveTime = 10.f;

UPROPERTY(EditDefaultsOnly, Category = "DBNO")
float ReviveHP = 10.f; // 부활 후 HP

UPROPERTY(EditDefaultsOnly, Category = "DBNO")
float CrawlSpeedMultiplier = 0.15f;

// 부활 후 무적 시간 (초)
UPROPERTY(EditDefaultsOnly, Category = "DBNO")
float PostReviveInvincibilityTime = 0.5f;

// DBNO 활성화 여부 (false면 HP 0 = 즉사. 추후 GameState 모드 정보로 교체)
UPROPERTY(EditDefaultsOnly, Category = "DBNO")
bool bDBNOEnabled = true;

UPROPERTY(BlueprintReadOnly, Category = "DBNO")
int32 KnockdownCount = 0; // 이번 매치 기절 횟수

// 부활 진행 상태
UPROPERTY(Replicated, BlueprintReadOnly, Category = "DBNO")
bool bIsBeingRevived = false;

UPROPERTY(Replicated, BlueprintReadOnly, Category = "DBNO")
float ReviveProgress = 0.f; // 0~1

// 부활 후 무적 타이머
float PostReviveInvincibilityRemaining = 0.f;

// 현재 이 캐릭터를 부활시키고 있는 팀원 (1명 제한)
TWeakObjectPtr<AActor> Reviver;

// 델리게이트
UPROPERTY(BlueprintAssignable) FOnDBNOEntered OnDBNOEntered;
UPROPERTY(BlueprintAssignable) FOnDBNORevived OnDBNORevived;
UPROPERTY(BlueprintAssignable) FOnDBNODeath OnDBNODeath;
```

**BeginPlay 바인딩**:

```cpp
void BeginPlay()
{
    // HealthComponent의 OnHealthDepleted에 바인딩
    HealthComp->OnHealthDepleted.BindUObject(this, &ThisClass::HandleHealthDepleted);
}

void HandleHealthDepleted(bool& bOutShouldDie)
{
    if (bIsDBNO) return; // 이미 DBNO 중이면 무시 (DBNO에서 죽는 건 DBNO HP로 판정)

    // 부활 후 무적 시간 중이면 데미지 무시
    if (PostReviveInvincibilityRemaining > 0.f) return;

    // DBNO 비활성화 (솔로 모드 등) → 즉사
    // 추후 GameState에서 모드 정보 조회로 교체 예정
    if (!bDBNOEnabled) return; // bOutShouldDie = true 유지

    // 스쿼드: DBNO 진입
    bOutShouldDie = false;
    EnterDBNO();
}
```

**DBNO 진입**:

```
EnterDBNO()
├── KnockdownCount++
├── DBNOHP = MaxDBNOHP (항상 100)
├── 현재 회차의 감소 속도 계산:
│   ├── Duration = KnockdownDurations[Min(KnockdownCount-1, 배열 끝)]
│   └── CurrentDecayRate = MaxDBNOHP / Duration  (예: 1회차 = 100/90 ≈ 1.11/s)
├── bIsDBNO = true
├── 이동 모드 변경: 기어가기 (속도 제한, 자세 변경)
├── 입력 제한: 공격/아이템 사용 불가
├── OnDBNOEntered 브로드캐스트
└── HealingComponent에 캐스팅 취소 알림
```

**DBNO Tick (서버)**:

```
TickComponent(DeltaTime)  [서버 전용]
├── [부활 후 무적 타이머]
│   └── if (PostReviveInvincibilityRemaining > 0)
│       └── PostReviveInvincibilityRemaining -= DeltaTime
│
├── [bIsDBNO == true 일 때만]
├── 이동 상태 판정:
│   └── bIsMoving = GetOwner()->GetVelocity().SizeSquared() > Threshold
├── 감소율 결정:
│   ├── 이동 중: DecayRate = CurrentDecayRate
│   └── 정지 중: DecayRate = CurrentDecayRate * IdleDecayMultiplier
├── DBNO HP 자연 감소: DBNOHP -= DecayRate * DeltaTime
├── if (DBNOHP <= 0)
│   └── DBNODeath() → HealthComp->Die()
├── 부활 처리:
│   └── if (bIsBeingRevived)
│       ├── 부활자 유효성 검증 (Reviver 존재, 범위 내, 살아있음)
│       │   └── 실패 시 → CancelRevive()
│       ├── ReviveProgress += DeltaTime / ReviveTime
│       └── if (ReviveProgress >= 1.0) → CompleteRevive()
```

**DBNO 중 피격**:

```cpp
// HealthComponent는 DBNO를 모르므로, DBNOComponent가 데미지를 가로챌 방법이 필요

// 방법: HealthComponent에 OnDamageReceived 델리게이트 추가
DECLARE_DELEGATE_ThreeParams(FOnDamageReceived, float /*Amount*/, AController* /*Instigator*/, bool& /*bOutHandled*/);

// DBNOComponent가 바인딩:
void HandleDamageReceived(float Amount, AController* Instigator, bool& bOutHandled)
{
    // 부활 후 무적 중이면 데미지 무시
    if (PostReviveInvincibilityRemaining > 0.f)
    {
        bOutHandled = true;
        return;
    }

    if (!bIsDBNO) return;
    bOutHandled = true;  // HealthComponent의 기본 데미지 처리를 막음
    DBNOHP -= Amount;
    if (DBNOHP <= 0) DBNODeath();
}
```

**부활**:

```
[부활하는 팀원]                    [기절한 플레이어]
     │                                  │
     ├─ 상호작용 키 유지                 │
     │──Server_StartRevive(Target)────>│
     │                                  ├─ 검증: Target이 DBNO 상태인가?
     │                                  ├─ 검증: 이미 다른 팀원이 부활 중인가? (1명 제한)
     │                                  ├─ 검증: 부활자가 이미 다른 대상을 부활 중인가?
     │                                  ├─ Reviver = 부활하는 팀원
     │                                  ├─ bIsBeingRevived = true
     │                                  ├─ ReviveProgress = 0 (처음부터)
     │                                  │
     │  (10초간 틱)                     ├─ ReviveProgress 증가
     │  부활자: 이동/공격 불가           │
     │                                  │
     │  부활 완료                       ├─ CompleteRevive()
     │                                  │  ├─ bIsDBNO = false
     │                                  │  ├─ HealthComp->ReviveWithHealth(10)
     │                                  │  ├─ PostReviveInvincibilityRemaining = 0.5
     │                                  │  ├─ 이동 모드 복원
     │                                  │  ├─ 부활자 행동 제한 해제
     │                                  │  └─ OnDBNORevived 브로드캐스트
```

**부활 취소 조건** (취소 시 ReviveProgress 리셋, 처음부터 재시작):
- 부활시키는 팀원이 피격 시
- 부활시키는 팀원이 범위를 벗어날 시
- 부활시키는 팀원이 상호작용 키를 뗄 시

**부활자 행동 제한**:
- 부활 중 이동 불가
- 부활 중 공격/아이템 사용 불가
- 상호작용 키를 떼거나 다른 입력 시 부활 취소

### 5. 컴포넌트 간 의존 관계

```
UBGHealthComponent  ◄─────────── UBGDBNOComponent
  (HP 관리)           델리게이트     (DBNO 상태)
       │                               │
       │                               │ 델리게이트
       v                               v
  OnHealthDepleted ──────────> HandleHealthDepleted()
  OnDamageReceived ──────────> HandleDamageReceived()
       │
       │ 참조
       v
UBGBoostComponent  ──참조──> UBGHealthComponent::ApplyHealing()
  (부스트 게이지)
       │
       │ 참조 없음 (AddBoostGauge를 외부에서 호출)
       v
UBGHealingComponent ──참조──> UBGHealthComponent (CanUseItem, ApplyHealing)
  (아이템 캐스팅)    ──참조──> UBGBoostComponent (AddBoostGauge)
                     ──참조──> UDataTable (FBGHealItemRow 조회)
                     ──구현──> IBGInventoryInterface (HasItem, ConsumeItem)
```

**의존 방향 요약**: 모든 컴포넌트는 HealthComponent를 참조하지만, HealthComponent는 다른 컴포넌트를 알지 못한다. DBNOComponent는 델리게이트만으로 HealthComponent에 연결된다.

### 6. 네트워크 복제 요약

| 컴포넌트            | 복제 프로퍼티                                            | OnRep 용도                          |
| ------------------- | -------------------------------------------------------- | ----------------------------------- |
| UBGHealthComponent  | `CurrentHP`, `bIsDead`                                   | HP 바 UI, 사망 연출                 |
| UBGBoostComponent   | `BoostGauge`                                             | 부스트 게이지 UI                    |
| UBGHealingComponent | `HealingState`, `CastProgress`, `CurrentItemRowName`     | 캐스팅 프로그래스 바, 아이템 아이콘 |
| UBGDBNOComponent    | `bIsDBNO`, `DBNOHP`, `bIsBeingRevived`, `ReviveProgress` | DBNO UI, 부활 프로그래스            |

### 7. 네트워크 흐름 (아이템 사용)

```
[Client]                                  [Server]
   │                                         │
   ├─ UseItem 입력                           │
   │──Server_RequestUseItem(RowName)───────>│
   │                                         ├─ DataTable에서 FBGHealItemRow 조회
   │                                         ├─ CanUseItem() 검증
   │                                         │  ├─ Row 존재 여부
   │                                         │  ├─ bIsDead? bIsDBNO?
   │                                         │  ├─ 치료제: HP >= MaxHealCap?
   │                                         │  └─ 인벤토리: HasHealItem(RowName)?
   │                                         ├─ HealingState = Casting
   │                                         │
   │<──OnRep_HealingState (UI 갱신)─────────│
   │                                         │
   │                                         ├─ Tick: CastProgress 갱신, 취소 판정
   │                                         │
   │                                         ├─ CastTime 경과 → OnCastComplete()
   │                                         │  ├─ 치료제: HealingState = Applying
   │                                         │  │          ProcessHealOverTime 시작
   │                                         │  └─ 부스터: BoostComp->AddBoostGauge()
   │                                         │            HealingState = Idle
   │                                         │
   │<──OnRep_CurrentHP ─────────────────────│
   │<──OnRep_BoostGauge ────────────────────│
```

### 8. 클래스 다이어그램

```
┌────────────────────────────────────────────────────────────────────┐
│                          ABGCharacter                              │
│                                                                    │
│ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌────────────┐ │
│ │ UBGHealth    │ │ UBGBoost     │ │ UBGHealing   │ │ UBGDBNO    │ │
│ │ Component    │ │ Component    │ │ Component    │ │ Component  │ │
│ │              │ │              │ │              │ │            │ │
│ │ CurrentHP    │ │ BoostGauge   │ │ HealingState │ │ bIsDBNO    │ │
│ │ bIsDead      │ │ BoostTiers[] │ │ CastProgress │ │ DBNOHP     │ │
│ │              │ │              │ │ RowName      │ │            │ │
│ │ ApplyDamage()│ │ AddBoost()   │ │ Server_Use() │ │ Revive()   │ │
│ │ ApplyHeal()  │ │ TickHP()     │ │ Cancel()     │ │ Crawl()    │ │
│ │ Die()        │ │ TickSpeed()  │ │ CanUse()     │ │ Enter()    │ │
│ └──────────────┘ └──────────────┘ └──────────────┘ └────────────┘ │
│                                                                    │
│  ┌──────────────────────────────────────────┐                      │
│  │ UDataTable (FBGHealItemRow)              │                      │
│  │ 에디터에서 아이템 행 추가/수정           │                      │
│  └──────────────────────────────────────────┘                      │
│                                                                    │
│  ┌──────────────────────────────────────────┐                      │
│  │ IBGInventoryInterface (추후 연동)        │                      │
│  │ HasHealItem(FName) / ConsumeHealItem()   │                      │
│  └──────────────────────────────────────────┘                      │
└────────────────────────────────────────────────────────────────────┘
```

### 9. 파일 구조

```
Source/PUBG_HotMode/[USERNAME]/
├── Public/
│   └── Health/
│       ├── BGHealthComponent.h       (신규 — HP 관리)
│       ├── BGBoostComponent.h        (신규 — 부스트)
│       ├── BGHealingComponent.h      (신규 — 회복 아이템 캐스팅)
│       ├── BGDBNOComponent.h         (신규 — DBNO)
│       ├── BGHealItemRow.h           (신규 — FBGHealItemRow DataTable Row 구조체)
│       └── BGInventoryInterface.h    (신규 — 인벤토리 인터페이스)
└── Private/
    └── Health/
        ├── BGHealthComponent.cpp
        ├── BGBoostComponent.cpp
        ├── BGHealingComponent.cpp
        └── BGDBNOComponent.cpp

Content/[USERNAME]/
└── Data/
    └── DT_HealItems.uasset          (DataTable — FBGHealItemRow)
```

### 10. 구현 순서

| 단계 | 작업                                                      | 의존성          | 비고                             |
| ---- | --------------------------------------------------------- | --------------- | -------------------------------- |
| 1    | `BGHealItemRow.h` — `FBGHealItemRow` (DataTable Row) 정의 | 없음            |                                  |
| 2    | `BGInventoryInterface.h` — 인터페이스 정의                | 없음            | FName 기반                       |
| 3    | `UBGHealthComponent` 구현                                 | 없음            | 데미지/회복/사망 위임            |
| 4    | `UBGBoostComponent` 구현                                  | 단계 3          | 게이지 감소, HP 틱, 속도         |
| 5    | `UBGHealingComponent` 구현                                | 단계 1, 2, 3, 4 | 캐스팅 상태 머신, DataTable 조회 |
| 6    | `UBGDBNOComponent` 구현                                   | 단계 3          | DBNO 상태, 부활                  |
| 7    | `ABGCharacter` 수정 — 컴포넌트 등록                       | 단계 3~6        |                                  |
| 8    | `DT_HealItems` DataTable 에셋 생성                        | 단계 1          | 에디터에서 6개 Row 입력          |
| 9    | 입력 바인딩 (아이템 사용/취소 키)                         | 단계 7          |                                  |
| 10   | 부활 상호작용 입력                                        | 단계 7          |                                  |
| 11   | 테스트                                                    | 전체            |                                  |
