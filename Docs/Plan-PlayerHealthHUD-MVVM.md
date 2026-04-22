# BG_PlayerHealthWidget 설계

## 결론

- 구조
  - `Widget + Provider + ViewData`
- 플러그인
  - UMG ViewModel 미사용
- 수정 범위
  - `GEONU` 내부 우선
- 실제 연결
  - 현재는 `Health`만
- 미구현 항목
  - 필드와 함수만 준비
  - 실제 gameplay component 참조 금지

## 파일 범위

- 수정
  - [Source/PUBG_HotMode/GEONU/BG_PlayerHealthWidget.h](/C:/Users/me/source/P4/PUBG_HotMode/Source/PUBG_HotMode/GEONU/BG_PlayerHealthWidget.h:13)
  - [Source/PUBG_HotMode/GEONU/BG_PlayerHealthWidget.cpp](/C:/Users/me/source/P4/PUBG_HotMode/Source/PUBG_HotMode/GEONU/BG_PlayerHealthWidget.cpp:4)
- 신규
  - `Source/PUBG_HotMode/GEONU/BG_HUDDataProvider.h`
  - `Source/PUBG_HotMode/GEONU/BG_HUDDataProvider.cpp`
- 수정 회피
  - `Source/PUBG_HotMode/WON/...`
  - `Build.cs`
  - `.uproject`

## ViewData 분리

- 1. `Health + Boost + DBNO`
  - 같은 바 영역
  - 수치형 표시 묶음
- 2. `Helmet + Vest + Backpack`
  - 장비 아이콘 영역
- 3. `Boost Run + Regeneration`
  - 상태 아이콘 영역
- 4. `Breath`
  - 별도 표시 조건이 강함

## ViewData 구조 제안

```cpp
USTRUCT(BlueprintType)
struct FBG_HealthBoostDBNOViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float HealthPercent = 1.0f;

    UPROPERTY(BlueprintReadOnly)
    float BoostPercent = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float DBNOPercent = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    bool bIsDead = false;

    UPROPERTY(BlueprintReadOnly)
    bool bIsDBNO = false;
};

USTRUCT(BlueprintType)
struct FBG_ArmorViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 HelmetLevel = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 VestLevel = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 BackpackLevel = 0;
};

USTRUCT(BlueprintType)
struct FBG_StatusEffectViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bHasBoostRun = false;

    UPROPERTY(BlueprintReadOnly)
    bool bHasRegeneration = false;
};

USTRUCT(BlueprintType)
struct FBG_BreathViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float BreathPercent = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    bool bShouldShowBreath = false;
};

USTRUCT(BlueprintType)
struct FBG_PlayerHealthHUDData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FBG_HealthBoostDBNOViewData MainBars;

    UPROPERTY(BlueprintReadOnly)
    FBG_ArmorViewData Armor;

    UPROPERTY(BlueprintReadOnly)
    FBG_StatusEffectViewData StatusEffects;

    UPROPERTY(BlueprintReadOnly)
    FBG_BreathViewData Breath;
};
```

## 책임 분리

### `UBG_PlayerHealthWidget`

- View 전용
- `Provider` 생성
- `Provider`의 `CachedData` 적용
- Gameplay 객체 직접 탐색 금지

### `UBG_HUDDataProvider`

- `UObject`
- 로컬 `OwningPlayer`, `Pawn`, `PlayerState` 확보
- `FBG_PlayerHealthHUDData` 갱신
- 미래 gameplay component 연결 지점 제공

## 현재 실제 연결 범위

- 연결
  - `MainBars.HealthPercent`
  - `MainBars.bIsDead`
- 소스
  - `ABG_PlayerState::CurrentHP`
  - `ABG_PlayerState::MaxHP`
  - `ABG_PlayerState::bIsDead`

## 현재 연결하지 않을 항목

- `MainBars.BoostPercent`
- `MainBars.DBNOPercent`
- `MainBars.bIsDBNO`
- `Armor.*`
- `StatusEffects.*`
- `Breath.*`

## Provider 구현 기준

- 실제 `HealthComponent`, `BoostComponent`, `DBNOComponent`, `EquipmentComponent`, `BreathComponent` 구현 금지
- Provider에는 조회 함수 틀만 둠
- 초기값만 반환

```cpp
float ResolveBoostPercent() const { return 0.0f; }
float ResolveDBNOPercent() const { return 0.0f; }
bool ResolveIsDBNO() const { return false; }
int32 ResolveHelmetLevel() const { return 0; }
bool ResolveHasBoostRun() const { return false; }
float ResolveBreathPercent() const { return 0.0f; }
bool ResolveShouldShowBreath() const { return false; }
```

## 참조 방향

- `Widget -> Provider`
- `Provider -> PlayerController`
- `Provider -> Pawn`
- `Provider -> PlayerState`

금지:

- `Widget -> PlayerState`
- `Widget -> Character`
- `Widget -> future component`

## 갱신 방식

- 1차
  - `NativeConstruct`
  - `NativeTick`
- 이유
  - `WON` 수정 없이 가능
  - `PlayerState` 외부 갱신 이벤트 없음

## API 초안

```cpp
UCLASS()
class PUBG_HOTMODE_API UBG_HUDDataProvider : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(APlayerController* InOwningPlayer);
    void Refresh();

    const FBG_PlayerHealthHUDData& GetCachedData() const { return CachedData; }

protected:
    void ResolveReferences();
    void UpdateHealthData();
    void UpdateFutureStubData();

protected:
    UPROPERTY(Transient)
    TWeakObjectPtr<APlayerController> OwningPlayerController;

    UPROPERTY(Transient)
    TWeakObjectPtr<APawn> CachedPawn;

    UPROPERTY(Transient)
    TWeakObjectPtr<class ABG_PlayerState> CachedPlayerState;

    UPROPERTY(Transient)
    FBG_PlayerHealthHUDData CachedData;
};
```

## 구현 순서

1. `FBG_PlayerHealthHUDData`와 하위 `ViewData` 정의
2. `UBG_HUDDataProvider` 생성
3. `Health`만 실제 연결
4. 나머지 항목은 stub 함수만 추가
5. `BG_PlayerHealthWidget`에서 `RefreshHUD()`와 `ApplyHUDData()` 연결

## 참고

- `BG_HUDDataProvider.h`
  - 현재 리포지토리에는 없음
  - 신규 생성 필요
