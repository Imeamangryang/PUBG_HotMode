# PUBG의 인벤토리 시스템

## 분석

### 장비 슬롯 (왼쪽)

- 헬멧: 머리 방어구 (Lv 1~3)
- 흉갑: 가슴 방어구 (Lv 1~3)
- 가방: 인벤토리 용량 확장 (Lv 0~3)
- 빤쓰: 영구 아이템, 인벤토리 용량 확장 → **생략 예정**

### 코스메틱 슬롯 (오른쪽)

- 전부 룩템 → **생략 예정**

### 무기 슬롯

| 슬롯 | 종류                       |
| ---- | -------------------------- |
| 1    | 일반 총기                  |
| 2    | 일반 총기                  |
| 3    | 권총                       |
| 4    | 근접 무기                  |
| 5    | 투척물 (수류탄, 연막탄 등) |

### 일반 인벤토리

- 최대 12칸 정도 보이는 목록, 스크롤 가능
- 가방 레벨에 따라 슬롯 수 증가
- 더 왼쪽에 **주변 아이템 목록** (바닥 루팅)

### 용량 (슬롯 기반)

| 가방 레벨   | 슬롯 수  |
| ----------- | -------- |
| 없음 (Lv 0) | 20       |
| Lv 1        | 30 (+10) |
| Lv 2        | 40 (+20) |
| Lv 3        | 50 (+30) |

### 원작 기준 방어구 (참고)

| 장비           | Lv 1 | Lv 2 | Lv 3 |
| -------------- | ---- | ---- | ---- |
| 헬멧 피해 감소 | 30%  | 40%  | 55%  |
| 흉갑 피해 감소 | 30%  | 40%  | 55%  |
| 내구도         | 80   | 150  | 230  |

---

## 확인된 사항

| #   | 질문             | 결정                                                                       |
| --- | ---------------- | -------------------------------------------------------------------------- |
| Q1  | 무기 시스템 범위 | 무기 슬롯 구조 포함. 총기 구현은 나중에 할 예정                            |
| Q2  | 용량 시스템      | 슬롯 수 기반. 가방 레벨에 따라 슬롯 수 증가                                |
| Q3  | 스택 규칙        | 원작처럼 아이템별 스택 상한이 다름. DataTable에 정의                       |
| Q4  | 방어구 세부      | 원작처럼 내구도 감소 + 파괴. 피해 적용 경로는 미정                         |
| Q5  | 바닥 아이템 스폰 | 원작처럼 고정 스폰 포인트 + 랜덤 아이템 테이블                             |
| Q6  | 줍기/버리기      | F키 + UI 클릭 줍기. 장비는 줍기 시 자동 장착. 버리기는 인벤토리에서 드래그 |
| Q7  | UI 구현          | UMG Widget + C++ UserWidget (로직). Tab 전체 인벤토리 + 퀵슬롯 HUD         |
| Q8  | 담당 경계        | 모든 코드는 `GEONU/`에 작성                                                |

---

## 설계

### 아키텍처 개요

```
ABGCharacter
├── UBGInventoryComponent      (신규 — 슬롯 기반 아이템 보관, 스택 관리)
├── UBGEquipmentComponent      (신규 — 장비 슬롯: 헬멧, 흉갑, 가방 + 무기 슬롯 5개)
├── UBGInteractionComponent    (신규 — 주변 아이템 감지, 줍기, 장비 자동 장착)
├── UBGHealthComponent         (기존 설계)
├── UBGBoostComponent          (기존 설계)
├── UBGHealingComponent        (기존 설계 — IBGInventoryInterface 사용)
└── UBGDBNOComponent           (기존 설계)

ABGPickupItem (Actor)           (신규 — 월드 배치 아이템 액터)
ABGItemSpawnPoint (Actor)       (신규 — 고정 스폰 포인트, 랜덤 아이템 테이블)

UI (C++ UserWidget 기반)
├── UBGInventoryWidget          (신규 — Tab 전체 인벤토리 화면)
├── UBGInventorySlotWidget      (신규 — 개별 슬롯 위젯)
├── UBGNearbyItemWidget         (신규 — 주변 아이템 목록)
└── UBGQuickSlotWidget          (신규 — 퀵슬롯 HUD)
```

### 1. 아이템 데이터 — DataTable 기반

모든 아이템 정보를 하나의 마스터 DataTable로 관리합니다.

```cpp
// 아이템 카테고리
UENUM(BlueprintType)
enum class EBGItemCategory : uint8
{
    Consumable,    // 소모품 (붕대, 구급상자, 에너지 드링크 등)
    Equipment,     // 장비 (헬멧, 흉갑, 가방)
    Weapon,        // 무기 (추후)
    Ammo,          // 탄약 (추후)
    Attachment,    // 부착물 (추후)
    Throwable      // 투척물 (추후)
};

// 장비 슬롯 타입 (방어구/가방)
UENUM(BlueprintType)
enum class EBGEquipSlot : uint8
{
    None,
    Helmet,
    Vest,
    Backpack
};

// 무기 슬롯 타입
UENUM(BlueprintType)
enum class EBGWeaponSlot : uint8
{
    None,
    Primary1,    // 슬롯 1: 일반 총기
    Primary2,    // 슬롯 2: 일반 총기
    Pistol,      // 슬롯 3: 권총
    Melee,       // 슬롯 4: 근접 무기
    Throwable    // 슬롯 5: 투척물
};
```

