# 스코프 UI & 머즐 발사 상세 구현 가이드

> **작성 날짜**: 2026-05-13  
> **대상**: WON 팀 (무기 시스템 담당)  
> **언어**: 한글 (한글 주석 포함)

---

## 📍 현재 문제 분석

### 발견된 이슈
1. **총알 발사 위치 오류**: 머즐이 아닌 바닥에서 발사됨
   - 원인: 무기의 Muzzle 소켓 미설정 또는 소켓 위치 오류
   - 영향: 비스코프 + 스코프 모드 모두 발생

2. **스코프 카메라 미설정**: 화면 정중앙 조준 불가
   - 원인: `ABG_Scope`의 `SceneCapture2D` 카메라 설정 불완전
   - 영향: 스코프 모드에서 조준 불가능

3. **WBP_ScopeUI 연결 불명**: UI와 C++ 간 바인딩 없음
   - 원인: Widget 컨트롤러 미생성
   - 영향: 스코프 활성화 시 UI 표시 불가

4. **크로스헤어 미구현**: 2배/4배 구분 불가
   - 원인: Crosshair Blueprint 미구현
   - 영향: 시각적 조준점 없음

---

## 🔧 구현 순서

### **STEP 1: 머즐 발사 로직 검증 (디버그)**

**목표**: 발사 로직의 각 단계를 시각화하여 문제 원인 파악

**작업 파일**: 
- `WON/Private/Combat/BG_WeaponFireComponent.cpp` (수정)

**구현 코드**:

```cpp
// BG_WeaponFireComponent.cpp의 ExecuteFire() 함수 수정
// ========================================================================
// 기존 코드와 신규 코드 위치:
//   기존: "AimTraceStart = ViewLocation; ... OutAimTarget = ..."
//   이후: 아래 디버그 코드 추가

// 화면 정중앙 목표점 계산 후 디버그 시각화
if (!ResolveScreenCenterAimTarget(*FireSettings, AimTraceStart, AimTarget))
{
    // ...기존 에러 처리...
}

// 🔍 디버그: 화면 정중앙 시각화
DrawDebugPoint(GetWorld(), AimTraceStart, 15.f, FColor::Blue, false, 2.f);      // 카메라 위치
DrawDebugPoint(GetWorld(), AimTarget, 15.f, FColor::Green, false, 2.f);        // 화면 정중앙 목표
DrawDebugLine(GetWorld(), AimTraceStart, AimTarget, FColor::Green, false, 2.f, 0, 2.f);

if (GEngine)
{
    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green,
        FString::Printf(TEXT("화면 정중앙: %.0f, %.0f, %.0f"),
            AimTarget.X, AimTarget.Y, AimTarget.Z));
}

// 머즐 위치 계산 후 디버그 시각화
if (!ResolveMuzzleShotStart(EquippedWeapon, ShotStart))
{
    ShotStart = AimTraceStart;
}

// 🔍 디버그: 머즐 위치 시각화
DrawDebugPoint(GetWorld(), ShotStart, 20.f, FColor::Red, false, 2.f);          // 머즐 위치
DrawDebugLine(GetWorld(), ShotStart, AimTarget, FColor::Red, false, 2.f, 0, 2.f);

if (GEngine)
{
    GEngine->AddOnScreenDebugMessage(-2, 2.f, FColor::Red,
        FString::Printf(TEXT("머즐 위치: %.0f, %.0f, %.0f"),
            ShotStart.X, ShotStart.Y, ShotStart.Z));
}

// 🔍 디버그: 최종 발사 방향
const FVector BaseShotDirection = (AimTarget - ShotStart).GetSafeNormal();
DrawDebugLine(GetWorld(), ShotStart, ShotStart + (BaseShotDirection * 3000.f),
    FColor::Yellow, false, 2.f, 0, 3.f);

UE_LOG(LogBGWeaponFire, Warning,
    TEXT("🎯 FIRE DEBUG | CameraPos: %s | ScreenCenter: %s | MuzzlePos: %s | Direction: %s"),
    *AimTraceStart.ToString(), *AimTarget.ToString(),
    *ShotStart.ToString(), *BaseShotDirection.ToString());
```

**검증 방법**:
1. Play 모드 진입
2. 총을 발사 → 화면에 다음이 표시되는지 확인:
   - 🔵 파란점: 카메라 위치 (오른쪽 어깨)
   - 🟢 초록점: 화면 정중앙 목표점
   - 🔴 빨간점: 머즐 위치
   - 초록선: 카메라→화면정중앙 라인
   - 빨간선: 머즐→화면정중앙 라인
   - 노란선: 발사 방향

**기대 결과**:
```
정상 동작:    빨간선이 화면 정중앙(초록점) 방향으로 향해야 함
오류 사례 1:  머즐 위치가 바닥에 있음 (소켓 오류)
오류 사례 2:  빨간선이 화면정중앙이 아닌 다른 방향을 향함
```

---

