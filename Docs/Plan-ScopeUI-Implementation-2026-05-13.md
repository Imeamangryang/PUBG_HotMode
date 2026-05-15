# 스코프 UI 및 머즐 발사 완전 구현 계획

## 📋 문제 정의

### 현재 문제점
1. **머즐 발사 오류**: 총알이 바닥에서 발사됨 (머즐이 아닌 곳에서)
2. **스코프 카메라 미설정**: 화면 정중앙으로 카메라 조준 불가
3. **WBP_ScopeUI 연결 불명**: UI와 C++ 바인딩 방법 없음
4. **크로스헤어 미구현**: 2배/4배 스코프 구분 필요
5. **UI 가시성 토글 불명**: 2배/4배 전환 시 렌더링 처리 방법 불명

---

## 🎯 최종 목표

```
[Player Camera (Right Shoulder)]
         ↓
   [Screen Center]
         ↓
   [Muzzle Socket]
         ↓
   [Bullet Trace] → [Hit]
```

- **비스코프**: 카메라 → 화면정중앙 → 머즐 → 발사
- **스코프 (2x/4x)**: SceneCapture → 중앙 조준 → 머즐 → 발사
- **UI**: 2배/4배 전환 시 크로스헤어/스코프 가시성 토글

---

## ✅ 구현 단계

### **Phase 1: 머즐 발사 기초 보정 (1-3단계)**

#### Step 1: BG_Character 뷰포인트 검증
**목표**: 카메라 위치가 올바른지 확인

**체크리스트**:
- [ ] 에디터에서 캐릭터 선택
- [ ] "Camera Boom" 컴포넌트 확인 (Shoulder Offset 설정)
- [ ] "Camera" 컴포넌트 확인 (Location 및 Rotation)
- [ ] Play 모드에서 화면 중앙 라인트레이스 시각화 (Debug Draw)

#### Step 2: 무기 머즐 소켓 설정
**목표**: Muzzle 소켓 위치 확인

**체크리스트**:
- [ ] 에디터에서 무기 SKM (Skeletal Mesh) 또는 SM (Static Mesh) 열기
- [ ] Socket 탭에서 "Muzzle" 소켓 위치 확인
- [ ] 총구와 정확히 일치하는지 시각 확인
- [ ] `GetMuzzleTransform()` 호출로 위치 검증

#### Step 3: BG_WeaponFireComponent 발사 로직 검증
**목표**: 현재 코드 흐름 확인

**체크리스트**:
- [ ] `ResolveScreenCenterAimTarget()`: 화면 중앙 목표점 계산
- [ ] `ResolveMuzzleShotStart()`: 머즐 위치 가져오기
- [ ] `TraceSingleShot()`: 머즐 → 목표점 라인트레이스
- [ ] 모든 계산이 올바른지 Debug Draw로 확인

---

### **Phase 2: 스코프 시스템 확장 (4-7단계)**

#### Step 4: ABG_EquippedWeaponBase에 Scope 상태 추가
**파일**: `Source/PUBG_HotMode/GEONU/Combat/BG_EquippedWeaponBase.h/cpp`

**추가 항목**:
```cpp
// 스코프 참조 및 상태
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scope")
TObjectPtr<class ABG_Scope> EquippedScope = nullptr;

UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scope")
bool bHasScope = false;

UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scope")
bool bIsScopeActive = false;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scope")
float ScopeMagnification = 2.0f;  // 2x or 4x

// 함수
UFUNCTION(BlueprintCallable, Category = "Scope")
void SetScopeActive(bool bNewActive);

UFUNCTION(BlueprintPure, Category = "Scope")
bool IsScopeActive() const { return bIsScopeActive; }

UFUNCTION(BlueprintPure, Category = "Scope")
float GetScopeMagnification() const { return ScopeMagnification; }
```

#### Step 5: BG_Scope 액터 기본 구현
**파일**: `Source/PUBG_HotMode/GEONU/Combat/BG_Scope.cpp`

**구현 함수**:
- `SetScopeActive()`: 스코프 On/Off
- `GetSceneCaptureComponent()`: SceneCapture2D 반환
- `UpdateCameraView()`: 카메라 위치 업데이트 (화면 정중앙)

#### Step 6: BG_Character에 스코프 상태 진입
**파일**: `Source/PUBG_HotMode/WON/Public/Player/BG_Character.h`

**추가**:
```cpp
UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon|Scope")
bool bIsScoping = false;

UFUNCTION(BlueprintCallable, Category = "Weapon|Scope")
void RequestToggleScope();

UFUNCTION(BlueprintCallable, Category = "Weapon|Scope")
void Auth_SetScopingState(bool bNewScoping);
```

#### Step 7: 스코프 진입 시 카메라 보정
**파일**: `Source/PUBG_HotMode/WON/Private/Player/BG_Character.cpp`

**구현**:
- 스코프 활성화 시 카메라 매개변수 변경:
  - Boom Arm Length → 60 (머즐 가까이)
  - Shoulder Offset → 0 (정중앙)
  - FOV → 40 (2배) / 20 (4배)

---

### **Phase 3: WBP_ScopeUI 구현 (8-11단계)**

#### Step 8: C++ ScopeUI Widget 컨트롤러 생성
**파일**: `Source/PUBG_HotMode/WON/Public/UI/BG_ScopeUIController.h`