```cpp
// 마스터 아이템 DataTable Row
USTRUCT(BlueprintType)
struct FBGItemRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EBGItemCategory Category = EBGItemCategory::Consumable;

    // 한 슬롯에 최대 쌓기 수량 (아이템마다 다름)
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MaxStackCount = 1;

    // --- 장비 전용 ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Category == EBGItemCategory::Equipment"))
    EBGEquipSlot EquipSlot = EBGEquipSlot::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 EquipLevel = 0;  // 1~3

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float DamageReduction = 0.f;  // 0~1 (0.3 = 30%)

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float MaxDurability = 0.f;

    // 가방: 슬롯 수 증가량
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ExtraSlots = 0;

    // --- 무기 전용 (추후) ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Category == EBGItemCategory::Weapon"))
    EBGWeaponSlot WeaponSlot = EBGWeaponSlot::None;

    // --- 공통 ---
    // 월드 배치용 메시
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UStaticMesh> PickupMesh;

    // UI용 아이콘
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UTexture2D> Icon;
};
```

**마스터 아이템 DataTable 예시 (DT_Items)**:

| RowName             | DisplayName       | Category   | MaxStack | EquipSlot | Level | DmgReduction | Durability | ExtraSlots |
| ------------------- | ----------------- | ---------- | -------- | --------- | ----- | ------------ | ---------- | ---------- |
| `Bandage`           | 붕대              | Consumable | 5        | —         | —     | —            | —          | —          |
| `FirstAidKit`       | 구급 상자         | Consumable | 1        | —         | —     | —            | —          | —          |
| `MedKit`            | 의료용 키트       | Consumable | 1        | —         | —     | —            | —          | —          |
| `EnergyDrink`       | 에너지 드링크     | Consumable | 1        | —         | —     | —            | —          | —          |
| `Painkiller`        | 진통제            | Consumable | 1        | —         | —     | —            | —          | —          |
| `AdrenalineSyringe` | 아드레날린 주사기 | Consumable | 1        | —         | —     | —            | —          | —          |
| `Helmet_Lv1`        | 군용 헬멧         | Equipment  | 1        | Helmet    | 1     | 0.30         | 80         | —          |
| `Helmet_Lv2`        | 전투 헬멧         | Equipment  | 1        | Helmet    | 2     | 0.40         | 150        | —          |
| `Helmet_Lv3`        | 특수 헬멧         | Equipment  | 1        | Helmet    | 3     | 0.55         | 230        | —          |
| `Vest_Lv1`          | 경찰 조끼         | Equipment  | 1        | Vest      | 1     | 0.30         | 200        | —          |
| `Vest_Lv2`          | 군용 조끼         | Equipment  | 1        | Vest      | 2     | 0.40         | 220        | —          |
| `Vest_Lv3`          | 특수 조끼         | Equipment  | 1        | Vest      | 3     | 0.55         | 250        | —          |
| `Backpack_Lv1`      | 소형 배낭         | Equipment  | 1        | Backpack  | 1     | —            | —          | 10         |
| `Backpack_Lv2`      | 중형 배낭         | Equipment  | 1        | Backpack  | 2     | —            | —          | 20         |
| `Backpack_Lv3`      | 대형 배낭         | Equipment  | 1        | Backpack  | 3     | —            | —          | 30         |

> 인벤토리 기본 슬롯: 20칸, 가방 Lv1: +10 = 30칸, Lv2: +20 = 40칸, Lv3: +30 = 50칸
> 회복 아이템의 효과(CastTime, Duration 등)는 기존 `DT_HealItems`(Health 시스템)에서 관리합니다.

### 2. `FBGInventorySlot` — 인벤토리 슬롯 구조체

```cpp
USTRUCT(BlueprintType)
struct FBGInventorySlot
{
    GENERATED_BODY()

    // 아이템 식별자 (DT_Items의 RowName). NAME_None이면 빈 슬롯
    UPROPERTY(BlueprintReadOnly)
    FName ItemRowName = NAME_None;

    // 현재 스택 수량
    UPROPERTY(BlueprintReadOnly)
    int32 Count = 0;

    bool IsEmpty() const { return ItemRowName == NAME_None || Count <= 0; }
};
```

### 3. `FBGEquipSlotState` — 장비 슬롯 상태

```cpp
USTRUCT(BlueprintType)
struct FBGEquipSlotState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName ItemRowName = NAME_None;

    // 현재 내구도 (장착 시 MaxDurability로 초기화)
    UPROPERTY(BlueprintReadOnly)
    float CurrentDurability = 0.f;

    bool IsEmpty() const { return ItemRowName == NAME_None; }
};
```

### 4. `UBGInventoryComponent` (ActorComponent)

슬롯 기반 아이템 보관을 담당합니다. `IBGInventoryInterface`를 구현합니다.