### **STEP 2: 무기 메시 Muzzle 소켓 설정 (에디터)**

**목표**: 무기별 머즐 소켓 정확한 위치 설정

**작업 위치**: 에디터
- Content Browser → 무기 SKM (예: `SK_Rifle_01`)
- 또는 유아클 블루프린트 선택

**단계별 작업**:

1. **SKM 열기**
   ```
   Content 폴더 → Characters/Weapons/ → SK_Rifle_01 (더블클릭)
   ```

2. **Socket 탭으로 이동**
   ```
   좌측 상단 "Skeleton" 탭 → "Sockets" 아이콘 클릭
   ```

3. **기존 Muzzle 소켓 확인**
   ```
   - 검색창에 "Muzzle" 입력
   - 없으면 우측마우스 → "Add Socket"
   - 이름: "Muzzle"
   - Parent Bone: 총구 끝 뼈 (예: "barrel_socket")
   ```

4. **소켓 위치 정확히 맞추기**
   ```
   - Socket Preview 활성화 (좌측 하단 "Socket" 체크박스)
   - 3D 뷰에서 수동으로 소켓 위치 조정
   - 총구 끝에 정확히 배치
   - Transform 값 확정 (예: Offset = 0, 0, 10)
   ```

5. **저장**
   ```
   Ctrl+S로 저장
   ```

**검증**:
- [ ] 모든 무기(Pistol, Rifle, Shotgun, Sniper)에 "Muzzle" 소켓 존재
- [ ] 각 소켓이 총구 끝에 정확히 위치

---

### **STEP 3: BG_Character에 스코프 상태 추가**

**목표**: 스코프 활성화/비활성화 상태 및 카메라 설정 기능 추가

**작업 파일**:
- `WON/Public/Player/BG_Character.h` (헤더 추가)
- `WON/Private/Player/BG_Character.cpp` (구현)

**헤더 파일 수정** (`BG_Character.h`):

```cpp
// ============================================================================
// [추가 위치] public 섹션의 다른 함수들 바로 아래에 추가
// ============================================================================

public:
    // 🔍 스코프 관련 함수들
    
    /// 스코프 토글 요청 (클라이언트에서 호출)
    /// 비스코프 ↔ 스코프 모드 전환
    UFUNCTION(BlueprintCallable, Category = "Weapon|Scope")
    void RequestToggleScope();

    /// 서버에서 스코프 상태 설정
    /// @param bNewScoping: true = 스코프 모드, false = 비스코프 모드
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SetScopingState(bool bNewScoping);

    /// 스코프 상태 조회
    UFUNCTION(BlueprintPure, Category = "Weapon|Scope")
    bool IsScoping() const { return bIsScoping; }

    /// 스코프 카메라 설정 (보정)
    /// 배율별로 다른 FOV 및 카메라 위치 적용
    /// @param Magnification: 스코프 배율 (2.0, 4.0 등)
    UFUNCTION(BlueprintCallable, Category = "Weapon|Scope")
    void ApplyScopeCamera(float Magnification);

    /// 기본 카메라로 복원 (비스코프 모드)
    UFUNCTION(BlueprintCallable, Category = "Weapon|Scope")
    void RestoreCameraFromScope();

    // ============================================================================
    // [기존 콘텐츠 이후]
    // ============================================================================

private:
    // 🔍 스코프 관련 상태 변수들
    
    /// 현재 스코프 활성화 여부
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, 
        Category = "Weapon|Scope", meta = (AllowPrivateAccess = "true"))
    bool bIsScoping = false;

    /// 기본 카메라 Boom 길이 저장 (스코프 복귀 시 사용)
    UPROPERTY(VisibleInstanceOnly, Category = "Camera|Scope", meta = (AllowPrivateAccess = "true"))
    float SavedCameraBoomLength = 150.0f;

    /// 기본 카메라 Socket Offset 저장
    UPROPERTY(VisibleInstanceOnly, Category = "Camera|Scope", meta = (AllowPrivateAccess = "true"))
    FVector SavedCameraBoomOffset = FVector(0.0f, 40.0f, 60.0f);

    /// 기본 카메라 FOV 저장
    UPROPERTY(VisibleInstanceOnly, Category = "Camera|Scope", meta = (AllowPrivateAccess = "true"))
    float SavedCameraFOV = 90.0f;
```

**구현 파일 수정** (`BG_Character.cpp`):

