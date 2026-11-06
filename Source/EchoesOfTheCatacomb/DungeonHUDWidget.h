// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class AEchoesOfTheCatacombCharacter;
class ABossMinotaur;

class SDungeonHUDWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDungeonHUDWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AEchoesOfTheCatacombCharacter>, Character)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	                      const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	                      int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TWeakObjectPtr<AEchoesOfTheCatacombCharacter> CharacterOwner;
	TWeakObjectPtr<ABossMinotaur> CachedBoss;

	// Smooth ghost health trailing animation (Souls-like)
	mutable float PlayerGhostPercent = 1.0f;
	mutable float BossGhostPercent = 1.0f;
	mutable double LastPaintTime = 0.0;

	void DrawBar(FSlateWindowElementList& OutDrawElements, int32& InOutLayerId,
	             const FGeometry& AllottedGeometry,
	             float X, float Y, float Width, float Height,
	             float FillPercent, float GhostPercent,
	             const FLinearColor& FillColor, const FLinearColor& GhostColor,
	             const FLinearColor& BorderColor) const;
};