**Owner**: `ABGCharacter`

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UBGInventoryComponent : public UActorComponent, public IBGInventoryInterface
{
    GENERATED_BODY()

public:
    // --- 복제 프로퍼티 ---

    // 인벤토리 슬롯 배열 (서버 권한, 클라이언트에 복제)
    UPROPERTY(ReplicatedUsing = OnRep_Slots, BlueprintReadOnly, Category = "Inventory")
    TArray<FBGInventorySlot> Slots;

    // 현재 최대 슬롯 수 (기본 + 가방 보너스). 서버에서 갱신
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Inventory")
    int32 MaxSlotCount = 20;

    // 기본 슬롯 수 (가방 없을 때)
    UPROPERTY(EditDefaultsOnly, Category = "Inventory")
    int32 BaseSlotCount = 20;

    // DataTable 참조
    UPROPERTY(EditDefaultsOnly, Category = "Inventory")
    TObjectPtr<UDataTable> ItemDataTable;

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryChanged OnInventoryChanged;

    // --- IBGInventoryInterface 구현 ---
    virtual bool HasHealItem(FName ItemRowName) const override;
    virtual bool ConsumeHealItem(FName ItemRowName) override;

    // --- 핵심 함수 (서버 전용) ---

    // 아이템 추가. 슬롯 부족 시 false 반환. OutRemaining: 넣지 못한 수량
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(FName ItemRowName, int32 Amount = 1, int32& OutRemaining = 0);

    // 아이템 제거
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItem(FName ItemRowName, int32 Amount = 1);

    // 아이템 버리기 (인벤토리에서 제거 → 월드에 PickupItem 스폰)
    UFUNCTION(Server, Reliable)
    void Server_DropItem(int32 SlotIndex, int32 Amount = 1);

    // 사용된 슬롯 수
    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetUsedSlotCount() const;

    // 빈 슬롯 수
    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetFreeSlotCount() const;

    // 특정 아이템 보유 수량 조회 (모든 슬롯 합산)
    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetItemCount(FName ItemRowName) const;

    // 슬롯 수 갱신 (EquipmentComponent에서 가방 변경 시 호출)
    void UpdateMaxSlots(int32 NewExtraSlots);

    // DataTable Row 조회 헬퍼
    const FBGItemRow* FindItemRow(FName RowName) const;

protected:
    UFUNCTION()
    void OnRep_Slots();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
```

**AddItem 흐름**:

```
AddItem(RowName, Amount, OutRemaining)
├── Authority 검증
├── DataTable에서 FBGItemRow 조회
├── 1단계: 기존 슬롯에 같은 아이템이 있고 스택 여유가 있으면 채움
│   └── 각 슬롯: Min(남은 Amount, MaxStackCount - Count) 만큼 추가
├── 2단계: 남은 Amount가 있으면 빈 슬롯에 새로 생성
│   ├── 빈 슬롯 = Slots 중 IsEmpty() || Slots.Num() < MaxSlotCount
│   └── 각 슬롯: Min(남은 Amount, MaxStackCount)
├── OutRemaining = 넣지 못한 수량
├── OnRep_Slots() → OnInventoryChanged 브로드캐스트
└── return (OutRemaining == 0)
```

**Server_DropItem 흐름**:

```
Server_DropItem(SlotIndex, Amount)
├── Authority 검증
├── SlotIndex 유효성, Slots[SlotIndex].IsEmpty() 체크
├── 버릴 수량 = Min(Amount, Slots[SlotIndex].Count)
├── Slots[SlotIndex].Count -= 버릴 수량
│   └── Count == 0이면 슬롯 비움 (ItemRowName = NAME_None)
├── 캐릭터 전방에 ABGPickupItem 스폰 (RowName, 버릴 수량)
├── OnRep_Slots() → OnInventoryChanged
└── 인벤토리가 MaxSlotCount 초과 상태였으면 초과 슬롯 정리
```

### 5. `UBGEquipmentComponent` (ActorComponent)

장비 슬롯(헬멧/흉갑/가방) + 무기 슬롯(5개)을 관리하고, 방어구 효과를 적용합니다.

**Owner**: `ABGCharacter`

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEquipmentChanged, EBGEquipSlot, Slot, FName, NewItemRowName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponSlotChanged, EBGWeaponSlot, Slot, FName, NewItemRowName);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UBGEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // --- 방어구/가방 슬롯 (복제) ---
    UPROPERTY(ReplicatedUsing = OnRep_HelmetSlot, BlueprintReadOnly, Category = "Equipment")
    FBGEquipSlotState HelmetSlot;

    UPROPERTY(ReplicatedUsing = OnRep_VestSlot, BlueprintReadOnly, Category = "Equipment")
    FBGEquipSlotState VestSlot;

    UPROPERTY(ReplicatedUsing = OnRep_BackpackSlot, BlueprintReadOnly, Category = "Equipment")
    FBGEquipSlotState BackpackSlot;

    // --- 무기 슬롯 (복제, 추후 확장) ---
    UPROPERTY(ReplicatedUsing = OnRep_WeaponSlots, BlueprintReadOnly, Category = "Equipment")
    TMap<EBGWeaponSlot, FName> WeaponSlots;
    // 초기화: {Primary1: NAME_None, Primary2: NAME_None, Pistol: NAME_None, Melee: NAME_None, Throwable: NAME_None}

    UPROPERTY(EditDefaultsOnly, Category = "Equipment")
    TObjectPtr<UDataTable> ItemDataTable;

    UPROPERTY(BlueprintAssignable, Category = "Equipment")
    FOnEquipmentChanged OnEquipmentChanged;

    UPROPERTY(BlueprintAssignable, Category = "Equipment")
    FOnWeaponSlotChanged OnWeaponSlotChanged;

    // --- 핵심 함수 (서버 전용) ---

    // 장비 장착. 인벤토리 연동: 기존 장비 해제 → 인벤토리, 새 장비 인벤토리에서 제거
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool EquipItem(FName ItemRowName);

    // 장비 해제 → 인벤토리로 이동
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool UnequipItem(EBGEquipSlot Slot);

    // 장비 자동 장착 시도 (줍기 시 호출). 빈 슬롯이면 장착, 상위 레벨이면 교체
    // return true이면 인벤토리에 넣지 않음 (바로 장착됨)
    bool TryAutoEquip(FName ItemRowName);

    // 피격 시: 방어구 효과 적용 (내구도 차감, 파괴)
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    float ApplyArmorReduction(float InDamage, EBGEquipSlot HitSlot);

    UFUNCTION(BlueprintPure, Category = "Equipment")
    float GetDamageReduction(EBGEquipSlot Slot) const;

    // 현재 가방의 ExtraSlots 값 조회
    int32 GetBackpackExtraSlots() const;

protected:
    UFUNCTION() void OnRep_HelmetSlot();
    UFUNCTION() void OnRep_VestSlot();
    UFUNCTION() void OnRep_BackpackSlot();
    UFUNCTION() void OnRep_WeaponSlots();

    void UpdateBackpackSlots();  // 가방 변경 시 InventoryComponent 슬롯 수 갱신

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
```

**TryAutoEquip 흐름 (장비 줍기 시)**:

```
TryAutoEquip(RowName)
├── DataTable에서 FBGItemRow 조회
├── Category != Equipment → return false
├── 해당 EquipSlot의 현재 장비 확인:
│   ├── 빈 슬롯 → 바로 장착 → return true
│   ├── 현재 장비 레벨 < 새 장비 레벨 → 기존 장비를 인벤토리로 이동, 새 장비 장착 → return true
│   └── 현재 장비 레벨 >= 새 장비 레벨 → return false (인벤토리로 넘김)
```

**ApplyArmorReduction 흐름**:

```
ApplyArmorReduction(InDamage, HitSlot)
├── 해당 슬롯의 FBGEquipSlotState 조회
├── 빈 슬롯이면 → return InDamage (감소 없음)
├── ReducedDamage = InDamage * (1 - DamageReduction)
├── AbsorbedDamage = InDamage - ReducedDamage
├── CurrentDurability -= AbsorbedDamage
├── if (CurrentDurability <= 0)
│   ├── 장비 파괴: 슬롯 비움 (ItemRowName = NAME_None)
│   ├── Backpack이면 → UpdateBackpackSlots()
│   │   └── 슬롯 수 감소 시 넘치는 아이템은 월드에 드롭
│   └── OnEquipmentChanged 브로드캐스트
└── return ReducedDamage
```

**EquipItem 흐름**:

```
EquipItem(RowName)
├── Authority 검증
├── DataTable에서 FBGItemRow 조회
├── Category == Equipment, EquipSlot != None 확인
├── 해당 슬롯에 이미 장비가 있는가?
│   └── 있으면: 기존 장비를 인벤토리로 이동 (UnequipItem)
├── 인벤토리에서 아이템 제거 (InventoryComp->RemoveItem)
├── 슬롯 상태 설정:
│   ├── ItemRowName = RowName
│   └── CurrentDurability = ItemRow.MaxDurability
├── Backpack인 경우 → UpdateBackpackSlots()
├── OnRep → OnEquipmentChanged 브로드캐스트
└── return true
```

**방어구 피해 적용 흐름 (DamageSystem 연동)**:

```
데미지 경로 (피해 적용 주체 미정):
├── 피격 부위 판정 (머리/몸통) → HitSlot 결정
├── float ReducedDamage = EquipComp->ApplyArmorReduction(Damage, HitSlot)
└── HealthComp->ApplyDamage(ReducedDamage, Instigator)
```

> 피해 적용 경로의 정확한 위치(DamageSystem vs 별도 컴포넌트)는 추후 결정.

### 6. `UBGInteractionComponent` (ActorComponent)

주변 아이템 감지와 줍기를 담당합니다. 장비 아이템은 줍기 시 자동 장착을 시도합니다.

**Owner**: `ABGCharacter`

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNearbyItemsChanged, const TArray<FBGNearbyItem>&, NearbyItems);