```cpp
// ============================================================================
// [위치: BeginPlay() 또는 적절한 초기화 함수]
// ============================================================================

void ABG_Character::BeginPlay()
{
    Super::BeginPlay();

    // ... 기존 코드 ...

    // 🔍 카메라 기본값 저장
    if (CameraBoom)
    {
        SavedCameraBoomLength = CameraBoom->TargetArmLength;
        SavedCameraBoomOffset = CameraBoom->SocketOffset;
    }
    if (FollowCamera)
    {
        SavedCameraFOV = FollowCamera->FieldOfView;
    }
}

// ============================================================================
// [신규 함수 구현 추가]
// ============================================================================

void ABG_Character::RequestToggleScope()
{
    // 클라이언트에서 스코프 토글 요청
    // 무기가 스코프를 가지고 있는지 확인
    
    if (!HasAuthority())
    {
        Server_SetScopingState(!bIsScoping);
        return;
    }

    // 서버에서 직접 처리
    Server_SetScopingState_Implementation(!bIsScoping);
}

void ABG_Character::Server_SetScopingState_Implementation(bool bNewScoping)
{
    // ✅ [핵심] 스코프 상태 설정 - 모든 클라이언트에 동기화
    
    UBG_EquipmentComponent* EquipmentComponent = GetEquipmentComponent();
    if (!EquipmentComponent)
    {
        UE_LOG(LogBGCharacter, Warning, 
            TEXT("%s: Server_SetScopingState failed - EquipmentComponent null"), 
            *GetNameSafe(this));
        return;
    }

    ABG_EquippedWeaponBase* EquippedWeapon = EquipmentComponent->GetActiveEquippedWeaponActor();
    if (!EquippedWeapon || !EquippedWeapon->HasScope())
    {
        // 스코프가 없으면 상태 변경 불가
        UE_LOG(LogBGCharacter, Warning,
            TEXT("%s: Cannot scope - weapon has no scope"), 
            *GetNameSafe(this));
        bIsScoping = false;
        return;
    }

    bIsScoping = bNewScoping;

    if (bIsScoping)
    {
        // 🎯 스코프 모드 진입
        CharacterState = EBGCharacterState::Scoped;
        float Magnification = EquippedWeapon->GetScopeMagnification();
        ApplyScopeCamera(Magnification);
        
        // 스코프 액터 활성화
        if (ABG_Scope* ScopeActor = EquippedWeapon->GetScopeActor())
        {
            ScopeActor->SetScopeActive(true);
        }

        UE_LOG(LogBGCharacter, Warning,
            TEXT("%s: 🎯 Scoping activated (%.1fx)"),
            *GetNameSafe(this), Magnification);
    }
    else
    {
        // 🔄 스코프 모드 종료
        CharacterState = EBGCharacterState::Idle;
        RestoreCameraFromScope();

        // 스코프 액터 비활성화
        if (ABG_EquippedWeaponBase* Weapon = EquipmentComponent->GetActiveEquippedWeaponActor())
        {
            if (ABG_Scope* ScopeActor = Weapon->GetScopeActor())
            {
                ScopeActor->SetScopeActive(false);
            }
        }

        UE_LOG(LogBGCharacter, Warning,
            TEXT("%s: 🔄 Scoping deactivated"),
            *GetNameSafe(this));
    }
}

bool ABG_Character::Server_SetScopingState_Validate(bool bNewScoping)
{
    return true;  // 나중에 필요하면 체크 추가
}

void ABG_Character::ApplyScopeCamera(float Magnification)
{
    // ✅ [핵심] 스코프 카메라 설정
    // 배율별로 FOV와 카메라 위치 조정
    
    if (!CameraBoom || !FollowCamera)
    {
        UE_LOG(LogBGCharacter, Error,
            TEXT("%s: ApplyScopeCamera failed - Camera components null"),
            *GetNameSafe(this));
        return;
    }

    // 📏 배율별 카메라 설정
    if (FMath::IsNearlyEqual(Magnification, 2.0f))
    {
        // 🔍 2배 스코프
        // - FOV: 45도 (정상 90도의 절반)
        // - Boom 길이: 60cm (머즐 가까이)
        // - Socket Offset: 중앙 정렬 (오른쪽 어깨에서 정중앙으로)
        
        CameraBoom->TargetArmLength = 60.0f;
        CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 60.0f);  // 높이만 유지
        FollowCamera->SetFieldOfView(45.0f);

        UE_LOG(LogBGCharacter, Warning, TEXT("📏 2x Scope Camera Applied"));
    }
    else if (FMath::IsNearlyEqual(Magnification, 4.0f))
    {
        // 🔍 4배 스코프
        // - FOV: 22.5도 (정상 90도의 1/4)
        // - Boom 길이: 40cm (머즐 매우 가까이)
        // - Socket Offset: 정중앙
        
        CameraBoom->TargetArmLength = 40.0f;
        CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 60.0f);
        FollowCamera->SetFieldOfView(22.5f);

        UE_LOG(LogBGCharacter, Warning, TEXT("📏 4x Scope Camera Applied"));
    }
    else
    {
        // 기타 배율 (기본값 설정)
        CameraBoom->TargetArmLength = 60.0f;
        CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 60.0f);
        FollowCamera->SetFieldOfView(45.0f);

        UE_LOG(LogBGCharacter, Warning,
            TEXT("📏 Unknown Scope Magnification: %.1f - Default settings applied"),
            Magnification);
    }
}

void ABG_Character::RestoreCameraFromScope()
{
    // ✅ [핵심] 기본 카메라로 복원
    // 스코프 모드에서 나올 때 호출
    
    if (!CameraBoom || !FollowCamera)
    {
        UE_LOG(LogBGCharacter, Error,
            TEXT("%s: RestoreCameraFromScope failed - Camera components null"),
            *GetNameSafe(this));
        return;
    }

    // 🔄 저장된 카메라 설정 복원
    CameraBoom->TargetArmLength = SavedCameraBoomLength;
    CameraBoom->SocketOffset = SavedCameraBoomOffset;
    FollowCamera->SetFieldOfView(SavedCameraFOV);

    UE_LOG(LogBGCharacter, Warning, TEXT("🔄 Camera Restored to Default"));
}

// ============================================================================
// [GetLifetimeReplicatedProps 수정]
// 기존 함수 내 마지막에 아래 라인 추가
// ============================================================================

void ABG_Character::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // ... 기존 DOREPLIFETIME 들 ...

    // 🔍 스코프 상태 복제
    DOREPLIFETIME(ABG_Character, bIsScoping);
}
```

