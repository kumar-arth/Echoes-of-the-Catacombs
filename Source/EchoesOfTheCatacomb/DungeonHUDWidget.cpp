// Copyright Epic Games, Inc. All Rights Reserved.

#include "DungeonHUDWidget.h"
#include "EchoesOfTheCatacombCharacter.h"
#include "BossMinotaur.h"
#include "BossGrux.h"
#include "BossShadowKnight.h"
#include "DungeonGenerator.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	float g_DungeonBossIntroTimer = 0.0f;
	const float g_DungeonBossIntroDuration = 4.5f;
	FString g_DungeonBossIntroTitle;
	FString g_DungeonBossIntroSubtitle;
	TWeakObjectPtr<AActor> g_DungeonLastIntroducedBoss = nullptr;
}

void SDungeonHUDWidget::Construct(const FArguments& InArgs)
{
	CharacterOwner = InArgs._Character;
	SetCanTick(true);
}

void SDungeonHUDWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Find the closest alive boss within the boss room arena
	if (CharacterOwner.IsValid())
	{
		if (UWorld* World = CharacterOwner->GetWorld())
		{
			const FVector PlayerLoc = CharacterOwner->GetActorLocation();
			AActor* ClosestBoss = nullptr;
			float MinDist = 1000000.0f;

			TArray<AActor*> CandidateBosses;
			UGameplayStatics::GetAllActorsOfClass(World, ABossShadowKnight::StaticClass(), CandidateBosses);
			UGameplayStatics::GetAllActorsOfClass(World, ABossGrux::StaticClass(), CandidateBosses);
			UGameplayStatics::GetAllActorsOfClass(World, ABossMinotaur::StaticClass(), CandidateBosses);
			UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("Boss")), CandidateBosses);

			for (AActor* BossActor : CandidateBosses)
			{
				if (BossActor)
				{
					bool bIsAlive = false;
					bool bIsDormant = true;
					float CurHP = 0.0f;
					float MaxHP = 1.0f;
					if (ABossShadowKnight* K = Cast<ABossShadowKnight>(BossActor))
					{
						bIsAlive = (K->BossState != EShadowKnightState::Dead && K->CurrentHealth > 0.0f);
						bIsDormant = (K->BossState == EShadowKnightState::Dormant);
						CurHP = K->CurrentHealth;
						MaxHP = K->MaxHealth;
					}
					else if (ABossGrux* G = Cast<ABossGrux>(BossActor))
					{
						bIsAlive = (G->BossState != EGruxState::Dead && G->CurrentHealth > 0.0f);
						bIsDormant = (G->BossState == EGruxState::Dormant);
						CurHP = G->CurrentHealth;
						MaxHP = G->MaxHealth;
					}
					else if (ABossMinotaur* M = Cast<ABossMinotaur>(BossActor))
					{
						bIsAlive = (M->BossState != EMinotaurState::Dead && M->CurrentHealth > 0.0f);
						bIsDormant = (M->BossState == EMinotaurState::Dormant);
						CurHP = M->CurrentHealth;
						MaxHP = M->MaxHealth;
					}

					// Only consider alive bosses
					if (!bIsAlive || CurHP <= 0.0f)
					{
						continue;
					}

					const float Dist = FVector::Dist(PlayerLoc, BossActor->GetActorLocation());
					// Boss arena is 3500x3500 cm. Only track boss when engaged in combat or woken up by entering the door (<= 3200 cm). Never track while dormant from outside walls/doors!
					const bool bInRoom = (!bIsDormant || CurHP < MaxHP) && (Dist <= 3200.0f);
					if (bInRoom && Dist < MinDist)
					{
						MinDist = Dist;
						ClosestBoss = BossActor;
					}
				}
			}

			if (ClosestBoss && ClosestBoss != g_DungeonLastIntroducedBoss)
			{
				g_DungeonLastIntroducedBoss = ClosestBoss;
				g_DungeonBossIntroTimer = g_DungeonBossIntroDuration;
				const FString ClassName = ClosestBoss->GetClass()->GetName();
				if (ClassName.Contains(TEXT("ShadowKnight")))
				{
					g_DungeonBossIntroTitle = TEXT("MALIKOR, THE SHADOW KNIGHT");
					g_DungeonBossIntroSubtitle = TEXT("CHAMPION OF THE ABYSSAL FLAME");
				}
				else if (ClassName.Contains(TEXT("Grux")))
				{
					g_DungeonBossIntroTitle = TEXT("VALAKOR, THE TWIN-BLADE BERSERKER");
					g_DungeonBossIntroSubtitle = TEXT("BLADE-DANCER OF THE SHADOWS");
				}
				else if (ClassName.Contains(TEXT("Minotaur")))
				{
					g_DungeonBossIntroTitle = TEXT("AURELIUS, THE IRON BASTION");
					g_DungeonBossIntroSubtitle = TEXT("BULWARK OF THE CELESTIAL ORDER");
				}
				else
				{
					g_DungeonBossIntroTitle = TEXT("BOSS OF THE CATACOMBS");
					g_DungeonBossIntroSubtitle = TEXT("ANCIENT GUARDIAN");
				}
			}

			CachedBossActor = ClosestBoss;
		}
	}

	if (g_DungeonBossIntroTimer > 0.0f)
	{
		g_DungeonBossIntroTimer = FMath::Max(0.0f, g_DungeonBossIntroTimer - (float)InDeltaTime);
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
	// 1b. TOP-LEFT PLAYER STAMINA BAR (Dark Souls Green Stamina Gauge)
	// =========================================================================
	const float PlayerCurStamina = FMath::Max(0.0f, CharacterOwner->CurrentStamina);
	const float PlayerMaxStamina = FMath::Max(1.0f, CharacterOwner->MaxStamina);
	const float TargetStaminaPct = FMath::Clamp(PlayerCurStamina / PlayerMaxStamina, 0.0f, 1.0f);

	PlayerStaminaGhostPercent = FMath::FInterpTo(PlayerStaminaGhostPercent, TargetStaminaPct, DeltaTime, 4.0f);

	const float StaminaBarY = BarY + BarH + 6.0f; // 62.0f
	const float StaminaBarW = BarW * 0.85f;        // 272.0f (classic Souls staggered width)
	const float StaminaBarH = 10.0f;

	DrawBar(
		OutDrawElements,
		LayerId,
		AllottedGeometry,
		BarX, StaminaBarY, StaminaBarW, StaminaBarH,
		TargetStaminaPct, PlayerStaminaGhostPercent,
		FLinearColor(0.12f, 0.75f, 0.28f, 1.0f), // Souls emerald green
		FLinearColor(0.20f, 0.45f, 0.15f, 0.85f), // Darker green ghost
		FLinearColor(0.45f, 0.40f, 0.20f, 0.90f)  // Bronze border
	);

	// =========================================================================
	// 2. TOP-CENTER BOSS HEALTH BAR (Dynamic for active Boss: Malikor, Kragthor, or Asterion)
	// =========================================================================
	if (CachedBossActor.IsValid() && CharacterOwner.IsValid())
	{
		bool bBossAlive = false;
		bool bBossDormant = true;
		float BossCurHP = 0.0f;
		float BossMaxHP = 1.0f;
		FString BossTitle = TEXT("BOSS");

		if (ABossShadowKnight* Knight = Cast<ABossShadowKnight>(CachedBossActor.Get()))
		{
			bBossAlive = (Knight->BossState != EShadowKnightState::Dead && Knight->CurrentHealth > 0.0f);
			bBossDormant = (Knight->BossState == EShadowKnightState::Dormant);
			BossCurHP = FMath::Max(0.0f, Knight->CurrentHealth);
			BossMaxHP = FMath::Max(1.0f, Knight->MaxHealth);
			BossTitle = TEXT("MALIKOR - THE SHADOW KNIGHT");
		}
		else if (ABossGrux* Grux = Cast<ABossGrux>(CachedBossActor.Get()))
		{
			bBossAlive = (Grux->BossState != EGruxState::Dead && Grux->CurrentHealth > 0.0f);
			bBossDormant = (Grux->BossState == EGruxState::Dormant);
			BossCurHP = FMath::Max(0.0f, Grux->CurrentHealth);
			BossMaxHP = FMath::Max(1.0f, Grux->MaxHealth);
			BossTitle = TEXT("VALAKOR - THE TWIN-BLADE BERSERKER");
		}
		else if (ABossMinotaur* Minotaur = Cast<ABossMinotaur>(CachedBossActor.Get()))
		{
			bBossAlive = (Minotaur->BossState != EMinotaurState::Dead && Minotaur->CurrentHealth > 0.0f);
			bBossDormant = (Minotaur->BossState == EMinotaurState::Dormant);
			BossCurHP = FMath::Max(0.0f, Minotaur->CurrentHealth);
			BossMaxHP = FMath::Max(1.0f, Minotaur->MaxHealth);
			BossTitle = TEXT("AURELIUS - THE IRON BASTION");
		}

		const float DistToBoss = FVector::Dist(CharacterOwner->GetActorLocation(), CachedBossActor->GetActorLocation());
		// Boss arena is 3500x3500 cm. Only show when engaged in combat or woken up by entering the door (<= 3200 cm). Never show while dormant from outside walls/doors!
		const bool bInsideBossRoom = (!bBossDormant || BossCurHP < BossMaxHP) && (DistToBoss <= 3200.0f);

		// ONLY display when the boss is alive AND the player is inside the boss room
		if (bBossAlive && BossCurHP > 0.0f && bInsideBossRoom)
		{
			const FVector2D ScreenSize = AllottedGeometry.GetLocalSize();
			const float BossBarW = 620.0f;
			const float BossBarH = 22.0f;
			const float BossBarX = (ScreenSize.X - BossBarW) * 0.5f;
			const float BossBarY = 48.0f;

			const float TargetBossPct = FMath::Clamp(BossCurHP / BossMaxHP, 0.0f, 1.0f);
			BossGhostPercent = FMath::FInterpTo(BossGhostPercent, TargetBossPct, DeltaTime, 1.8f);

			const FLinearColor BossTitleColor = FLinearColor(0.95f, 0.28f, 0.22f, 1.0f);

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

	// =========================================================================
	// BOSS INTRO BANNER (Souls-style entrance pop-up when passing through door)
	// =========================================================================
	if (g_DungeonBossIntroTimer > 0.0f && CachedBossActor.IsValid())
	{
		const FVector2D ScreenSize = AllottedGeometry.GetLocalSize();
		const float Elapsed = g_DungeonBossIntroDuration - g_DungeonBossIntroTimer;
		float Alpha = 1.0f;
		if (Elapsed < 0.7f)
		{
			Alpha = Elapsed / 0.7f;
		}
		else if (g_DungeonBossIntroTimer < 1.0f)
		{
			Alpha = g_DungeonBossIntroTimer / 1.0f;
		}
		Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

		const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
		if (!WhiteBrush) WhiteBrush = FCoreStyle::Get().GetDefaultBrush();

		if (WhiteBrush && Alpha > 0.01f)
		{
			const float BannerW = FMath::Min(860.0f, (float)ScreenSize.X * 0.85f);
			const float BannerH = 74.0f;
			const float BannerX = ((float)ScreenSize.X - BannerW) * 0.5f;
			const float BannerY = (float)ScreenSize.Y * 0.28f;

			// Dark translucent backing box
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId++,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(BannerW, BannerH),
					FSlateLayoutTransform(FVector2D(BannerX, BannerY))
				),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.02f, 0.02f, 0.03f, 0.85f * Alpha)
			);

			// Top & bottom golden hairline borders
			const float TrimHeight = 1.5f;
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId++,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(BannerW, TrimHeight),
					FSlateLayoutTransform(FVector2D(BannerX, BannerY))
				),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.88f, 0.72f, 0.28f, 0.90f * Alpha)
			);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId++,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(BannerW, TrimHeight),
					FSlateLayoutTransform(FVector2D(BannerX, BannerY + BannerH - TrimHeight))
				),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.88f, 0.72f, 0.28f, 0.90f * Alpha)
			);

			// Boss Name Text
			const FSlateFontInfo IntroTitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 20);
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId++,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(BannerW, 30.0f),
					FSlateLayoutTransform(FVector2D(BannerX + 24.0f, BannerY + 12.0f))
				),
				g_DungeonBossIntroTitle,
				IntroTitleFont,
				ESlateDrawEffect::None,
				FLinearColor(0.96f, 0.82f, 0.35f, Alpha)
			);

			// Subtitle Text
			const FSlateFontInfo SubtitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId++,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(BannerW, 20.0f),
					FSlateLayoutTransform(FVector2D(BannerX + 26.0f, BannerY + 44.0f))
				),
				g_DungeonBossIntroSubtitle,
				SubtitleFont,
				ESlateDrawEffect::None,
				FLinearColor(0.85f, 0.88f, 0.92f, 0.85f * Alpha)
			);
		}
	}

	// =========================================================================
	// LOCK-ON RETICLE (Souls-style glowing reticle over target)
	// =========================================================================
	if (CharacterOwner.IsValid())
	{
		if (AActor* LockedTarget = CharacterOwner->GetLockedTarget())
		{
			if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
			{
				FVector TargetWorldPos = LockedTarget->GetActorLocation() + FVector(0.0f, 0.0f, 45.0f);
				if (const ACharacter* TargetChar = Cast<ACharacter>(LockedTarget))
				{
					if (const USkeletalMeshComponent* TargetMesh = TargetChar->GetMesh())
					{
						static const FName ChestSockets[] = {
							FName(TEXT("spine_03")), FName(TEXT("spine_02")), FName(TEXT("spine_01")),
							FName(TEXT("spine")), FName(TEXT("head")), FName(TEXT("pelvis"))
						};
						for (const FName& Socket : ChestSockets)
						{
							if (TargetMesh->DoesSocketExist(Socket))
							{
								TargetWorldPos = TargetMesh->GetSocketLocation(Socket);
								break;
							}
						}
					}
				}
				FVector2D ScreenPos2D;
				if (PC->ProjectWorldLocationToScreen(TargetWorldPos, ScreenPos2D, true))
				{
					const FVector2D LocalPos = ScreenPos2D;
					const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
					if (!WhiteBrush) WhiteBrush = FCoreStyle::Get().GetDefaultBrush();

					if (WhiteBrush)
					{
						const float DotSize = 6.0f;
						const FLinearColor GlowColor(1.0f, 1.0f, 1.0f, 0.95f);
						const FLinearColor ShadowColor(0.0f, 0.0f, 0.0f, 0.90f);

						// Shadow
						FSlateDrawElement::MakeBox(
							OutDrawElements,
							LayerId++,
							AllottedGeometry.ToPaintGeometry(
								FVector2D(DotSize + 2.0f, DotSize + 2.0f),
								FSlateLayoutTransform(FVector2D(LocalPos.X - ((DotSize + 2.0f) * 0.5f), LocalPos.Y - ((DotSize + 2.0f) * 0.5f)))
							),
							WhiteBrush, ESlateDrawEffect::None, ShadowColor
						);

						// Dot
						FSlateDrawElement::MakeBox(
							OutDrawElements,
							LayerId++,
							AllottedGeometry.ToPaintGeometry(
								FVector2D(DotSize, DotSize),
								FSlateLayoutTransform(FVector2D(LocalPos.X - (DotSize * 0.5f), LocalPos.Y - (DotSize * 0.5f)))
							),
							WhiteBrush, ESlateDrawEffect::None, GlowColor
						);

						// Brackets
						const float BracketLen = 6.0f;
						const float BracketDist = 12.0f;
						const float BracketThick = 1.5f;

						FSlateDrawElement::MakeBox(
							OutDrawElements, LayerId,
							AllottedGeometry.ToPaintGeometry(FVector2D(BracketThick, BracketLen), FSlateLayoutTransform(FVector2D(LocalPos.X - (BracketThick * 0.5f), LocalPos.Y - BracketDist - BracketLen))),
							WhiteBrush, ESlateDrawEffect::None, GlowColor
						);
						FSlateDrawElement::MakeBox(
							OutDrawElements, LayerId,
							AllottedGeometry.ToPaintGeometry(FVector2D(BracketThick, BracketLen), FSlateLayoutTransform(FVector2D(LocalPos.X - (BracketThick * 0.5f), LocalPos.Y + BracketDist))),
							WhiteBrush, ESlateDrawEffect::None, GlowColor
						);
						FSlateDrawElement::MakeBox(
							OutDrawElements, LayerId,
							AllottedGeometry.ToPaintGeometry(FVector2D(BracketLen, BracketThick), FSlateLayoutTransform(FVector2D(LocalPos.X - BracketDist - BracketLen, LocalPos.Y - (BracketThick * 0.5f)))),
							WhiteBrush, ESlateDrawEffect::None, GlowColor
						);
						FSlateDrawElement::MakeBox(
							OutDrawElements, LayerId++,
							AllottedGeometry.ToPaintGeometry(FVector2D(BracketLen, BracketThick), FSlateLayoutTransform(FVector2D(LocalPos.X + BracketDist, LocalPos.Y - (BracketThick * 0.5f)))),
							WhiteBrush, ESlateDrawEffect::None, GlowColor
						);
					}
				}
			}
		}
	}

	const FVector2D ScreenSize = AllottedGeometry.GetLocalSize();
	const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
	if (!WhiteBrush) WhiteBrush = FCoreStyle::Get().GetDefaultBrush();

	// =========================================================================
	// 5. RELICS / BOSS GEM TRACKER (Top-Right HUD)
	// =========================================================================
	if (WhiteBrush)
	{
		const float RelicBoxW = 160.0f;
		const float RelicBoxH = 36.0f;
		const float RelicBoxX = ScreenSize.X - RelicBoxW - 35.0f;
		const float RelicBoxY = 32.0f;

		// Background box
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(RelicBoxW, RelicBoxH),
				FSlateLayoutTransform(FVector2D(RelicBoxX, RelicBoxY))
			),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.04f, 0.05f, 0.08f, 0.75f)
		);

		// Gold border
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(RelicBoxW, 1.5f),
				FSlateLayoutTransform(FVector2D(RelicBoxX, RelicBoxY))
			),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.85f, 0.72f, 0.28f, 0.80f)
		);

		// Label: "RELICS 1/3"
		const FSlateFontInfo RelicFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
		const FString RelicLabel = FString::Printf(TEXT("RELICS  [%d/3]"), FDungeonGameState::GetGemCount());
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(RelicBoxW, 16.0f),
				FSlateLayoutTransform(FVector2D(RelicBoxX + 10.0f, RelicBoxY + 10.0f))
			),
			RelicLabel,
			RelicFont,
			ESlateDrawEffect::None,
			FLinearColor(0.92f, 0.88f, 0.75f, 0.95f)
		);

		// 3 Gem Indicators: Ruby (Room 1), Emerald (Room 2), Sapphire (Room 3)
		const FLinearColor GemColors[3] = {
			FLinearColor(1.0f, 0.25f, 0.15f),
			FLinearColor(0.15f, 0.95f, 0.35f),
			FLinearColor(0.25f, 0.60f, 1.0f)
		};
		const float IconSize = 14.0f;
		const float IconStartX = RelicBoxX + RelicBoxW - 65.0f;
		const float IconY = RelicBoxY + 11.0f;

		for (int32 i = 0; i < 3; ++i)
		{
			const bool bCollected = FDungeonGameState::HasGem(i + 1);
			const FLinearColor DrawCol = bCollected ? GemColors[i] : FLinearColor(0.25f, 0.25f, 0.28f, 0.40f);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId++,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(IconSize, IconSize),
					FSlateLayoutTransform(FVector2D(IconStartX + (i * 18.0f), IconY))
				),
				WhiteBrush,
				ESlateDrawEffect::None,
				DrawCol
			);
		}
	}

	// =========================================================================
	// 6. INTERACTION PROMPT PILL (Center-Bottom HUD)
	// =========================================================================
	const FString PromptText = FDungeonGameState::GetPromptText();
	if (!PromptText.IsEmpty() && WhiteBrush && !FDungeonGameState::HasPlayerWon())
	{
		const float PillW = FMath::Clamp((float)(PromptText.Len() * 9.5f + 40.0f), 260.0f, 650.0f);
		const float PillH = 34.0f;
		const float PillX = (ScreenSize.X - PillW) * 0.5f;
		const float PillY = ScreenSize.Y * 0.70f;

		// Pill background
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(PillW, PillH),
				FSlateLayoutTransform(FVector2D(PillX, PillY))
			),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.04f, 0.05f, 0.09f, 0.88f)
		);

		// Gold trim top
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(PillW, 1.5f),
				FSlateLayoutTransform(FVector2D(PillX, PillY))
			),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.90f, 0.75f, 0.28f, 0.90f)
		);

		// Gold trim bottom
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(PillW, 1.5f),
				FSlateLayoutTransform(FVector2D(PillX, PillY + PillH - 1.5f))
			),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.90f, 0.75f, 0.28f, 0.90f)
		);

		// Prompt text
		const FSlateFontInfo PromptFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(PillW, PillH),
				FSlateLayoutTransform(FVector2D(PillX + 16.0f, PillY + 8.0f))
			),
			PromptText,
			PromptFont,
			ESlateDrawEffect::None,
			FLinearColor(0.98f, 0.92f, 0.72f, 1.0f)
		);
	}

	// =========================================================================
	// 7. VICTORY SCREEN (Full-Screen Victory Presentation)
	// =========================================================================
	if (FDungeonGameState::HasPlayerWon() && WhiteBrush)
	{
		// Dark Vignette Overlay
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(
				ScreenSize,
				FSlateLayoutTransform(FVector2D(0.0f, 0.0f))
			),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.01f, 0.01f, 0.02f, 0.82f)
		);

		const float BannerW = 740.0f;
		const float BannerH = 150.0f;
		const float BannerX = (ScreenSize.X - BannerW) * 0.5f;
		const float BannerY = (ScreenSize.Y - BannerH) * 0.42f;

		// Banner background
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(BannerW, BannerH),
				FSlateLayoutTransform(FVector2D(BannerX, BannerY))
			),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.04f, 0.05f, 0.08f, 0.92f)
		);

		// Gold top header line
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(BannerW, 3.0f),
				FSlateLayoutTransform(FVector2D(BannerX, BannerY))
			),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.95f, 0.82f, 0.32f, 1.0f)
		);

		// Gold bottom footer line
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(BannerW, 3.0f),
				FSlateLayoutTransform(FVector2D(BannerX, BannerY + BannerH - 3.0f))
			),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.95f, 0.82f, 0.32f, 1.0f)
		);

		// Main Title: "VICTORY ACHIEVED"
		const FSlateFontInfo VictoryTitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 28);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(BannerW, 40.0f),
				FSlateLayoutTransform(FVector2D(BannerX + 40.0f, BannerY + 22.0f))
			),
			TEXT("VICTORY ACHIEVED"),
			VictoryTitleFont,
			ESlateDrawEffect::None,
			FLinearColor(0.98f, 0.84f, 0.30f, 1.0f)
		);

		// Subtitle: "YOU ESCAPED THE CATACOMBS"
		const FSlateFontInfo VictorySubFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(BannerW, 24.0f),
				FSlateLayoutTransform(FVector2D(BannerX + 42.0f, BannerY + 68.0f))
			),
			TEXT("YOU ESCAPED THE CATACOMBS"),
			VictorySubFont,
			ESlateDrawEffect::None,
			FLinearColor(0.92f, 0.94f, 0.96f, 0.95f)
		);

		// Detail: "All 3 Boss Relics Restored — Total Victory!"
		const FSlateFontInfo VictoryDetailFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(BannerW, 20.0f),
				FSlateLayoutTransform(FVector2D(BannerX + 42.0f, BannerY + 100.0f))
			),
			TEXT("All 3 Boss Relics Restored \u2014 The Ancient Seal has been lifted!"),
			VictoryDetailFont,
			ESlateDrawEffect::None,
			FLinearColor(0.80f, 0.85f, 0.80f, 0.85f)
		);
	}

	return LayerId;
}