USTRUCT(BlueprintType)
struct FBGNearbyItem
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> ItemActor;
    UPROPERTY(BlueprintReadOnly) FName ItemRowName;
    UPROPERTY(BlueprintReadOnly) int32 Count;
    UPROPERTY(BlueprintReadOnly) float Distance;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UBGInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // 감지 범위
    UPROPERTY(EditDefaultsOnly, Category = "Interaction")
    float DetectionRadius = 300.f;

    // 줍기 가능 최대 거리
    UPROPERTY(EditDefaultsOnly, Category = "Interaction")
    float PickupRange = 200.f;

    // 감지 주기 (성능 최적화)
    UPROPERTY(EditDefaultsOnly, Category = "Interaction")
    float ScanInterval = 0.25f;

    UPROPERTY(BlueprintAssignable)
    FOnNearbyItemsChanged OnNearbyItemsChanged;

    // 주변 아이템 목록 (클라이언트 로컬, 복제 불필요)
    UPROPERTY(BlueprintReadOnly, Category = "Interaction")
    TArray<FBGNearbyItem> NearbyItems;

    // F키 입력 시: 가장 가까운 아이템 줍기
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void PickupClosest();

    // UI에서 특정 아이템 클릭 시: 해당 아이템 줍기
    UFUNCTION(Server, Reliable)
    void Server_PickupItem(AActor* ItemActor);

