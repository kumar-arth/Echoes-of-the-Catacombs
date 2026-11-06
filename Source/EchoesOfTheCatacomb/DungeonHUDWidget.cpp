// Copyright Epic Games, Inc. All Rights Reserved.

#include "DungeonHUDWidget.h"
#include "EchoesOfTheCatacombCharacter.h"
#include "BossMinotaur.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void SDungeonHUDWidget::Construct(const FArguments& InArgs)
{
	CharacterOwner = InArgs._Character;
	SetCanTick(true);
}

void SDungeonHUDWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Cache boss reference if not already found
	if (!CachedBoss.IsValid() && CharacterOwner.IsValid())
	{
		if (UWorld* World = CharacterOwner->GetWorld())
		{
			TArray<AActor*> FoundBosses;
			UGameplayStatics::GetAllActorsOfClass(World, ABossMinotaur::StaticClass(), FoundBosses);
			if (FoundBosses.Num() > 0)
			{
				CachedBoss = Cast<ABossMinotaur>(FoundBosses[0]);
			}
		}
	}
}

void SDungeonHUDWidget::DrawBar(FSlateWindowElementList& OutDrawElements, int32& InOutLayerId,
                               const FGeometry& AllottedGeometry,
                               float X, float Y, float Width, float Height,
                               float FillPercent, float GhostPercent,
                               const FLinearColor& FillColor, const FLinearColor& GhostColor,
                               const FLinearColor& BorderColor) const
{
	const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
	if (!WhiteBrush)
	{
		WhiteBrush = FCoreStyle::Get().GetDefaultBrush();
	}
	if (!WhiteBrush) return;

	// 1. Dark Shadow Drop / Outer Border Margin
	const float BorderThick = 2.0f;
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		InOutLayerId++,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(Width + (BorderThick * 2.0f), Height + (BorderThick * 2.0f)),
			FSlateLayoutTransform(FVector2D(X - BorderThick, Y - BorderThick))
		),
		WhiteBrush,
		ESlateDrawEffect::None,
		BorderColor
	);

	// 2. Dark Empty Background Slate
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		InOutLayerId++,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(Width, Height),
			FSlateLayoutTransform(FVector2D(X, Y))
		),
		WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.04f, 0.04f, 0.05f, 0.92f)
	);

	// 3. Ghost Catch-up Bar (shows damage taken)
	const float GhostWidth = FMath::Clamp(GhostPercent, 0.0f, 1.0f) * Width;
	if (GhostWidth > 0.0f)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			InOutLayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(GhostWidth, Height),
				FSlateLayoutTransform(FVector2D(X, Y))
			),
			WhiteBrush,
			ESlateDrawEffect::None,
			GhostColor
		);
	}

	// 4. Active Health Bar Fill
	const float FillWidth = FMath::Clamp(FillPercent, 0.0f, 1.0f) * Width;
	if (FillWidth > 0.0f)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			InOutLayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(FillWidth, Height),
				FSlateLayoutTransform(FVector2D(X, Y))
			),
			WhiteBrush,
			ESlateDrawEffect::None,
			FillColor
		);

		// Gloss highlight strip on top half of bar
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			InOutLayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(FillWidth, Height * 0.40f),
				FSlateLayoutTransform(FVector2D(X, Y))
			),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 1.0f, 1.0f, 0.18f)
		);
	}
}