**검증**:
- [ ] 컴파일 성공 (Visual Studio 또는 Rider)
- [ ] Play 모드에서 스코프 활성화 함수 호출 가능
- [ ] 스코프 활성화 시 카메라 설정 변경 확인

---

### **STEP 4: BG_Scope 카메라 설정**

**목표**: SceneCapture2D 카메라를 화면 정중앙 기준으로 설정

**작업 파일**:
- `GEONU/Combat/BG_Scope.cpp` (수정)

**구현 코드**:

```cpp
// BG_Scope.cpp - SetScopeActive() 함수 개선

void ABG_Scope::SetScopeActive(bool bNewActive)
{
    bIsScopeActive = bNewActive;

    if (!SceneCaptureComponent)
    {
        UE_LOG(LogBGScope, Error,
            TEXT("%s: SetScopeActive failed - SceneCaptureComponent null"),
            *GetNameSafe(this));
        return;
    }

    if (bNewActive)
    {
        // ✅ [핵심] 스코프 활성화 - 카메라 설정
        
        SceneCaptureComponent->SetActive(true);
        SceneCaptureComponent->bCaptureEveryFrame = true;

        // 📷 스코프 카메라를 오너(캐릭터)의 카메라와 동기화
        // 이를 통해 항상 화면 정중앙 기준 촬영
        if (GetOwner())
        {
            if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
            {
                FVector ViewLocation;
                FRotator ViewRotation;
                OwnerPawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);

                // 스코프 카메라를 캐릭터 카메라 위치로 설정
                SceneCaptureComponent->SetWorldLocation(ViewLocation);
                SceneCaptureComponent->SetWorldRotation(ViewRotation);

                UE_LOG(LogBGScope, Warning,
                    TEXT("🎥 Scope Camera Synced - Pos: %s, Rot: %s"),
                    *ViewLocation.ToString(), *ViewRotation.ToString());
            }
        }
    }
    else
    {
        // 🔄 스코프 비활성화
        SceneCaptureComponent->SetActive(false);
        SceneCaptureComponent->bCaptureEveryFrame = false;

        UE_LOG(LogBGScope, Warning, TEXT("🔄 Scope Deactivated"));
    }
}

// 📅 Tick에서 연속 동기화 (중요!)
void ABG_Scope::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ✅ [핵심] 활성화 중인 스코프는 매 프레임 카메라 동기화
    if (bIsScopeActive && SceneCaptureComponent)
    {
        if (GetOwner())
        {
            if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
            {
                FVector ViewLocation;
                FRotator ViewRotation;
                OwnerPawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);

                // 연속 동기화로 카메라 회전/이동 반영
                SceneCaptureComponent->SetWorldLocation(ViewLocation);
                SceneCaptureComponent->SetWorldRotation(ViewRotation);
            }
        }
    }
}
```

**BG_Scope.h 수정** (Tick 활성화):

```cpp
// BG_Scope.h - Constructor 수정

ABG_Scope::ABG_Scope()
{
    PrimaryActorTick.bCanEverTick = true;      // ✅ Tick 활성화
    PrimaryActorTick.TickInterval = 0.0f;      // 매 프레임 실행

    // ... 기존 코드 ...
}
```

**검증**:
- [ ] 스코프 활성화 시 SceneCapture RenderTarget에 카메라 뷰 반영
- [ ] 카메라 회전 시 스코프 뷰도 동시에 회전

---

### **STEP 5: WBP_ScopeUI 컨트롤러 생성**

**목표**: C++에서 WBP_ScopeUI Widget 제어

**작업 파일**: (신규 생성)
- `WON/Public/UI/BG_ScopeUIController.h`
- `WON/Private/UI/BG_ScopeUIController.cpp`

**헤더 파일 생성** (`BG_ScopeUIController.h`):

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BG_ScopeUIController.generated.h"

class UTextureRenderTarget2D;
class UImage;
class UCanvasPanel;
class UBorder;

