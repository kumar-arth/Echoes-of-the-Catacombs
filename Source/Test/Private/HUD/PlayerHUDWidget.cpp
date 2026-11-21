#include "HUD/PlayerHUDWidget.h"
#include "HUD/BaseHealthBarWidget.h"
#include "Components/ProgressBar.h"
#include "AttributeComponent/AttributeComponent.h"
#include "Utils/DebugDrawHelper.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "Rendering/DrawElements.h"
#include "Widgets/InvalidateWidgetReason.h"
#include "Engine/Texture2D.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Character/MyCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"

namespace
{
	float g_BossIntroBannerTimer = 0.0f;
	const float g_BossIntroBannerDuration = 4.5f;
	FString g_BossIntroBannerTitle;
	FString g_BossIntroBannerSubtitle;
	TWeakObjectPtr<AActor> g_LastIntroducedBoss = nullptr;
}

void UPlayerHUDWidget::SetHealthPercent(float Percent)
{
	RefreshHealthValueText();

	if (PB_Health)
	{
		// 更新目标值
		TargetHealthPercent = Percent;

		// 掉血：立即更新 + 触发红晕
		if (Percent < CurrentHealthPercent)
		{
			CurrentHealthPercent = Percent;
			PB_Health->SetPercent(Percent);
			CurrentBufferDelay = BufferDelayTime;
			DamageFlashPeakAlphaScaled = DamageFlashPeakAlpha * PendingDamageFlashScale;
			DamageFlashTimer = 0.f;
			bDamageFlashAttacking = true;
			PendingDamageFlashScale = 1.f;
		}
		// 回血：启动插值（在 NativeTick 中处理）
	}
}

