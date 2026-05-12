#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CompassWidget.generated.h"

class UCompassComponent;

UCLASS()
class PUBG_HOTMODE_API UCompassWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled
    ) const override;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    float VisibleAngle = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    float MinorTickStep = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    float MajorTickStep = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    float DirectionStep = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    float MinorTickHeight = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    float MajorTickHeight = 18.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    float TickThickness = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    float TickBaseY = 28.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    float LabelY = 34.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    float HeadingTextY = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    FLinearColor MinorTickColor = FLinearColor(1.f, 1.f, 1.f, 0.65f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    FLinearColor MajorTickColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    FLinearColor CardinalColor = FLinearColor(1.0f, 0.85f, 0.1f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    FLinearColor IntercardinalColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    FLinearColor DegreeTextColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    FLinearColor HeadingTextColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    FLinearColor MarkerColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    int32 FontSizeDegrees = 16;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    int32 FontSizeDirections = 22;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    int32 FontSizeHeading = 30;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    float MarkerWidth = 14.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    float MarkerHeight = 16.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    float MarkerTopOffset = 0.0f;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Compass")
    TObjectPtr<UCompassComponent> CompassComponent;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Compass")
    float CachedYaw = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
    float HeadingMaskHalfWidth = 110.0f;

protected:
    UCompassComponent* ResolveCompassComponent();
    static float Normalize360(float Degree);
    static FString GetDirectionLabel(float Degree);
    static bool IsCardinalDirection(float Degree);
    static bool IsNearMultiple(float Value, float Step, float Tolerance = 0.1f);

    void DrawLine(
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FGeometry& AllottedGeometry,
        const FVector2D& Start,
        const FVector2D& End,
        const FLinearColor& Color,
        float Thickness
    ) const;

    void DrawTextLabel(
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FGeometry& AllottedGeometry,
        const FString& Text,
        const FVector2D& Position,
        const FLinearColor& Color,
        int32 FontSize,
        const TCHAR* FontStyle
    ) const;

    void DrawTriangleMarker(
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FGeometry& AllottedGeometry,
        const FVector2D& TopCenter,
        float Width,
        float Height,
        const FLinearColor& Color,
        float Thickness
    ) const;
};