/// 스코프 UI 위젯 컨트롤러
/// 역할:
/// - RenderTarget 텍스처 표시
/// - 2배/4배 크로스헤어 가시성 토글
/// - 스코프 UI 생명주기 관리
UCLASS()
class PUBG_HOTMODE_API UBG_ScopeUIController : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ✅ [핵심] 스코프 배율 설정 및 UI 업데이트
    /// @param Magnification: 2.0 (2배) 또는 4.0 (4배)
    UFUNCTION(BlueprintCallable, Category = "Scope UI")
    void SetScopeMagnification(float Magnification);

    // ✅ [핵심] RenderTarget 텍스처 설정
    /// @param NewRenderTarget: SceneCapture2D의 RenderTarget
    UFUNCTION(BlueprintCallable, Category = "Scope UI")
    void SetScopeRenderTarget(UTextureRenderTarget2D* NewRenderTarget);

    // ✅ [핵심] 스코프 UI 표시/숨김
    UFUNCTION(BlueprintCallable, Category = "Scope UI")
    void ShowScopeUI();

    UFUNCTION(BlueprintCallable, Category = "Scope UI")
    void HideScopeUI();

    UFUNCTION(BlueprintPure, Category = "Scope UI")
    bool IsScopeVisible() const { return bIsScopeVisible; }

protected:
    // 🔍 Widget 참조
    UPROPERTY(BlueprintReadWrite, Category = "Scope UI", meta = (BindWidget))
    TObjectPtr<UImage> ScopeRenderTargetImage;

    // 2배 크로스헤어 패널 (가시성 토글)
    UPROPERTY(BlueprintReadWrite, Category = "Scope UI", meta = (BindWidgetOptional))
    TObjectPtr<UCanvasPanel> Crosshair2xPanel;

    // 4배 크로스헤어 패널 (가시성 토글)
    UPROPERTY(BlueprintReadWrite, Category = "Scope UI", meta = (BindWidgetOptional))
    TObjectPtr<UCanvasPanel> Crosshair4xPanel;

    // 스코프 마스크/테두리 (렌즈 모양)
    UPROPERTY(BlueprintReadWrite, Category = "Scope UI", meta = (BindWidgetOptional))
    TObjectPtr<UBorder> ScopeMaskBorder;

private:
    // 🔍 상태
    UPROPERTY(VisibleAnywhere, Category = "Scope UI", meta = (AllowPrivateAccess = "true"))
    float CurrentMagnification = 2.0f;

    UPROPERTY(VisibleAnywhere, Category = "Scope UI", meta = (AllowPrivateAccess = "true"))
    bool bIsScopeVisible = false;

    // 🔍 보조 함수
    void UpdateCrosshairVisibility();
};
```

**구현 파일 생성** (`BG_ScopeUIController.cpp`):

```cpp
#include "UI/BG_ScopeUIController.h"

#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/Border.h"
#include "Engine/TextureRenderTarget2D.h"

UBG_ScopeUIController::UBG_ScopeUIController()
{
    // 기본 설정
    CurrentMagnification = 2.0f;
    bIsScopeVisible = false;
}

void UBG_ScopeUIController::NativeConstruct()
{
    Super::NativeConstruct();

    // ✅ 초기화 시 스코프 UI 숨김
    HideScopeUI();
}

void UBG_ScopeUIController::NativeDestruct()
{
    Super::NativeDestruct();
    
    // 정리
    ScopeRenderTargetImage = nullptr;
    Crosshair2xPanel = nullptr;
    Crosshair4xPanel = nullptr;
}

void UBG_ScopeUIController::SetScopeMagnification(float Magnification)
{
    // ✅ [핵심] 배율 설정 → 크로스헤어 시각성 변경
    
    CurrentMagnification = Magnification;
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow,
            FString::Printf(TEXT("Scope Magnification Set: %.1fx"), Magnification));
    }

    UpdateCrosshairVisibility();
}

void UBG_ScopeUIController::SetScopeRenderTarget(UTextureRenderTarget2D* NewRenderTarget)
{
    // ✅ [핵심] RenderTarget 이미지에 표시
    
    if (!ScopeRenderTargetImage)
    {
        return;
    }

    if (NewRenderTarget)
    {
        ScopeRenderTargetImage->SetBrushFromTexture(NewRenderTarget, true);
        
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-2, 2.f, FColor::Cyan,
                FString::Printf(TEXT("RenderTarget Set: %s"), 
                    *GetNameSafe(NewRenderTarget)));
        }
    }
}

void UBG_ScopeUIController::ShowScopeUI()
{
    // ✅ [핵심] 스코프 UI 표시
    
    bIsScopeVisible = true;

    // 모든 스코프 UI 컴포넌트 표시
    if (ScopeRenderTargetImage)
    {
        ScopeRenderTargetImage->SetVisibility(ESlateVisibility::Visible);
    }
    if (ScopeMaskBorder)
    {
        ScopeMaskBorder->SetVisibility(ESlateVisibility::Visible);
    }

    UpdateCrosshairVisibility();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-3, 2.f, FColor::Green,
            TEXT("🎯 Scope UI Shown"));
    }
}