protected:
    virtual void TickComponent(float DeltaTime, ...) override;

    float ScanAccumulator = 0.f;
    void ScanNearbyItems();  // Overlap 기반 주변 스캔
};
```

**주변 아이템 감지 (클라이언트 Tick)**:

```
TickComponent(DeltaTime)  [Owning Client]
├── ScanAccumulator += DeltaTime
├── if (ScanAccumulator >= ScanInterval)
│   ├── ScanAccumulator = 0
│   └── ScanNearbyItems()
│       ├── OverlapMultiByChannel(구체 감지, DetectionRadius)
│       ├── ABGPickupItem인 액터만 필터
│       ├── NearbyItems 배열 갱신 (거리순 정렬)
│       └── OnNearbyItemsChanged 브로드캐스트 → UI 갱신
```

**줍기 흐름 (장비 자동 장착 포함)**:

```
[Client]                                  [Server]
   │                                         │
   ├─ F키 / UI 클릭                          │
   │──Server_PickupItem(ItemActor)─────────>│
   │                                         ├─ ItemActor 유효성 검증
   │                                         ├─ 거리 검증 (PickupRange)
   │                                         ├─ DataTable에서 FBGItemRow 조회
   │                                         │
   │                                         ├─ [장비 아이템인 경우]
   │                                         │   ├─ EquipComp->TryAutoEquip(RowName)
   │                                         │   ├─ 성공 시 → ItemActor->Destroy(), 끝
   │                                         │   └─ 실패 시 → 아래 인벤토리 추가로 진행
   │                                         │
   │                                         ├─ [일반 아이템 / 자동장착 실패]
   │                                         │   ├─ InventoryComp->AddItem(RowName, Count)
   │                                         │   └─ 슬롯 부족 시 실패 → 클라이언트에 알림
   │                                         │
   │                                         ├─ 성공 시: ItemActor->Destroy()
   │                                         │
   │<──OnRep_Slots (인벤토리 갱신)──────────│
   │<──OnRep_EquipSlot (장비 갱신)──────────│
```

### 7. `ABGPickupItem` (Actor)

월드에 배치되는 줍기 가능한 아이템 액터.

```cpp
UCLASS()
class ABGPickupItem : public AActor
{
    GENERATED_BODY()

public:
    ABGPickupItem();

    // 이 액터가 나타내는 아이템 (DT_Items의 RowName)
    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Item")
    FName ItemRowName;

    // 수량
    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Item")
    int32 Count = 1;

    UPROPERTY(EditDefaultsOnly, Category = "Item")
    TObjectPtr<UDataTable> ItemDataTable;

protected:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> MeshComp;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> CollisionComp;

    virtual void BeginPlay() override;
    // BeginPlay에서 DataTable 조회 → MeshComp에 PickupMesh 설정
};
```

### 8. `ABGItemSpawnPoint` (Actor)

에디터에 배치하는 아이템 스폰 포인트. 매치 시작 시 랜덤 아이템 테이블에서 아이템을 뽑아 `ABGPickupItem`을 스폰합니다.

```cpp
// 스폰 테이블 항목
USTRUCT(BlueprintType)
struct FBGItemSpawnEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) FName ItemRowName;  // DT_Items의 RowName
    UPROPERTY(EditAnywhere) int32 MinCount = 1;
    UPROPERTY(EditAnywhere) int32 MaxCount = 1;
    UPROPERTY(EditAnywhere) float Weight = 1.f; // 확률 가중치 (높을수록 자주 스폰)
};

UCLASS()
class ABGItemSpawnPoint : public AActor
{
    GENERATED_BODY()

public:
    // 이 스폰 포인트에서 나올 수 있는 아이템 목록과 확률
    UPROPERTY(EditAnywhere, Category = "Spawn")
    TArray<FBGItemSpawnEntry> SpawnTable;

    // 아무것도 스폰되지 않을 확률 (0~1, 0.3 = 30% 확률로 빈 스폰)
    UPROPERTY(EditAnywhere, Category = "Spawn")
    float EmptyChance = 0.2f;