```cpp
UCLASS()
class PUBG_HOTMODE_API UBG_ScopeUIController : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Scope UI")
    void SetScopeMagnification(float Magnification);  // 2.0 or 4.0

    UFUNCTION(BlueprintCallable, Category = "Scope UI")
    void UpdateCrosshairPosition(const FVector2D& ScreenCenter);

    UPROPERTY(BlueprintReadWrite, Category = "Scope UI")
    class UTextureRenderTarget2D* ScopeRenderTarget;

    // 가시성 토글
    UPROPERTY(BlueprintReadWrite, Category = "Scope UI")
    bool bShow2xCrosshair = true;

    UPROPERTY(BlueprintReadWrite, Category = "Scope UI")
    bool bShow4xCrosshair = false;
};
```

#### Step 9: WBP_ScopeUI 위젯 구조 설계
**위젯 계층**:
```
Canvas Panel
├── ScopeRenderTarget_Image
│   └── 스코프 카메라 출력 (RenderTarget 텍스처)
├── Crosshair_2x_Panel (가시성 토글)
│   ├── Top_Line
│   ├── Bottom_Line
│   ├── Left_Line
│   └── Right_Line
└── Crosshair_4x_Panel (가시성 토글)
    ├── Center_Dot
    ├── Reticle_Ring
    └── Distance_Markers
```

#### Step 10: Material for Scope View
**마테리얼**: `Content/WON/Scope/M_ScopeView`

**구성**:
- SceneCapture RenderTarget을 기본 텍스처로
- 스코프 테두리 마스크 추가 (Vignette)
- 렌즈 반사 선택적 추가

#### Step 11: WBP_ScopeUI와 C++ 바인딩
**구현**:
```cpp
// BG_Character에서
UPROPERTY(BlueprintReadOnly, Category = "UI")
class UBG_ScopeUIController* ScopeUIController;

// BeginPlay에서
ScopeUIController = CreateWidget<UBG_ScopeUIController>(
    GetController<APlayerController>(),
    ScopeUIWidgetClass);
if (ScopeUIController)
{
    ScopeUIController->AddToViewport(100);
}

// 스코프 활성화 시
if (bIsScoping && EquippedScope)
{
    ScopeUIController->SetScopeMagnification(
        EquippedWeapon->GetScopeMagnification());
    ScopeUIController->AddToViewport(100);
}
else
{
    ScopeUIController->RemoveFromViewport();
}
```

---

### **Phase 4: 크로스헤어 구현 (12-15단계)**

#### Step 12: 2배 스코프 크로스헤어 (BP_Crosshair_2x)
**설계**:
```
     ┃ 4px
     ┃
─────╋─────  8px gap
     ┃
     ┃ 4px
```
**구성**:
- 4개의 Line (1x1px, 길이 8px)
- 중심 gap 8px
- 색상: 녹색 #00FF00

#### Step 13: 4배 스코프 크로스헤어 (BP_Crosshair_4x)
**설계**:
```
    ●  1px 도트
    ⊙  지름 10px 링
  ⋮ ⋮  거리 마커
```
**구성**:
- 중심 도트 (1x1px, 흰색)
- 동심원 (10px, 8px, 6px)
- 거리 마커 (상하좌우, 2px gap)
- 색상: 빨강 #FF0000

#### Step 14: 크로스헤어 가시성 토글 Logic
**구현**:
```cpp
// WBP_ScopeUI에서
if (Magnification == 2.0f)
{
    Crosshair_2x_Panel->SetVisibility(ESlateVisibility::Visible);
    Crosshair_4x_Panel->SetVisibility(ESlateVisibility::Hidden);
}
else if (Magnification == 4.0f)
{
    Crosshair_2x_Panel->SetVisibility(ESlateVisibility::Hidden);
    Crosshair_4x_Panel->SetVisibility(ESlateVisibility::Visible);
}
```

#### Step 15: 스코프 활성화/비활성화 State Machine
**상태**:
```
┌─────────┐
│ Idle    │ (비무장 또는 비스코프)
└────┬────┘
     │ RequestToggleScope()
     ↓
┌──────────────┐
│ Scoping      │ (UI 표시, 카메라 조정)
└────┬─────────┘
     │ RequestToggleScope() 또는 Reload
     ↓
┌─────────┐
│ Idle    │
└─────────┘
```

---

## 📦 파일 목록 (생성/수정)

| 파일 | 작업 | 설명 |
|------|------|------|
| BG_EquippedWeaponBase.h/cpp | 수정 | Scope 상태 추가 |
| BG_Scope.cpp | 수정 | SceneCapture 카메라 설정 |
| BG_Character.h/cpp | 수정 | 스코프 상태 진입 |
| BG_ScopeUIController.h/cpp | 생성 | Widget 컨트롤러 |
| WBP_ScopeUI | 수정 | UI 구조 완성 |
| M_ScopeView | 생성 | 스코프 렌더 마테리얼 |
| BP_Crosshair_2x | 생성 | 2배 크로스헤어 |
| BP_Crosshair_4x | 생성 | 4배 크로스헤어 |

---

## 🔍 테스트 체크리스트

- [ ] 비스코프 발사: 머즐 정렬 확인
- [ ] 비스코프 발사: 화면 정중앙 조준 확인
- [ ] 스코프 On/Off: UI 나타남/사라짐 확인
- [ ] 스코프 2배: 녹색 십자 크로스헤어 표시
- [ ] 스코프 4배: 빨강색 동심원 크로스헤어 표시
- [ ] 스코프 발사: 머즐 정렬 + 화면 정중앙 조준 확인
- [ ] 스코프 중 리로드: 자동 해제 및 복귀 확인

---

## ⏱️ 예상 소요 시간

- Phase 1: 30분 (검증)
- Phase 2: 45분 (상태 추가 및 카메라 설정)
- Phase 3: 90분 (Widget + 바인딩)
- Phase 4: 60분 (크로스헤어)
- **총 약 4시간**