void UBG_ScopeUIController::HideScopeUI()
{
    // ✅ [핵심] 스코프 UI 숨김
    
    bIsScopeVisible = false;

    // 모든 스코프 UI 컴포넌트 숨김
    if (ScopeRenderTargetImage)
    {
        ScopeRenderTargetImage->SetVisibility(ESlateVisibility::Hidden);
    }
    if (ScopeMaskBorder)
    {
        ScopeMaskBorder->SetVisibility(ESlateVisibility::Hidden);
    }
    if (Crosshair2xPanel)
    {
        Crosshair2xPanel->SetVisibility(ESlateVisibility::Hidden);
    }
    if (Crosshair4xPanel)
    {
        Crosshair4xPanel->SetVisibility(ESlateVisibility::Hidden);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-3, 2.f, FColor::Red,
            TEXT("🔄 Scope UI Hidden"));
    }
}

void UBG_ScopeUIController::UpdateCrosshairVisibility()
{
    // ✅ [핵심] 배율별 크로스헤어 토글
    
    if (!bIsScopeVisible)
    {
        return;  // 스코프 UI 자체가 숨겨진 상태
    }

    if (FMath::IsNearlyEqual(CurrentMagnification, 2.0f))
    {
        // 2배: 초록색 십자 크로스헤어 표시
        if (Crosshair2xPanel)
        {
            Crosshair2xPanel->SetVisibility(ESlateVisibility::Visible);
        }
        if (Crosshair4xPanel)
        {
            Crosshair4xPanel->SetVisibility(ESlateVisibility::Hidden);
        }

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-4, 1.f, FColor::Green,
                TEXT("✅ 2x Crosshair Visible"));
        }
    }
    else if (FMath::IsNearlyEqual(CurrentMagnification, 4.0f))
    {
        // 4배: 빨강색 동심원 크로스헤어 표시
        if (Crosshair2xPanel)
        {
            Crosshair2xPanel->SetVisibility(ESlateVisibility::Hidden);
        }
        if (Crosshair4xPanel)
        {
            Crosshair4xPanel->SetVisibility(ESlateVisibility::Visible);
        }

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-4, 1.f, FColor::Red,
                TEXT("✅ 4x Crosshair Visible"));
        }
    }
}
```

**검증**:
- [ ] 컴파일 성공
- [ ] WBP_ScopeUI Blueprint에서 자동으로 인식
- [ ] 스코프 활성화 시 ShowScopeUI() 호출 가능

---

### **STEP 6: BG_Character에서 WBP_ScopeUI 연결**

**목표**: 스코프 활성화 시 UI 표시, 비활성화 시 숨김

**작업 파일**:
- `WON/Public/Player/BG_Character.h` (추가)
- `WON/Private/Player/BG_Character.cpp` (수정)

**헤더 파일 추가** (`BG_Character.h`):

```cpp
// ============================================================================
// [추가 위치: private 섹션의 UI 관련 변수들 아래]
// ============================================================================