    // 참조할 아이템 DataTable
    UPROPERTY(EditDefaultsOnly, Category = "Spawn")
    TObjectPtr<UDataTable> ItemDataTable;

    // 서버에서 호출: 랜덤 아이템 스폰
    UFUNCTION(BlueprintCallable, Category = "Spawn")
    void SpawnRandomItem();

protected:
    virtual void BeginPlay() override;
    // BeginPlay에서 서버만 SpawnRandomItem() 호출
};
```

**스폰 흐름 (서버, 매치 시작 시)**:

```
BeginPlay() [서버]
└── SpawnRandomItem()
    ├── FMath::FRand() < EmptyChance → 스폰 안 함, 종료
    ├── 가중치 기반 랜덤 선택: SpawnTable에서 1개 FBGItemSpawnEntry 뽑기
    ├── Count = FMath::RandRange(MinCount, MaxCount)
    ├── ABGPickupItem 스폰 (이 액터 위치, RowName, Count)
    └── 스폰된 PickupItem은 복제되어 모든 클라이언트에 보임
```

> 스폰 포인트는 에디터에서 레벨에 수동 배치합니다.
> SpawnTable은 스폰 포인트별로 다르게 설정할 수 있어 (건물 안 = 방어구 위주, 야외 = 소모품 위주 등) 지역별 룻 차별화가 가능합니다.

### 9. DamageSystem 연동 변경

기존 데미지 경로에 방어구를 삽입합니다. **피해 적용 주체는 미정**이므로 인터페이스만 정의합니다.

```
기존: DamageSystem → PlayerState::ApplyDamage(RawDamage)

변경: [피해 적용 주체] → EquipmentComponent::ApplyArmorReduction(RawDamage, HitSlot)
                       → HealthComponent::ApplyDamage(ReducedDamage, Instigator)
```

피격 부위(HitSlot) 판정:
- Sphere Trace의 HitResult에서 BoneName 확인
- 머리 본(head)에 해당하면 → Helmet 슬롯
- 나머지 → Vest 슬롯
- 본 판정 불가 시 → Vest (기본값)

### 10. UI 구조 (C++ UserWidget 기반)

UI 로직은 최대한 C++ `UUserWidget` 서브클래스에서 처리하고, UMG Editor는 비주얼 레이아웃만 담당합니다.

#### `UBGInventoryWidget` (Tab 전체 인벤토리 화면)

```cpp
UCLASS()
class UBGInventoryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 인벤토리 슬롯 그리드 (UMG에서 바인딩)
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UUniformGridPanel> SlotGrid;

    // 장비 슬롯 영역
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBGInventorySlotWidget> HelmetSlotWidget;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBGInventorySlotWidget> VestSlotWidget;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBGInventorySlotWidget> BackpackSlotWidget;

    // 주변 아이템 목록
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UScrollBox> NearbyItemList;

    // 컴포넌트 참조 (NativeConstruct에서 캐싱)
    void SetOwnerComponents(UBGInventoryComponent* InvComp, UBGEquipmentComponent* EquipComp, UBGInteractionComponent* InterComp);

protected:
    virtual void NativeConstruct() override;

    // 델리게이트 핸들러: 컴포넌트의 OnChanged에 바인딩
    UFUNCTION() void RefreshInventorySlots();
    UFUNCTION() void RefreshEquipmentSlots();
    UFUNCTION() void RefreshNearbyItems(const TArray<FBGNearbyItem>& Items);

    // 드래그 앤 드롭: 인벤토리 슬롯 → 화면 밖 = 버리기
    // UMG의 OnDrop / NativeOnDragDetected 오버라이드
};
```

#### `UBGInventorySlotWidget` (개별 슬롯)

```cpp
UCLASS()
class UBGInventorySlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> ItemIcon;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CountText;

    // 슬롯 데이터 설정
    void SetSlotData(const FBGInventorySlot& SlotData, const FBGItemRow* ItemRow);
    void SetEmpty();

    // 이 슬롯의 인덱스 (드래그/드롭, 버리기에 사용)
    int32 SlotIndex = INDEX_NONE;

protected:
    // 드래그 시작: NativeOnDragDetected
    // 클릭: 아이템 사용 (소모품) 또는 장착 (장비)
};
```

#### `UBGNearbyItemWidget` (주변 아이템 항목)

```cpp
UCLASS()
class UBGNearbyItemWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> ItemIcon;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ItemName;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CountText;

    void SetItemData(const FBGNearbyItem& NearbyItem, const FBGItemRow* ItemRow);

    // 클릭 시 → InteractionComp->Server_PickupItem(ItemActor)
    TWeakObjectPtr<AActor> BoundItemActor;
};
```

#### `UBGQuickSlotWidget` (퀵슬롯 HUD)

```cpp
UCLASS()
class UBGQuickSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 무기 슬롯 5개 표시
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UHorizontalBox> WeaponSlotBox;

    // 소모품 퀵슬롯 (회복 아이템 등)
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UHorizontalBox> ConsumableSlotBox;

    // 현재 선택된 무기 슬롯 하이라이트
    UPROPERTY(BlueprintReadOnly) EBGWeaponSlot ActiveWeaponSlot = EBGWeaponSlot::None;

    void SetOwnerComponents(UBGEquipmentComponent* EquipComp, UBGInventoryComponent* InvComp);