void UPlayerHUDWidget::SetStaminaPercent(float Percent)
{
	if (PB_Stamina)
	{
		PB_Stamina->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Text_StaminaValue)
	{
		Text_StaminaValue->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UPlayerHUDWidget::SetPotionCount(int32 CurrentCount, int32 MaxCount)
{
	CurrentPotionCount = CurrentCount;
	MaxPotionCount = MaxCount;
	RefreshPotionVisuals();
}

void UPlayerHUDWidget::SetGoldCount(int32 CurrentGold)
{
	if (Text_GoldCount)
	{
		Text_GoldCount->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UPlayerHUDWidget::SetPotionCooldown(float RemainingTime, float TotalTime)
{
	PotionCooldownDuration = FMath::Max(0.f, TotalTime);
	PotionCooldownRemaining = FMath::Clamp(RemainingTime, 0.f, PotionCooldownDuration);
	bPotionCooldownActive = PotionCooldownDuration > 0.f && PotionCooldownRemaining > PotionCooldownTextHideThreshold;

	RefreshPotionVisuals();
}

void UPlayerHUDWidget::BindToAttributes(UAttributeComponent* Attributes)
{
	UnbindFromAttributes();
	BoundAttributes = Attributes;

	// Hide Stamina, Potion, and Gold UI
	if (PB_Stamina) PB_Stamina->SetVisibility(ESlateVisibility::Collapsed);
	if (Text_StaminaValue) Text_StaminaValue->SetVisibility(ESlateVisibility::Collapsed);
	if (Text_PotionCount) Text_PotionCount->SetVisibility(ESlateVisibility::Collapsed);
	if (Image_PotionIcon) Image_PotionIcon->SetVisibility(ESlateVisibility::Collapsed);
	if (Image_PotionCooldownOverlay) Image_PotionCooldownOverlay->SetVisibility(ESlateVisibility::Collapsed);
	if (PB_PotionCooldown) PB_PotionCooldown->SetVisibility(ESlateVisibility::Collapsed);
	if (Text_PotionCooldown) Text_PotionCooldown->SetVisibility(ESlateVisibility::Collapsed);
	if (Text_GoldCount) Text_GoldCount->SetVisibility(ESlateVisibility::Collapsed);

	if (UWidget* W = GetWidgetFromName(TEXT("Text_SPLabel"))) W->SetVisibility(ESlateVisibility::Collapsed);
	if (UWidget* W = GetWidgetFromName(TEXT("Size_SPLabel"))) W->SetVisibility(ESlateVisibility::Collapsed);
	if (UWidget* W = GetWidgetFromName(TEXT("HB_PotionSlot"))) W->SetVisibility(ESlateVisibility::Collapsed);
	if (UWidget* W = GetWidgetFromName(TEXT("SB_PotionIconSlot"))) W->SetVisibility(ESlateVisibility::Collapsed);
	if (UWidget* W = GetWidgetFromName(TEXT("SB_PotionCooldownBar"))) W->SetVisibility(ESlateVisibility::Collapsed);
if (PB_Stamina)
	{
		PB_Stamina->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Text_StaminaValue)
	{
		Text_StaminaValue->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Text_GoldCount)
	{
		Text_GoldCount->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (BoundAttributes)
	{
		BoundAttributes->OnHealthChanged.AddDynamic(this, &UPlayerHUDWidget::SetHealthPercent);

		// 初始化血量显示（同步当前值和目标值）
		float InitialHealth = BoundAttributes->GetHealthPercent();
		TargetHealthPercent = InitialHealth;
		CurrentHealthPercent = InitialHealth;
		if (PB_Health)
		{
			PB_Health->SetPercent(InitialHealth);
		}
		RefreshHealthValueText();

		BoundAttributes->OnStaminaChanged.AddDynamic(this, &UPlayerHUDWidget::SetStaminaPercent);
		SetStaminaPercent(BoundAttributes->GetStaminaPercent());

		BoundAttributes->OnPotionCountChanged.AddDynamic(this, &UPlayerHUDWidget::SetPotionCount);
		SetPotionCount(BoundAttributes->GetPotionCount(), BoundAttributes->GetMaxPotionCount());

		BoundAttributes->OnGoldChanged.AddDynamic(this, &UPlayerHUDWidget::SetGoldCount);
		SetGoldCount(BoundAttributes->GetGold());
	}
}

void UPlayerHUDWidget::UnbindFromAttributes()
{
	if (!BoundAttributes)
	{
		return;
	}

	BoundAttributes->OnHealthChanged.RemoveDynamic(this, &UPlayerHUDWidget::SetHealthPercent);
	BoundAttributes->OnStaminaChanged.RemoveDynamic(this, &UPlayerHUDWidget::SetStaminaPercent);
	BoundAttributes->OnPotionCountChanged.RemoveDynamic(this, &UPlayerHUDWidget::SetPotionCount);
	BoundAttributes->OnGoldChanged.RemoveDynamic(this, &UPlayerHUDWidget::SetGoldCount);
	BoundAttributes = nullptr;
}

void UPlayerHUDWidget::RefreshHealthValueText()
{
	if (!Text_HealthValue || !BoundAttributes)
	{
		return;
	}

	Text_HealthValue->SetText(FText::Format(INVTEXT("{0} / {1}"),
		FMath::RoundToInt(BoundAttributes->GetCurrentHealth()),
		FMath::RoundToInt(BoundAttributes->GetMaxHealth())));
}

void UPlayerHUDWidget::RefreshStaminaValueText()
{
	if (!Text_StaminaValue || !BoundAttributes)
	{
		return;
	}

	Text_StaminaValue->SetText(FText::Format(INVTEXT("{0} / {1}"),
		FMath::RoundToInt(BoundAttributes->GetCurrentStamina()),
		FMath::RoundToInt(BoundAttributes->GetMaxStamina())));
}

void UPlayerHUDWidget::NativeDestruct()
{
	UnbindFromAttributes();
	Super::NativeDestruct();
}

void UPlayerHUDWidget::SetPendingDamageFlashScale(float Scale)
{
	PendingDamageFlashScale = FMath::Max(0.f, Scale);
}

void UPlayerHUDWidget::SetAimReticleVisible(bool bVisible)
{
	if (bAimReticleVisible == bVisible)
	{
		return;
	}

	bAimReticleVisible = bVisible;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UPlayerHUDWidget::SetAimReticleCharge(float Charge)
{
	const float ClampedCharge = FMath::Clamp(Charge, 0.f, 1.f);
	if (FMath::IsNearlyEqual(AimReticleCharge, ClampedCharge))
	{
		return;
	}

	AimReticleCharge = ClampedCharge;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UPlayerHUDWidget::SetAimReticleReFireBlocked(bool bBlocked)
{
	if (bAimReticleReFireBlocked == bBlocked)
	{
		return;
	}

	bAimReticleReFireBlocked = bBlocked;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UPlayerHUDWidget::ShowBowHitMarker()
{
	BowHitMarkerRemaining = 0.12f;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UPlayerHUDWidget::SetArrowCount(int32 LoadedCount, int32 Capacity, bool bVisible)
{
	if (!Text_ArrowCount)
	{
		return;
	}

	Text_ArrowCount->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bVisible)
	{
		Text_ArrowCount->SetText(FText::Format(INVTEXT("{0} / {1}"),
			FMath::Max(0, LoadedCount), FMath::Max(0, Capacity)));
	}
}

void UPlayerHUDWidget::RefreshPotionVisuals()
{
	if (Image_PotionIcon) Image_PotionIcon->SetVisibility(ESlateVisibility::Collapsed);
	if (Image_PotionCooldownOverlay) Image_PotionCooldownOverlay->SetVisibility(ESlateVisibility::Collapsed);
	if (PB_PotionCooldown) PB_PotionCooldown->SetVisibility(ESlateVisibility::Collapsed);
	if (Text_PotionCooldown) Text_PotionCooldown->SetVisibility(ESlateVisibility::Collapsed);
	if (Text_PotionCount) Text_PotionCount->SetVisibility(ESlateVisibility::Collapsed);
	if (UWidget* W = GetWidgetFromName(TEXT("HB_PotionSlot"))) W->SetVisibility(ESlateVisibility::Collapsed);
}

void UPlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 血条恢复插值（先快后慢）
	if (PB_Health && CurrentHealthPercent < TargetHealthPercent)
	{
		CurrentHealthPercent = FMath::FInterpTo(CurrentHealthPercent, TargetHealthPercent, InDeltaTime, HealthRecoverInterpSpeed);
		PB_Health->SetPercent(CurrentHealthPercent);

		// 到达目标值时停止插值
		if (FMath::IsNearlyEqual(CurrentHealthPercent, TargetHealthPercent, 0.001f))
		{
			CurrentHealthPercent = TargetHealthPercent;
			PB_Health->SetPercent(TargetHealthPercent);
		}
	}

	UBaseHealthBarWidget::TickBufferDelayImpl(PB_Buffer, PB_Health,
		CurrentBufferDelay, BufferDelayTime, BufferInterpSpeed, InDeltaTime);

	if (BowHitMarkerRemaining > 0.f)
	{
		BowHitMarkerRemaining = FMath::Max(0.f, BowHitMarkerRemaining - InDeltaTime);
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	// 受击染红：渐入 → 指数衰减
	if (bDamageFlashAttacking)
	{
		DamageFlashTimer += InDeltaTime;
		if (DamageFlashTimer >= DamageFlashAttackDuration)
		{
			DamageFlashAlpha = DamageFlashPeakAlphaScaled;
			bDamageFlashAttacking = false;
		}
		else
		{
			DamageFlashAlpha = DamageFlashPeakAlphaScaled * (DamageFlashTimer / DamageFlashAttackDuration);
		}
	}
	else if (DamageFlashAlpha > 0.f)
	{
		DamageFlashAlpha *= FMath::Pow(0.01f, InDeltaTime / DamageFlashDuration);
		if (DamageFlashAlpha < 0.005f) DamageFlashAlpha = 0.f;
	}

	// Update Boss Health Bar tracking
	BossSearchTimer += InDeltaTime;
	if (BossSearchTimer >= 0.25f || !CachedBossActor.IsValid())
	{
		BossSearchTimer = 0.0f;
		if (UWorld* World = GetWorld())
		{
			APawn* PlayerPawn = GetOwningPlayerPawn();
			const FVector PlayerLoc = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
			AActor* BestBoss = nullptr;
			float BestDist = 10000000.f;

			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Actor = *It;
				if (!Actor || !IsValid(Actor)) continue;
				const FString ClassName = Actor->GetClass()->GetName();
				if (ClassName.Contains(TEXT("Boss")) || Actor->ActorHasTag(FName(TEXT("Boss"))))
				{
					float CurHP = 0.f;
					float MaxHP = 1.f;
					if (FProperty* Prop = Actor->GetClass()->FindPropertyByName(TEXT("CurrentHealth")))
					{
						if (FFloatProperty* FProp = CastField<FFloatProperty>(Prop))
						{
							CurHP = FProp->GetPropertyValue_InContainer(Actor);
						}
					}
					if (FProperty* Prop = Actor->GetClass()->FindPropertyByName(TEXT("MaxHealth")))
					{
						if (FFloatProperty* FProp = CastField<FFloatProperty>(Prop))
						{
							MaxHP = FProp->GetPropertyValue_InContainer(Actor);
						}
					}

					// Only consider alive bosses!
					if (CurHP <= 0.0f)
					{
						continue;
					}

					// Check dormancy via BossState reflection
					bool bIsDormant = false;
					if (FByteProperty* StateProp = CastField<FByteProperty>(Actor->GetClass()->FindPropertyByName(TEXT("BossState"))))
					{
						bIsDormant = (StateProp->GetPropertyValue_InContainer(Actor) == 0);
					}
					else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Actor->GetClass()->FindPropertyByName(TEXT("BossState"))))
					{
						if (FNumericProperty* UnderProp = EnumProp->GetUnderlyingProperty())
						{
							bIsDormant = (UnderProp->GetSignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(Actor)) == 0);
						}
					}

					// Never track a dormant boss from outside walls or doors!
					if (bIsDormant && CurHP >= MaxHP)
					{
						continue;
					}

					const float Dist = FVector::Dist(PlayerLoc, Actor->GetActorLocation());
					// Boss is active/engaged (woken up upon crossing entrance door) or damaged (<= 3200 cm)
					const bool bInsideRoom = (Dist <= 3200.0f);
					if (bInsideRoom && Dist < BestDist)
					{
						BestDist = Dist;
						BestBoss = Actor;
					}
				}
			}
			CachedBossActor = BestBoss;
			if (!BestBoss)
			{
				g_BossIntroBannerTimer = 0.0f;
			}
		}
	}

	if (CachedBossActor.IsValid())
	{
		AActor* Boss = CachedBossActor.Get();
		if (FProperty* CurHPProp = Boss->GetClass()->FindPropertyByName(TEXT("CurrentHealth")))
		{
			if (FFloatProperty* FCurHP = CastField<FFloatProperty>(CurHPProp))
			{
				BossCurrentHP = FMath::Max(0.0f, FCurHP->GetPropertyValue_InContainer(Boss));
			}
		}
		if (FProperty* MaxHPProp = Boss->GetClass()->FindPropertyByName(TEXT("MaxHealth")))
		{
			if (FFloatProperty* FMaxHP = CastField<FFloatProperty>(MaxHPProp))
			{
				BossMaxHP = FMath::Max(1.0f, FMaxHP->GetPropertyValue_InContainer(Boss));
			}
		}

		// When boss dies, clear immediately so boss bar disappears
		if (BossCurrentHP <= 0.0f)
		{
			CachedBossActor = nullptr;
			g_BossIntroBannerTimer = 0.0f;
		}
		else
		{
			BossTargetPercent = FMath::Clamp(BossCurrentHP / BossMaxHP, 0.0f, 1.0f);
			BossGhostPercent = FMath::FInterpTo(BossGhostPercent, BossTargetPercent, InDeltaTime, 2.0f);

			const FString ClassName = Boss->GetClass()->GetName();
			if (ClassName.Contains(TEXT("ShadowKnight")))
			{
				BossTitleText = TEXT("MALIKOR - THE SHADOW KNIGHT");
			}
			else if (ClassName.Contains(TEXT("Grux")))
			{
				BossTitleText = TEXT("KRAGTHOR - WARLORD OF THE DEEP");
			}
			else if (ClassName.Contains(TEXT("Minotaur")))
			{
				BossTitleText = TEXT("ASTERION - BULL OF THE LABYRINTH");
			}
			else
			{
				BossTitleText = TEXT("BOSS");
			}

			// Pop up boss name banner when entering the boss room!
			if (CachedBossActor != g_LastIntroducedBoss)
			{
				g_LastIntroducedBoss = CachedBossActor;
				g_BossIntroBannerTimer = g_BossIntroBannerDuration;
				if (ClassName.Contains(TEXT("ShadowKnight")))
				{
					g_BossIntroBannerTitle = TEXT("MALIKOR, THE SHADOW KNIGHT");
					g_BossIntroBannerSubtitle = TEXT("CHAMPION OF THE ABYSSAL FLAME");
				}
				else if (ClassName.Contains(TEXT("Grux")))
				{
					g_BossIntroBannerTitle = TEXT("KRAGTHOR, WARLORD OF THE DEEP");
					g_BossIntroBannerSubtitle = TEXT("PRIMAL TERROR OF THE UNDERCITY");
				}
				else if (ClassName.Contains(TEXT("Minotaur")))
				{
					g_BossIntroBannerTitle = TEXT("ASTERION, BULL OF THE LABYRINTH");
					g_BossIntroBannerSubtitle = TEXT("CORRUPTED BEAST OF THE MAZE");
				}
				else
				{
					g_BossIntroBannerTitle = BossTitleText;
					g_BossIntroBannerSubtitle = TEXT("ANCIENT GUARDIAN OF THE CATACOMBS");
				}
			}
		}
	}

	if (g_BossIntroBannerTimer > 0.0f)
	{
		g_BossIntroBannerTimer = FMath::Max(0.0f, g_BossIntroBannerTimer - InDeltaTime);
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	InitVignetteBrush();
}

int32 UPlayerHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// =========================================================================
	// BOSS HEALTH BAR (Top-Center)
	// ONLY visible when the boss is alive AND the player is inside the boss room
	// =========================================================================
	bool bInsideBossRoom = false;
	if (CachedBossActor.IsValid() && BossCurrentHP > 0.0f)
	{
		APawn* PlayerPawn = GetOwningPlayerPawn();
		const float DistToBoss = PlayerPawn ? FVector::Dist(PlayerPawn->GetActorLocation(), CachedBossActor->GetActorLocation()) : 100000.f;

		// Check dormancy via BossState reflection
		bool bBossDormant = false;
		if (FByteProperty* StateProp = CastField<FByteProperty>(CachedBossActor->GetClass()->FindPropertyByName(TEXT("BossState"))))
		{
			bBossDormant = (StateProp->GetPropertyValue_InContainer(CachedBossActor.Get()) == 0);
		}
		else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(CachedBossActor->GetClass()->FindPropertyByName(TEXT("BossState"))))
		{
			if (FNumericProperty* UnderProp = EnumProp->GetUnderlyingProperty())
			{
				bBossDormant = (UnderProp->GetSignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(CachedBossActor.Get())) == 0);
			}
		}

		// Boss bar only renders when boss is engaged in combat (woken up upon crossing entrance door) or damaged
		bInsideBossRoom = (!bBossDormant || BossCurrentHP < BossMaxHP) && (DistToBoss <= 3200.0f);
		if (bInsideBossRoom)
		{
			const FVector2f LocalSize = FVector2f(AllottedGeometry.GetLocalSize());
			const float BossBarW = 620.0f;
			const float BossBarH = 22.0f;
			const float BossBarX = (LocalSize.X - BossBarW) * 0.5f;
			const float BossBarY = 50.0f;

			const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
			if (!WhiteBrush)
			{
				WhiteBrush = FCoreStyle::Get().GetDefaultBrush();
			}

			if (WhiteBrush)
			{
			// 1. Gold Trim Border Margin
			const float BorderThick = 2.0f;
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				MaxLayer + 1,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(BossBarW + (BorderThick * 2.0f), BossBarH + (BorderThick * 2.0f)),
					FSlateLayoutTransform(FVector2f(BossBarX - BorderThick, BossBarY - BorderThick))
				),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.85f, 0.68f, 0.25f, 1.0f)
			);

			// 2. Dark Background
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				MaxLayer + 2,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(BossBarW, BossBarH),
					FSlateLayoutTransform(FVector2f(BossBarX, BossBarY))
				),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.04f, 0.04f, 0.05f, 0.95f)
			);

			// 3. Ghost Catch-up Bar (Amber)
			const float GhostW = FMath::Clamp(BossGhostPercent, 0.0f, 1.0f) * BossBarW;
			if (GhostW > 0.0f)
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					MaxLayer + 3,
					AllottedGeometry.ToPaintGeometry(
						FVector2f(GhostW, BossBarH),
						FSlateLayoutTransform(FVector2f(BossBarX, BossBarY))
					),
					WhiteBrush,
					ESlateDrawEffect::None,
					FLinearColor(0.70f, 0.35f, 0.08f, 0.85f)
				);
			}

			// 4. Crimson Health Bar Fill
			const float FillW = FMath::Clamp(BossTargetPercent, 0.0f, 1.0f) * BossBarW;
			if (FillW > 0.0f)
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					MaxLayer + 4,
					AllottedGeometry.ToPaintGeometry(
						FVector2f(FillW, BossBarH),
						FSlateLayoutTransform(FVector2f(BossBarX, BossBarY))
					),
					WhiteBrush,
					ESlateDrawEffect::None,
					FLinearColor(0.82f, 0.08f, 0.10f, 1.0f)
				);
			}

			// 5. Boss Title Text
			const FSlateFontInfo BossTitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
			const FLinearColor TitleColor = (BossCurrentHP > 0.0f)
				? FLinearColor(0.95f, 0.25f, 0.20f, 1.0f)
				: FLinearColor(0.95f, 0.80f, 0.35f, 1.0f);

			FSlateDrawElement::MakeText(
				OutDrawElements,
				MaxLayer + 5,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(BossBarW, 22.0f),
					FSlateLayoutTransform(FVector2f(BossBarX + 6.0f, BossBarY - 24.0f))
				),
				BossTitleText,
				BossTitleFont,
				ESlateDrawEffect::None,
				TitleColor
			);

			// 6. Numeric HP Text: "800 / 800"
			const FSlateFontInfo ValueFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
			const FString BossHPText = FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(BossCurrentHP), FMath::RoundToInt(BossMaxHP));
			FSlateDrawElement::MakeText(
				OutDrawElements,
				MaxLayer + 6,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(BossBarW, BossBarH),
					FSlateLayoutTransform(FVector2f(BossBarX + (BossBarW * 0.5f) - 35.0f, BossBarY + 3.0f))
				),
				BossHPText,
				ValueFont,
				ESlateDrawEffect::None,
				FLinearColor(1.0f, 1.0f, 1.0f, 0.95f)
			);

			MaxLayer += 7;
		}
		}
	}

	// =========================================================================
	// BOSS INTRO BANNER (Souls-style entrance banner when passing through door)
	// =========================================================================
	if (g_BossIntroBannerTimer > 0.0f && CachedBossActor.IsValid() && BossCurrentHP > 0.0f && bInsideBossRoom)
	{
		const FVector2f LocalSize = FVector2f(AllottedGeometry.GetLocalSize());
		const float Elapsed = g_BossIntroBannerDuration - g_BossIntroBannerTimer;
		float Alpha = 1.0f;
		if (Elapsed < 0.7f)
		{
			Alpha = Elapsed / 0.7f;
		}
		else if (g_BossIntroBannerTimer < 1.0f)
		{
			Alpha = g_BossIntroBannerTimer / 1.0f;
		}
		Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

		const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
		if (!WhiteBrush) WhiteBrush = FCoreStyle::Get().GetDefaultBrush();

		if (WhiteBrush && Alpha > 0.01f)
		{
			const float BannerW = FMath::Min(860.0f, LocalSize.X * 0.85f);
			const float BannerH = 74.0f;
			const float BannerX = (LocalSize.X - BannerW) * 0.5f;
			const float BannerY = LocalSize.Y * 0.28f;

			// Dark translucent backing box
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				MaxLayer + 30,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(BannerW, BannerH),
					FSlateLayoutTransform(FVector2f(BannerX, BannerY))
				),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.02f, 0.02f, 0.03f, 0.85f * Alpha)
			);

			// Top and Bottom Golden Hairline Trim
			const float TrimHeight = 1.5f;
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				MaxLayer + 31,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(BannerW, TrimHeight),
					FSlateLayoutTransform(FVector2f(BannerX, BannerY))
				),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.88f, 0.72f, 0.28f, 0.90f * Alpha)
			);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				MaxLayer + 31,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(BannerW, TrimHeight),
					FSlateLayoutTransform(FVector2f(BannerX, BannerY + BannerH - TrimHeight))
				),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.88f, 0.72f, 0.28f, 0.90f * Alpha)
			);

			// Boss Name Text (Large Bold Gold)
			const FSlateFontInfo IntroTitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 20);
			FSlateDrawElement::MakeText(
				OutDrawElements,
				MaxLayer + 32,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(BannerW, 30.0f),
					FSlateLayoutTransform(FVector2f(BannerX + 24.0f, BannerY + 12.0f))
				),
				g_BossIntroBannerTitle,
				IntroTitleFont,
				ESlateDrawEffect::None,
				FLinearColor(0.96f, 0.82f, 0.35f, Alpha)
			);

			// Subtitle Text (Silver-White)
			const FSlateFontInfo SubtitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
			FSlateDrawElement::MakeText(
				OutDrawElements,
				MaxLayer + 32,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(BannerW, 20.0f),
					FSlateLayoutTransform(FVector2f(BannerX + 26.0f, BannerY + 44.0f))
				),
				g_BossIntroBannerSubtitle,
				SubtitleFont,
				ESlateDrawEffect::None,
				FLinearColor(0.85f, 0.88f, 0.92f, 0.85f * Alpha)
			);

			MaxLayer += 33;
		}
	}

	// =========================================================================
	// LOCK-ON RETICLE (Souls-style glowing diamond over target)
	// =========================================================================
	if (AMyCharacter* MyChar = Cast<AMyCharacter>(GetOwningPlayerPawn()))
	{
		if (AActor* LockedTarget = MyChar->GetLockedActor())
		{
			APlayerController* PC = Cast<APlayerController>(MyChar->GetController());
			if (PC && IsValid(LockedTarget))
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

				FVector2D WidgetPos2D;
				if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, TargetWorldPos, WidgetPos2D, false))
				{
					const FVector2f LocalPos = FVector2f(WidgetPos2D.X, WidgetPos2D.Y);
					const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
					if (!WhiteBrush) WhiteBrush = FCoreStyle::Get().GetDefaultBrush();

					if (WhiteBrush)
					{
						const float DotSize = 8.0f;
						const FLinearColor ReticleGlowColor(1.0f, 1.0f, 1.0f, 0.95f);
						const FLinearColor ReticleShadowColor(0.0f, 0.0f, 0.0f, 0.9f);

						// Dark shadow backing
						FSlateDrawElement::MakeBox(
							OutDrawElements,
							MaxLayer + 20,
							AllottedGeometry.ToPaintGeometry(
								FVector2f(DotSize + 2.0f, DotSize + 2.0f),
								FSlateLayoutTransform(FVector2f(LocalPos.X - ((DotSize + 2.0f) * 0.5f), LocalPos.Y - ((DotSize + 2.0f) * 0.5f)))
							),
							WhiteBrush,
							ESlateDrawEffect::None,
							ReticleShadowColor
						);

						// White bright center dot
						FSlateDrawElement::MakeBox(
							OutDrawElements,
							MaxLayer + 21,
							AllottedGeometry.ToPaintGeometry(
								FVector2f(DotSize, DotSize),
								FSlateLayoutTransform(FVector2f(LocalPos.X - (DotSize * 0.5f), LocalPos.Y - (DotSize * 0.5f)))
							),
							WhiteBrush,
							ESlateDrawEffect::None,
							ReticleGlowColor
						);

						// Four targeting brackets around the dot
						const float BracketLen = 6.0f;
						const float BracketDist = 14.0f;
						const float BracketThick = 2.0f;

						// Top
						FSlateDrawElement::MakeBox(
							OutDrawElements, MaxLayer + 21,
							AllottedGeometry.ToPaintGeometry(
								FVector2f(BracketThick, BracketLen),
								FSlateLayoutTransform(FVector2f(LocalPos.X - (BracketThick * 0.5f), LocalPos.Y - BracketDist - BracketLen))
							),
							WhiteBrush, ESlateDrawEffect::None, ReticleGlowColor
						);
						// Bottom
						FSlateDrawElement::MakeBox(
							OutDrawElements, MaxLayer + 21,
							AllottedGeometry.ToPaintGeometry(
								FVector2f(BracketThick, BracketLen),
								FSlateLayoutTransform(FVector2f(LocalPos.X - (BracketThick * 0.5f), LocalPos.Y + BracketDist))
							),
							WhiteBrush, ESlateDrawEffect::None, ReticleGlowColor
						);
						// Left
						FSlateDrawElement::MakeBox(
							OutDrawElements, MaxLayer + 21,
							AllottedGeometry.ToPaintGeometry(
								FVector2f(BracketLen, BracketThick),
								FSlateLayoutTransform(FVector2f(LocalPos.X - BracketDist - BracketLen, LocalPos.Y - (BracketThick * 0.5f)))
							),
							WhiteBrush, ESlateDrawEffect::None, ReticleGlowColor
						);
						// Right
						FSlateDrawElement::MakeBox(
							OutDrawElements, MaxLayer + 21,
							AllottedGeometry.ToPaintGeometry(
								FVector2f(BracketLen, BracketThick),
								FSlateLayoutTransform(FVector2f(LocalPos.X + BracketDist, LocalPos.Y - (BracketThick * 0.5f)))
							),
							WhiteBrush, ESlateDrawEffect::None, ReticleGlowColor
						);

						MaxLayer += 22;
					}
				}
			}
		}
	}

	// Red screen damage flash vignette disabled per user request
	/*
	if (DamageFlashAlpha > 0.f)
	{
		if (VignetteBrush.GetResourceObject())
		{
			const FVector2f LocalSize = FVector2f(AllottedGeometry.GetLocalSize());
			const float EdgeAlpha = DamageFlashAlpha * VignetteEdgeAlpha;
			const FLinearColor DrawColor(DamageFlashColor.R, DamageFlashColor.G, DamageFlashColor.B, EdgeAlpha);
			FSlateDrawElement::MakeBox(OutDrawElements, MaxLayer + 1,
				AllottedGeometry.ToPaintGeometry(LocalSize, FSlateLayoutTransform()),
				&VignetteBrush, ESlateDrawEffect::None, DrawColor);
			MaxLayer += 1;
		}
	}
	*/

	// Debug entries drawing disabled

	if (bAimReticleVisible)
	{
		const FVector2f LocalSize = FVector2f(AllottedGeometry.GetLocalSize());
		const FVector2f Center = LocalSize * 0.5f;
		const float ReticleHalfGap = FMath::Lerp(5.f, 1.25f, AimReticleCharge);
		constexpr float ReticleOuterRadius = 10.f;
		const FLinearColor ReticleInnerColor = bAimReticleReFireBlocked
			? FLinearColor(0.95f, 0.17f, 0.12f, 0.95f)
			: FLinearColor(0.94f, 0.96f, 0.98f, 0.95f);

		const FVector2f Segments[][2] = {
			{ FVector2f(Center.X - ReticleOuterRadius, Center.Y), FVector2f(Center.X - ReticleHalfGap, Center.Y) },
			{ FVector2f(Center.X + ReticleHalfGap, Center.Y), FVector2f(Center.X + ReticleOuterRadius, Center.Y) },
			{ FVector2f(Center.X, Center.Y - ReticleOuterRadius), FVector2f(Center.X, Center.Y - ReticleHalfGap) },
			{ FVector2f(Center.X, Center.Y + ReticleHalfGap), FVector2f(Center.X, Center.Y + ReticleOuterRadius) }
		};

		const int32 OutlineLayer = MaxLayer + 1;
		const int32 InnerLayer = OutlineLayer + 1;
		for (const FVector2f (&Segment)[2] : Segments)
		{
			TArray<FVector2f> OutlinePoints = { Segment[0], Segment[1] };
			FSlateDrawElement::MakeLines(OutDrawElements, OutlineLayer, AllottedGeometry.ToPaintGeometry(),
				MoveTemp(OutlinePoints), ESlateDrawEffect::None, FLinearColor(0.02f, 0.03f, 0.04f, 0.7f), true, 3.f);

			TArray<FVector2f> InnerPoints = { Segment[0], Segment[1] };
			FSlateDrawElement::MakeLines(OutDrawElements, InnerLayer, AllottedGeometry.ToPaintGeometry(),
				MoveTemp(InnerPoints), ESlateDrawEffect::None, ReticleInnerColor, true, 1.25f);
		}
		MaxLayer = InnerLayer;
	}

	if (BowHitMarkerRemaining > 0.f)
	{
		const FVector2f LocalSize = FVector2f(AllottedGeometry.GetLocalSize());
		const FVector2f Center = LocalSize * 0.5f;
		constexpr float HitMarkerInnerRadius = 3.f;
		constexpr float HitMarkerOuterRadius = 12.f;
		const float Opacity = FMath::Clamp(BowHitMarkerRemaining / 0.12f, 0.f, 1.f);
		const FVector2f Directions[] = {
			FVector2f(-1.f, -1.f).GetSafeNormal(),
			FVector2f(1.f, 1.f).GetSafeNormal(),
			FVector2f(-1.f, 1.f).GetSafeNormal(),
			FVector2f(1.f, -1.f).GetSafeNormal()
		};

		const int32 OutlineLayer = MaxLayer + 1;
		const int32 InnerLayer = OutlineLayer + 1;
		for (const FVector2f& Direction : Directions)
		{
			const FVector2f SegmentStart = Center + Direction * HitMarkerInnerRadius;
			const FVector2f SegmentEnd = Center + Direction * HitMarkerOuterRadius;

			TArray<FVector2f> OutlinePoints = { SegmentStart, SegmentEnd };
			FSlateDrawElement::MakeLines(OutDrawElements, OutlineLayer, AllottedGeometry.ToPaintGeometry(),
				MoveTemp(OutlinePoints), ESlateDrawEffect::None, FLinearColor(0.02f, 0.03f, 0.04f, Opacity * 0.8f), true, 3.f);

			TArray<FVector2f> InnerPoints = { SegmentStart, SegmentEnd };
			FSlateDrawElement::MakeLines(OutDrawElements, InnerLayer, AllottedGeometry.ToPaintGeometry(),
				MoveTemp(InnerPoints), ESlateDrawEffect::None, FLinearColor(0.96f, 0.97f, 0.99f, Opacity), true, 1.25f);
		}
		MaxLayer = InnerLayer;
	}

	return MaxLayer;
}

