#include "CompassWidget.h"
#include "Components/CompassComponent.h"
#include "GameFramework/Pawn.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UCompassWidget::NativeConstruct()
{
    Super::NativeConstruct();
    CompassComponent = ResolveCompassComponent();
}

void UCompassWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!CompassComponent)
    {
        CompassComponent = ResolveCompassComponent();
    }

    if (CompassComponent)
    {
        const float RawYaw = CompassComponent->GetCompassYaw();
        CachedYaw = Normalize360(FMath::RoundToFloat(RawYaw / 5.0f) * 5.0f);
    }
}

int32 UCompassWidget::NativePaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled
) const
{
    const int32 BaseLayer = Super::NativePaint(
        Args,
        AllottedGeometry,
        MyCullingRect,
        OutDrawElements,
        LayerId,
        InWidgetStyle,
        bParentEnabled
    );

    const FVector2D Size = AllottedGeometry.GetLocalSize();
    if (Size.X <= KINDA_SMALL_NUMBER || Size.Y <= KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Error, TEXT("UCompassWidget::NativePaint - Invalid geometry size."));
        return BaseLayer;
    }

    if (VisibleAngle <= KINDA_SMALL_NUMBER || MinorTickStep <= KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Error, TEXT("UCompassWidget::NativePaint - VisibleAngle or MinorTickStep is invalid."));
        return BaseLayer;
    }

    const float CenterX = Size.X * 0.5f;
    const float PixelsPerDegree = Size.X / VisibleAngle;
    const float HalfVisibleAngle = VisibleAngle * 0.5f;
    const float StartRelative = -HalfVisibleAngle;
    const float EndRelative = HalfVisibleAngle;

    const float StartWorld = CachedYaw + StartRelative;
    const float EndWorld = CachedYaw + EndRelative;
    const float FirstTickWorld = FMath::FloorToFloat(StartWorld / MinorTickStep) * MinorTickStep;

    int32 DrawLayer = BaseLayer + 1;

    for (float TickWorld = FirstTickWorld; TickWorld <= EndWorld + MinorTickStep; TickWorld += MinorTickStep)
    {
        const float Relative = FMath::FindDeltaAngleDegrees(CachedYaw, TickWorld);
        if (Relative < StartRelative - KINDA_SMALL_NUMBER || Relative > EndRelative + KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const float X = CenterX + Relative * PixelsPerDegree;
        if (X < -50.0f || X > Size.X + 50.0f)
        {
            continue;
        }

        const float NormalizedWorld = Normalize360(TickWorld);
        const bool bIsMajor = IsNearMultiple(NormalizedWorld, MajorTickStep, 0.15f);
        const bool bIsDirection = IsNearMultiple(NormalizedWorld, DirectionStep, 0.15f);

        const float TickHeight = bIsMajor ? MajorTickHeight : MinorTickHeight;
        const FLinearColor TickColor = bIsMajor ? MajorTickColor : MinorTickColor;

        if (bIsDirection)
        {
            DrawLine(
                OutDrawElements,
                DrawLayer,
                AllottedGeometry,
                FVector2D(X, TickBaseY),
                FVector2D(X, TickBaseY + TickHeight),
                TickColor,
                TickThickness
            );

            const FString DirLabel = GetDirectionLabel(NormalizedWorld);
            const bool bIsCardinal = IsCardinalDirection(NormalizedWorld);
            const FLinearColor DirColor = bIsCardinal ? CardinalColor : IntercardinalColor;
            const int32 FontSize = bIsCardinal ? FontSizeDirections : FMath::Max(FontSizeDirections - 2, 10);

            if (FMath::Abs(X - CenterX) < HeadingMaskHalfWidth)
            {
                continue;
            }
            
            DrawTextLabel(
                OutDrawElements,
                DrawLayer + 1,
                AllottedGeometry,
                DirLabel,
                FVector2D(X, LabelY),
                DirColor,
                FontSize,
                TEXT("Bold")
            );
        }
        else if (bIsMajor)
        {
            DrawLine(
                OutDrawElements,
                DrawLayer,
                AllottedGeometry,
                FVector2D(X, TickBaseY),
                FVector2D(X, TickBaseY + TickHeight),
                TickColor,
                TickThickness
            );
            
            if (FMath::Abs(X - CenterX) < HeadingMaskHalfWidth)
            {
                continue;
            }
            
            const int32 RoundedDegree = FMath::RoundToInt(NormalizedWorld);
            DrawTextLabel(
                OutDrawElements,
                DrawLayer + 1,
                AllottedGeometry,
                FString::FromInt(RoundedDegree),
                FVector2D(X, LabelY),
                DegreeTextColor,
                FontSizeDegrees,
                TEXT("Regular")
            );
        }
    }

    const int32 CurrentHeading = FMath::RoundToInt(Normalize360(CachedYaw));
    const FString HeadingText = FString::FromInt(CurrentHeading);

    DrawTextLabel(
        OutDrawElements,
        DrawLayer + 3,
        AllottedGeometry,
        HeadingText,
        FVector2D(CenterX, HeadingTextY),
        HeadingTextColor,
        FontSizeHeading,
        TEXT("Bold")
    );

    DrawTriangleMarker(
        OutDrawElements,
        DrawLayer + 3,
        AllottedGeometry,
        FVector2D(CenterX, MarkerTopOffset),
        MarkerWidth,
        MarkerHeight,
        MarkerColor,
        2.0f
    );

    return DrawLayer + 5;
}