protected:
    virtual void NativeConstruct() override;

    UFUNCTION() void RefreshWeaponSlots();
    UFUNCTION() void RefreshConsumableSlots();
};
```

**UI 흐름**:

```
[Tab 키 누름]
├── PlayerController → InventoryWidget->SetVisibility(Toggle)
├── 열릴 때: SetInputMode(GameAndUI), bShowMouseCursor = true
└── 닫힐 때: SetInputMode(GameOnly), bShowMouseCursor = false

[인벤토리 아이템 드래그 → 화면 밖]
├── NativeOnDrop 감지
└── InventoryComp->Server_DropItem(SlotIndex, Count)

[주변 아이템 클릭]
└── InteractionComp->Server_PickupItem(ItemActor)

[퀵슬롯 HUD]
├── 항상 표시
├── EquipmentComp::OnWeaponSlotChanged → RefreshWeaponSlots()
└── InventoryComp::OnInventoryChanged → RefreshConsumableSlots()
```

### 11. 컴포넌트 간 의존 관계

```
UBGInventoryComponent
  ├── IBGInventoryInterface 구현 (HealingComponent가 사용)
  └── DataTable (FBGItemRow)

UBGEquipmentComponent
  ├──참조──> UBGInventoryComponent (장착/해제 시 아이템 이동)
  └── DataTable (FBGItemRow)

UBGInteractionComponent
  ├──참조──> UBGInventoryComponent (줍기 시 AddItem)
  ├──참조──> UBGEquipmentComponent (장비 자동 장착 TryAutoEquip)
  └──감지──> ABGPickupItem (월드 액터)

ABGItemSpawnPoint
  └──스폰──> ABGPickupItem (매치 시작 시)

[피해 적용 주체 - 미정]
  └──참조──> UBGEquipmentComponent (방어구 피해 감소)
  └──참조──> UBGHealthComponent (데미지 적용)

UBGHealingComponent
  └──참조──> IBGInventoryInterface (HasHealItem, ConsumeHealItem)

UI (C++ UserWidget)
  ├──참조──> UBGInventoryComponent (슬롯 데이터, 델리게이트)
  ├──참조──> UBGEquipmentComponent (장비 데이터, 델리게이트)
  └──참조──> UBGInteractionComponent (주변 아이템, 줍기 RPC)
```

### 12. 네트워크 복제 요약

| 컴포넌트                | 복제 프로퍼티                                           | OnRep 용도                |
| ----------------------- | ------------------------------------------------------- | ------------------------- |
| UBGInventoryComponent   | `Slots[]`, `MaxSlotCount`                               | 인벤토리 UI 갱신          |
| UBGEquipmentComponent   | `HelmetSlot`, `VestSlot`, `BackpackSlot`, `WeaponSlots` | 장비/무기 UI, 캐릭터 외형 |
| UBGInteractionComponent | 없음 (클라이언트 로컬 스캔)                             | —                         |
| ABGPickupItem           | `ItemRowName`, `Count`                                  | 월드 아이템 표시          |

### 13. 클래스 다이어그램

```
┌───────────────────────────────────────────────────────────────────────────┐
│                              ABGCharacter                                 │
│                                                                           │
│ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐                        │
│ │ UBGInventory │ │ UBGEquipment │ │ UBGInter-    │                        │
│ │ Component    │ │ Component    │ │ action Comp  │                        │
│ │              │ │              │ │              │                        │
│ │ Slots[]      │ │ HelmetSlot   │ │ NearbyItems  │                        │
│ │ MaxSlotCount │ │ VestSlot     │ │ DetectRadius │                        │
│ │              │ │ BackpackSlot │ │              │                        │
│ │ AddItem()    │ │ WeaponSlots  │ │ Scan()       │                        │
│ │ RemoveItem() │ │              │ │ PickupClose()│                        │
│ │ DropItem()   │ │ Equip()      │ │ Server_Pick()│                        │
│ │ HasHealItem()│ │ TryAutoEquip │ │              │                        │
│ │              │ │ ArmorReduce()│ │              │                        │
│ └──────────────┘ └──────────────┘ └──────────────┘                        │
│                                                                           │
│ ┌─ 기존 Health 시스템 ────────────────────────────────────────────────┐   │
│ │ UBGHealthComp / UBGBoostComp / UBGHealingComp / UBGDBNOComp        │   │
│ └────────────────────────────────────────────────────────────────────┘   │
└───────────────────────────────────────────────────────────────────────────┘
         │ 감지/줍기               │ 스폰
         v                         v
┌─────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│ ABGPickupItem   │     │ ABGItemSpawnPoint │     │ UDataTable       │
│ (월드 액터)     │<────│ (에디터 배치)     │     │ DT_Items         │
│                 │     │                   │     │ (FBGItemRow)     │
│ ItemRowName     │     │ SpawnTable[]      │     │                  │
│ Count           │     │ EmptyChance       │     │ 스택/방어구      │
│ MeshComp        │     │ SpawnRandomItem() │     │ 아이콘/메시      │
└─────────────────┘     └──────────────────┘     └──────────────────┘