void UPlayerHUDWidget::InitVignetteBrush()
{
	if (bVignetteInitialized && FMath::IsNearlyEqual(CachedFadeWidth, VignetteFadeWidth)) return;
	bVignetteInitialized = true;
	CachedFadeWidth = VignetteFadeWidth;

	if (VignetteTexture)
	{
		VignetteBrush.SetResourceObject(nullptr);
		VignetteTexture->ConditionalBeginDestroy();
		VignetteTexture = nullptr;
	}

	const int32 Size = 256;
	VignetteTexture = UTexture2D::CreateTransient(Size, Size, PF_R8G8B8A8);
	if (!VignetteTexture) return;

	FTexture2DMipMap& Mip = VignetteTexture->GetPlatformData()->Mips[0];
	uint8* Pixels = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));

	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			// 归一化坐标：0=边缘, 1=中心
			const float U = (X + 0.5f) / Size;
			const float V = (Y + 0.5f) / Size;
			const float EdgeDist = FMath::Min(
				FMath::Min(U, 1.f - U),
				FMath::Min(V, 1.f - V)
			);

			const float T = 1.f - FMath::Clamp(EdgeDist / CachedFadeWidth, 0.f, 1.f);
			const float Smooth = T * T * (3.f - 2.f * T);  // smoothstep
			const uint8 Alpha = static_cast<uint8>(Smooth * 255.f);

			const int32 PixelIndex = (Y * Size + X) * 4;
			Pixels[PixelIndex + 0] = 255;
			Pixels[PixelIndex + 1] = 255;
			Pixels[PixelIndex + 2] = 255;
			Pixels[PixelIndex + 3] = Alpha;
		}
	}

	Mip.BulkData.Unlock();
	VignetteTexture->SRGB = true;
	VignetteTexture->NeverStream = true;
	VignetteTexture->Filter = TF_Bilinear;
	VignetteTexture->UpdateResource();

	VignetteBrush.SetResourceObject(VignetteTexture);
	VignetteBrush.ImageSize = FVector2D(Size, Size);
	VignetteBrush.DrawAs = ESlateBrushDrawType::Image;
	VignetteBrush.Tiling = ESlateBrushTileType::NoTile;
}