private:
    // 🔍 스코프 UI 위젯
    UPROPERTY(VisibleInstanceOnly, Category = "UI|Scope", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UBG_ScopeUIController> ScopeUIController;

    /// 스코프 UI Widget 클래스 (에디터에서 설정)
    UPROPERTY(EditAnywhere, Category = "UI|Scope", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<class UBG_ScopeUIController> ScopeUIWidgetClass;
```

**구현 파일 수정** (`BG_Character.cpp`):

```cpp
// ============================================================================
// [수정 위치: BeginPlay() - 기존 UI 초기화 코드 이후]
// ============================================================================

void ABG_Character::BeginPlay()
{
    Super::BeginPlay();

    // ... 기존 코드 ...

    // 🔍 스코프 UI 초기화
    if (IsLocallyControlled() && ScopeUIWidgetClass)
    {
        APlayerController* PC = Cast<APlayerController>(Controller);
        if (PC)
        {
            ScopeUIController = CreateWidget<UBG_ScopeUIController>(
                PC,
                ScopeUIWidgetClass);

            if (ScopeUIController)
            {
                ScopeUIController->AddToViewport(100);  // 높은 우선순위
                ScopeUIController->HideScopeUI();        // 초기 상태: 숨김

                UE_LOG(LogBGCharacter, Warning,
                    TEXT("🔍 Scope UI Controller Created"));
            }
        }
    }
}

// ============================================================================
// [수정 위치: Server_SetScopingState_Implementation() - 스코프 활성화 부분]
// ============================================================================

void ABG_Character::Server_SetScopingState_Implementation(bool bNewScoping)
{
    // ... 기존 검증 코드 ...

    bIsScoping = bNewScoping;

    if (bNewScoping)
    {
        // 🎯 스코프 모드 진입
        CharacterState = EBGCharacterState::Scoped;
        float Magnification = EquippedWeapon->GetScopeMagnification();
        ApplyScopeCamera(Magnification);
        
        // 스코프 액터 활성화
        if (ABG_Scope* ScopeActor = EquippedWeapon->GetScopeActor())
        {
            ScopeActor->SetScopeActive(true);

            // ✅ [핵심] UI 업데이트
            if (IsLocallyControlled() && ScopeUIController)
            {
                // RenderTarget 설정
                ScopeUIController->SetScopeRenderTarget(
                    ScopeActor->GetScopeRenderTarget());

                // 배율 설정
                ScopeUIController->SetScopeMagnification(Magnification);

                // UI 표시
                ScopeUIController->ShowScopeUI();

                UE_LOG(LogBGCharacter, Warning,
                    TEXT("🎯 Scope UI Updated & Shown"));
            }
        }
    }
    else
    {
        // 🔄 스코프 모드 종료
        CharacterState = EBGCharacterState::Idle;
        RestoreCameraFromScope();

        // 스코프 액터 비활성화
        if (ABG_EquippedWeaponBase* Weapon = EquipmentComponent->GetActiveEquippedWeaponActor())
        {
            if (ABG_Scope* ScopeActor = Weapon->GetScopeActor())
            {
                ScopeActor->SetScopeActive(false);
            }
        }

        // ✅ [핵심] UI 숨김
        if (IsLocallyControlled() && ScopeUIController)
        {
            ScopeUIController->HideScopeUI();

            UE_LOG(LogBGCharacter, Warning,
                TEXT("🔄 Scope UI Hidden"));
        }
    }
}
```

**검증**:
- [ ] 스코프 활성화 시 UI 표시
- [ ] 스코프 비활성화 시 UI 숨김
- [ ] RenderTarget이 UI에 렌더링됨

---

### **STEP 7: 2배/4배 크로스헤어 블루프린트 구현**

**목표**: 스코프별 크로스헤어 시각 구현

**작업 위치**: 에디터 (Unreal Editor)

#### **2배 스코프 크로스헤어** (`BP_Crosshair_2x`)

**생성 단계**:
1. Content Browser → `Content/WON/Scope/` → 우측마우스
2. → "Widget Blueprint" 선택
3. 이름: `BP_Crosshair_2x`
4. 부모: `UserWidget`

**위젯 구조**:
```
Canvas Panel (루트)
├── Top Line
│   └── Vertical Box (높이 8px, 폭 1px, 위치 정중앙 상단)
├── Bottom Line
│   └── Vertical Box (높이 8px, 폭 1px, 위치 정중앙 하단)
├── Left Line
│   └── Horizontal Box (높이 1px, 폭 8px, 위치 정중앙 좌측)
└── Right Line
    └── Horizontal Box (높이 1px, 폭 8px, 위치 정중앙 우측)
```

**Line 색상 설정**:
- 색상: 초록색 (RGB: 0, 255, 0)
- Opacity: 0.8

**위치 설정** (Canvas Panel 좌표):
- Top Line: Position X=0.5, Y=0.35 (앵커: 중앙 상단)
- Bottom Line: Position X=0.5, Y=0.65 (앵커: 중앙 하단)
- Left Line: Position X=0.35, Y=0.5 (앵커: 좌측 중앙)
- Right Line: Position X=0.65, Y=0.5 (앵커: 우측 중앙)

#### **4배 스코프 크로스헤어** (`BP_Crosshair_4x`)

**생성 단계**:
1. Content Browser → `Content/WON/Scope/` → 우측마우스
2. → "Widget Blueprint" 선택
3. 이름: `BP_Crosshair_4x`
4. 부모: `UserWidget`

**위젯 구조**:
```
Canvas Panel (루트)
├── Center Dot
│   └── Image (1x1px, 흰색, 중앙)
├── Reticle_Ring_Outer
│   └── Border (지름 30px, 빨강색 테두리)
├── Reticle_Ring_Mid
│   └── Border (지름 20px, 빨강색 테두리)
├── Reticle_Ring_Inner
│   └── Border (지름 10px, 빨강색 테두리)
├── Distance_Top
│   └── Vertical Box (높이 6px, 폭 1px, 상단 거리 마커)
├── Distance_Bottom
│   └── Vertical Box (높이 6px, 폭 1px, 하단 거리 마커)
├── Distance_Left
│   └── Horizontal Box (높이 1px, 폭 6px, 좌측 거리 마커)
└── Distance_Right
    └── Horizontal Box (높이 1px, 폭 6px, 우측 거리 마커)
```

**색상 설정**:
- Reticle Rings: 빨강색 (RGB: 255, 0, 0)
- Distance Markers: 빨강색
- Opacity: 0.9

**위치 설정** (정중앙 기준):
- Center Dot: X=0.5, Y=0.5 (크기: 1x1px)
- Rings: 모두 X=0.5, Y=0.5 (중앙 중심, 반지름만 다름)
- Distance Markers: 정중앙에서 약 50px 떨어진 위치

**검증**:
- [ ] BP_Crosshair_2x 생성 완료
- [ ] BP_Crosshair_4x 생성 완료
- [ ] WBP_ScopeUI에 2개 패널 추가

---

### **STEP 8: WBP_ScopeUI 최종 구조**

**목표**: 스코프 UI 위젯 완성

**작업 위치**: 에디터 (Unreal Editor)

**WBP_ScopeUI 위젯 계층**:
```
Canvas Panel (루트)
├── ScopeRenderTarget_Image
│   └── Image (RenderTarget 표시)
│       위치: 화면 중앙
│       크기: 전체 화면
│       마진: 20px (테두리 용 공간)
│       색상: 흰색 (기본)
├── ScopeMask_Border
│   └── Border (스코프 테두리/렌즈 모양)
│       배경색: 검정색
│       테두리 반지름: 400px (원형)
│       테두리 색상: 검정색 (투명도 높음)
├── Crosshair2xPanel
│   └── [BP_Crosshair_2x 포함시키기]
│       가시성: 초기 Hidden
│       위치: 화면 정중앙
└── Crosshair4xPanel
    └── [BP_Crosshair_4x 포함시키기]
        가시성: 초기 Hidden
        위치: 화면 정중앙
```

**WBP_ScopeUI 바인딩** (C++ 연결):

1. Class Details → Search "ScopeRenderTargetImage"
2. → 찾아서 "Bind Widget" 활성화
3. 동일하게 Crosshair2xPanel, Crosshair4xPanel, ScopeMaskBorder도 설정

**디버그 정보 표시** (Canvas 추가):

```
Canvas Panel (디버그)
└── DebugText_Canvas
    └── TextBlock
        텍스트: "Scope: 2x | Ammo: 30/120"
        위치: 좌측 하단
        폰트 크기: 20
        색상: 초록색
```

---

## 📊 최종 통합 테스트 체크리스트

### 비스코프 모드
- [ ] 화면 정중앙에서 라인트레이스 수행
- [ ] 머즐 소켓에서 정확히 발사
- [ ] Debug Draw 확인 (파란점→초록점→빨간선)
- [ ] 총알이 정중앙 조준점으로 정확히 날아감

### 스코프 모드 (2배)
- [ ] E 키 누르면 스코프 활성화
- [ ] 카메라 FOV 45도로 변경
- [ ] WBP_ScopeUI 표시
- [ ] 초록색 십자 크로스헤어 표시
- [ ] 머즐 소켓에서 정확히 발사
- [ ] 총알이 정중앙 조준점으로 정확히 날아감

### 스코프 모드 (4배)
- [ ] 스코프 배율 변경 버튼으로 4배로 변경
- [ ] 카메라 FOV 22.5도로 변경
- [ ] WBP_ScopeUI 유지
- [ ] 빨강색 동심원 크로스헤어로 변경
- [ ] 머즐 소켓에서 정확히 발사
- [ ] 총알이 정중앙 조준점으로 정확히 날아감

### 스코프 해제
- [ ] E 키 누르면 스코프 비활성화
- [ ] 카메라 FOV 90도로 복원
- [ ] WBP_ScopeUI 숨김
- [ ] 비스코프 모드로 복귀

---

## 🐛 주요 문제 해결 가이드

### 문제 1: 총알이 바닥에서 발사됨
**원인**: Muzzle 소켓 미설정 또는 위치 오류
**해결**:
1. STEP 2 재확인 (무기 메시의 Muzzle 소켓)
2. 소켓 위치를 총구 끝으로 정확히 조정
3. Play 모드에서 Debug Draw로 빨간점(머즐) 위치 확인

### 문제 2: 스코프 UI가 표시되지 않음
**원인 1**: ScopeUIWidgetClass가 에디터에서 설정되지 않음
**해결**:
- 에디터 → Viewport → BP_Character 선택
- Details → Scope UI Widget Class → WBP_ScopeUI 드래그

**원인 2**: IsLocallyControlled() 실패 (멀티플레이)
**해결**:
- Server_SetScopingState에서 Client_ShowScopeUI() 호출로 변경

### 문제 3: 스코프 카메라가 회전하지 않음
**원인**: BG_Scope의 Tick이 비활성화됨
**해결**:
- BG_Scope.h에서 PrimaryActorTick.bCanEverTick = true 설정 확인

---

## 📚 참고 자료

| 항목 | 파일 경로 |
|------|---------|
| 발사 로직 | `WON/Private/Combat/BG_WeaponFireComponent.cpp` |
| 캐릭터 상태 | `WON/Public/Player/BG_Character.h` |
| 무기 기본 | `GEONU/Combat/BG_EquippedWeaponBase.h` |
| 스코프 액터 | `GEONU/Combat/BG_Scope.h` |
| 스코프 UI | `WON/Public/UI/BG_ScopeUIController.h` |

---

**문의사항이 있으면 언제든 물어보세요!** 🎯