int32 SDungeonHUDWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                                const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                                int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!CharacterOwner.IsValid())
	{
		return LayerId;
	}

	const double CurrentTime = FPlatformTime::Seconds();
	const float DeltaTime = (LastPaintTime > 0.0) ? FMath::Clamp((float)(CurrentTime - LastPaintTime), 0.001f, 0.1f) : 0.016f;
	LastPaintTime = CurrentTime;

	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
	const FSlateFontInfo ValueFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
	const FSlateFontInfo BossTitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);

	// =========================================================================
	// 1. TOP-LEFT PLAYER HEALTH BAR
	// =========================================================================
	const float PlayerCurHP = FMath::Max(0.0f, CharacterOwner->CurrentHealth);
	const float PlayerMaxHP = FMath::Max(1.0f, CharacterOwner->MaxHealth);
	const float TargetPlayerPct = FMath::Clamp(PlayerCurHP / PlayerMaxHP, 0.0f, 1.0f);

	// Smooth ghost catchup
	PlayerGhostPercent = FMath::FInterpTo(PlayerGhostPercent, TargetPlayerPct, DeltaTime, 2.5f);

	const float BarX = 35.0f;
	const float BarY = 32.0f;
	const float BarW = 320.0f;
	const float BarH = 24.0f;

	// Label above bar: "VITALITY"
	const FString PlayerLabel = TEXT("VITALITY");
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId++,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(120.0f, 18.0f),
			FSlateLayoutTransform(FVector2D(BarX + 2.0f, BarY - 18.0f))
		),
		PlayerLabel,
		TitleFont,
		ESlateDrawEffect::None,
		FLinearColor(0.85f, 0.70f, 0.35f, 1.0f) // Gold title
	);

	// Draw player health bar
	FLinearColor CrimsonFill = FLinearColor(0.85f, 0.12f, 0.14f, 1.0f);
	// Low health pulse warning (< 25 HP)
	if (TargetPlayerPct < 0.25f)
	{
		const float Pulse = (FMath::Sin((float)CurrentTime * 7.0f) + 1.0f) * 0.5f;
		CrimsonFill = FMath::Lerp(FLinearColor(0.70f, 0.05f, 0.05f, 1.0f), FLinearColor(1.0f, 0.25f, 0.25f, 1.0f), Pulse);
	}

	DrawBar(
		OutDrawElements,
		LayerId,
		AllottedGeometry,
		BarX, BarY, BarW, BarH,
		TargetPlayerPct, PlayerGhostPercent,
		CrimsonFill,
		FLinearColor(0.65f, 0.35f, 0.10f, 0.85f), // Ghost amber-red
		FLinearColor(0.68f, 0.52f, 0.22f, 1.0f)  // Ornate ancient bronze border
	);

	// Numeric Text: "100 / 100" inside the bar
	const FString PlayerHPText = FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(PlayerCurHP), FMath::RoundToInt(PlayerMaxHP));
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId++,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(BarW, BarH),
			FSlateLayoutTransform(FVector2D(BarX + 12.0f, BarY + 4.0f))
		),
		PlayerHPText,
		ValueFont,
		ESlateDrawEffect::None,
		FLinearColor(1.0f, 1.0f, 1.0f, 0.95f)
	);

	// =========================================================================
	// 2. TOP-CENTER BOSS HEALTH BAR (Asterion the Minotaur)
	// =========================================================================
	if (CachedBoss.IsValid())
	{
		const bool bBossAlive = (CachedBoss->BossState != EMinotaurState::Dead);
		const bool bBossActive = (CachedBoss->BossState == EMinotaurState::Engaged ||
		                          CachedBoss->BossState == EMinotaurState::Attacking ||
		                          CachedBoss->BossState == EMinotaurState::Charging ||
		                          CachedBoss->BossState == EMinotaurState::Stunned);

		const float DistToBoss = FVector::Dist(CharacterOwner->GetActorLocation(), CachedBoss->GetActorLocation());

		// Display boss bar when fighting Asterion
		if (bBossActive || (bBossAlive && DistToBoss < 2800.0f) || (!bBossAlive && CachedBoss->CurrentHealth <= 0.0f && DistToBoss < 3000.0f))
		{
			const FVector2D ScreenSize = AllottedGeometry.GetLocalSize();
			const float BossBarW = 620.0f;
			const float BossBarH = 22.0f;
			const float BossBarX = (ScreenSize.X - BossBarW) * 0.5f;
			const float BossBarY = 48.0f;

			const float BossCurHP = FMath::Max(0.0f, CachedBoss->CurrentHealth);
			const float BossMaxHP = FMath::Max(1.0f, CachedBoss->MaxHealth);
			const float TargetBossPct = FMath::Clamp(BossCurHP / BossMaxHP, 0.0f, 1.0f);

			BossGhostPercent = FMath::FInterpTo(BossGhostPercent, TargetBossPct, DeltaTime, 1.8f);

			// Boss Title Text
			const FString BossTitle = bBossAlive ? TEXT("ASTERION - BULL OF THE LABYRINTH") : TEXT("ASTERION SLAIN");
			const FLinearColor BossTitleColor = bBossAlive ? FLinearColor(0.95f, 0.28f, 0.22f, 1.0f) : FLinearColor(0.95f, 0.80f, 0.35f, 1.0f);

			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId++,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(BossBarW, 22.0f),
					FSlateLayoutTransform(FVector2D(BossBarX + 6.0f, BossBarY - 22.0f))
				),
				BossTitle,
				BossTitleFont,
				ESlateDrawEffect::None,
				BossTitleColor
			);

			// Draw Boss Bar
			DrawBar(
				OutDrawElements,
				LayerId,
				AllottedGeometry,
				BossBarX, BossBarY, BossBarW, BossBarH,
				TargetBossPct, BossGhostPercent,
				FLinearColor(0.78f, 0.08f, 0.08f, 1.0f),
				FLinearColor(0.60f, 0.25f, 0.05f, 0.85f),
				FLinearColor(0.85f, 0.65f, 0.25f, 1.0f)
			);

			// Numeric Boss HP Text
			const FString BossHPText = FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(BossCurHP), FMath::RoundToInt(BossMaxHP));
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId++,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(BossBarW, BossBarH),
					FSlateLayoutTransform(FVector2D(BossBarX + (BossBarW * 0.5f) - 35.0f, BossBarY + 3.0f))
				),
				BossHPText,
				ValueFont,
				ESlateDrawEffect::None,
				FLinearColor(1.0f, 1.0f, 1.0f, 0.92f)
			);
		}
	}

	return LayerId;
}