UCompassComponent* UCompassWidget::ResolveCompassComponent()
{
    APawn* OwningPawn = GetOwningPlayerPawn();
    if (!OwningPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("UCompassWidget::ResolveCompassComponent - OwningPlayerPawn is null."));
        return nullptr;
    }

    UCompassComponent* FoundComponent = OwningPawn->FindComponentByClass<UCompassComponent>();
    if (!FoundComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("UCompassWidget::ResolveCompassComponent - CompassComponent missing on %s."), *GetNameSafe(OwningPawn));
        return nullptr;
    }

    return FoundComponent;
}

float UCompassWidget::Normalize360(float Degree)
{
    float Result = FMath::Fmod(Degree, 360.0f);
    if (Result < 0.0f)
    {
        Result += 360.0f;
    }
    return Result;
}

FString UCompassWidget::GetDirectionLabel(float Degree)
{
    const float Normalized = Normalize360(Degree);

    if (Normalized >= 337.5f || Normalized < 22.5f) return TEXT("N");
    if (Normalized < 67.5f) return TEXT("NE");
    if (Normalized < 112.5f) return TEXT("E");
    if (Normalized < 157.5f) return TEXT("SE");
    if (Normalized < 202.5f) return TEXT("S");
    if (Normalized < 247.5f) return TEXT("SW");
    if (Normalized < 292.5f) return TEXT("W");
    return TEXT("NW");
}

bool UCompassWidget::IsCardinalDirection(float Degree)
{
    const float Normalized = Normalize360(Degree);
    return
        IsNearMultiple(Normalized, 90.0f, 0.15f) ||
        IsNearMultiple(Normalized + 360.0f, 90.0f, 0.15f);
}

bool UCompassWidget::IsNearMultiple(float Value, float Step, float Tolerance)
{
    if (Step <= KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Error, TEXT("UCompassWidget::IsNearMultiple - Step must be greater than zero."));
        return false;
    }

    const float Remainder = FMath::Fmod(Value, Step);
    return
        FMath::IsNearlyZero(Remainder, Tolerance) ||
        FMath::IsNearlyEqual(Remainder, Step, Tolerance);
}

void UCompassWidget::DrawLine(
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FGeometry& AllottedGeometry,
    const FVector2D& Start,
    const FVector2D& End,
    const FLinearColor& Color,
    float Thickness
) const
{
    TArray<FVector2f> Points;
    Points.Reserve(2);
    Points.Add(FVector2f((float)Start.X, (float)Start.Y));
    Points.Add(FVector2f((float)End.X, (float)End.Y));

    FSlateDrawElement::MakeLines(
        OutDrawElements,
        LayerId,
        AllottedGeometry.ToPaintGeometry(),
        MoveTemp(Points),
        ESlateDrawEffect::None,
        Color,
        true,
        Thickness
    );
}

void UCompassWidget::DrawTextLabel(
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FGeometry& AllottedGeometry,
    const FString& Text,
    const FVector2D& Position,
    const FLinearColor& Color,
    int32 FontSize,
    const TCHAR* FontStyle
) const
{
    if (Text.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("UCompassWidget::DrawTextLabel - Text is empty."));
        return;
    }

    const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle(FontStyle, FontSize);

    // Slate font measure 의존성을 피하기 위한 단순 폭 추정
    const float EstimatedCharWidth = static_cast<float>(FontSize) * 0.6f;
    const float EstimatedWidth = Text.Len() * EstimatedCharWidth;
    const float EstimatedHeight = static_cast<float>(FontSize);

    const FVector2D DrawPos(Position.X - EstimatedWidth * 0.5f - 3.0f, Position.Y);
    const FVector2D DrawSize(
        FMath::Max(EstimatedWidth, 1.0f),
        FMath::Max(EstimatedHeight, 1.0f)
    );

    FSlateDrawElement::MakeText(
        OutDrawElements,
        LayerId,
        AllottedGeometry.ToPaintGeometry(DrawPos, DrawSize),
        Text,
        FontInfo,
        ESlateDrawEffect::None,
        Color
    );
}

void UCompassWidget::DrawTriangleMarker(
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FGeometry& AllottedGeometry,
    const FVector2D& TopCenter,
    float Width,
    float Height,
    const FLinearColor& Color,
    float Thickness
) const
{
    if (Width <= KINDA_SMALL_NUMBER || Height <= KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Error, TEXT("UCompassWidget::DrawTriangleMarker - Width/Height must be > 0."));
        return;
    }

    TArray<FVector2f> Points;
    Points.Reserve(4);
    // 위쪽 밑변
    Points.Add(FVector2f((float)(TopCenter.X - Width * 0.5f), (float)TopCenter.Y));
    Points.Add(FVector2f((float)(TopCenter.X + Width * 0.5f), (float)TopCenter.Y));

    // 아래 꼭짓점
    Points.Add(FVector2f((float)TopCenter.X, (float)(TopCenter.Y + Height)));

    // 닫기
    Points.Add(FVector2f((float)(TopCenter.X - Width * 0.5f), (float)TopCenter.Y));

    FSlateDrawElement::MakeLines(
        OutDrawElements,
        LayerId,
        AllottedGeometry.ToPaintGeometry(),
        MoveTemp(Points),
        ESlateDrawEffect::None,
        Color,
        true,
        Thickness
    );
}