┌─ UI (C++ UserWidget) ────────────────────────────────────────────┐
│                                                                   │
│ ┌──────────────────┐  ┌───────────────────┐  ┌────────────────┐  │
│ │ UBGInventory     │  │ UBGNearbyItem     │  │ UBGQuickSlot   │  │
│ │ Widget (Tab)     │  │ Widget            │  │ Widget (HUD)   │  │
│ │                  │  │                   │  │                │  │
│ │ SlotGrid         │  │ ItemIcon/Name     │  │ WeaponSlotBox  │  │
│ │ EquipSlots       │  │ 클릭→줍기         │  │ ConsumableBox  │  │
│ │ NearbyItemList   │  │                   │  │                │  │
│ │ 드래그→버리기    │  │                   │  │ 항상 표시      │  │
│ └──────────────────┘  └───────────────────┘  └────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
```

### 14. 파일 구조

```
Source/PUBG_HotMode/GEONU/
├── Public/
│   ├── Inventory/
│   │   ├── BGItemRow.h               (FBGItemRow, EBGItemCategory, EBGEquipSlot, EBGWeaponSlot)
│   │   ├── BGInventoryComponent.h    (슬롯 기반 아이템 보관, IBGInventoryInterface 구현)
│   │   ├── BGEquipmentComponent.h    (장비 슬롯 + 무기 슬롯, 방어구, 자동 장착)
│   │   ├── BGInteractionComponent.h  (주변 감지, 줍기, 자동 장착 연동)
│   │   ├── BGPickupItem.h            (월드 아이템 액터)
│   │   └── BGItemSpawnPoint.h        (스폰 포인트, 랜덤 아이템 테이블)
│   └── UI/
│       ├── BGInventoryWidget.h       (Tab 전체 인벤토리 화면)
│       ├── BGInventorySlotWidget.h   (개별 슬롯 위젯)
│       ├── BGNearbyItemWidget.h      (주변 아이템 항목)
│       └── BGQuickSlotWidget.h       (퀵슬롯 HUD)
└── Private/
    ├── Inventory/
    │   ├── BGInventoryComponent.cpp
    │   ├── BGEquipmentComponent.cpp
    │   ├── BGInteractionComponent.cpp
    │   ├── BGPickupItem.cpp
    │   └── BGItemSpawnPoint.cpp
    └── UI/
        ├── BGInventoryWidget.cpp
        ├── BGInventorySlotWidget.cpp
        ├── BGNearbyItemWidget.cpp
        └── BGQuickSlotWidget.cpp

Content/GEONU/
├── Data/
│   └── DT_Items.uasset              (DataTable — FBGItemRow)
└── UI/
    ├── WBP_Inventory.uasset          (UMG — UBGInventoryWidget 기반)
    ├── WBP_InventorySlot.uasset      (UMG — UBGInventorySlotWidget 기반)
    ├── WBP_NearbyItem.uasset         (UMG — UBGNearbyItemWidget 기반)
    └── WBP_QuickSlot.uasset          (UMG — UBGQuickSlotWidget 기반)
```

### 15. 구현 순서

| 단계 | 작업                                                           | 의존성               |
| ---- | -------------------------------------------------------------- | -------------------- |
| 1    | `BGItemRow.h` — 구조체/열거형 정의                             | 없음                 |
| 2    | `UBGInventoryComponent` 구현 (슬롯 기반)                       | 단계 1               |
| 3    | `UBGEquipmentComponent` 구현 (방어구 + 무기 슬롯 + 자동 장착)  | 단계 1, 2            |
| 4    | `ABGPickupItem` 구현                                           | 단계 1               |
| 5    | `UBGInteractionComponent` 구현 (감지 + 줍기 + 자동 장착 연동)  | 단계 2, 3, 4         |
| 6    | `ABGItemSpawnPoint` 구현 (스폰 포인트 + 랜덤 테이블)           | 단계 4               |
| 7    | `ABGCharacter`에 컴포넌트 등록                                 | 단계 2, 3, 5         |
| 8    | `DT_Items` DataTable 에셋 생성 (에디터)                        | 단계 1               |
| 9    | 입력 바인딩 (F키 줍기, Tab 인벤토리, 1~5 무기 선택)            | 단계 7               |
| 10   | `UBGInventorySlotWidget` — 슬롯 위젯 (C++ + UMG)               | 단계 1               |
| 11   | `UBGNearbyItemWidget` — 주변 아이템 위젯                       | 단계 1               |
| 12   | `UBGInventoryWidget` — 전체 인벤토리 화면 (드래그 버리기 포함) | 단계 2, 3, 5, 10, 11 |
| 13   | `UBGQuickSlotWidget` — 퀵슬롯 HUD                              | 단계 3               |
| 14   | 아이템 버리기 (`Server_DropItem` → 월드 드롭)                  | 단계 2, 4            |
| 15   | 레벨에 스폰 포인트 배치 + 테스트                               | 단계 6, 8            |
| 16   | 통합 테스트 (멀티플레이어)                                     | 전체                 |