// Copyright Epic Games, Inc. All Rights Reserved.

#include "CompanionGuide.h"
#include "CompanionVoiceComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "DungeonGenerator.h"
#include "CombatEnemy.h"
#include "CombatDamageable.h"
#include "EchoPulseComponent.h"
#include "Particles/ParticleSystem.h"
#include "Engine/World.h"

ACompanionGuide::ACompanionGuide()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// 1. Root collision sphere
	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	SphereCollision->InitSphereRadius(25.0f);
	SphereCollision->SetCollisionProfileName(TEXT("NoCollision"));
	SphereCollision->SetCanEverAffectNavigation(false);
	RootComponent = SphereCollision;

	// 2. Spherical mesh
	SphereMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SphereMesh"));
	SphereMesh->SetupAttachment(RootComponent);
	SphereMesh->SetCollisionProfileName(TEXT("NoCollision"));
	SphereMesh->SetCanEverAffectNavigation(false);
	SphereMesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.35f)); // ~35cm diameter orb
	SphereMesh->SetCastShadow(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereAsset.Succeeded())
	{
		SphereMesh->SetStaticMesh(SphereAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BaseMaterial.Succeeded())
	{
		SphereMesh->SetMaterial(0, BaseMaterial.Object);
	}

	// 3. Point light giving an ethereal companion glow
	CompanionLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CompanionLight"));
	CompanionLight->SetupAttachment(SphereMesh);
	CompanionLight->SetIntensity(2000.0f);
	CompanionLight->SetAttenuationRadius(550.0f);
	CompanionLight->SetLightColor(DefaultLightColor);
	CompanionLight->SetCastShadows(false);
	CompanionLight->bUseTemperature = false;

	// 4. Voice capture, noise filter, and Gemini LLM component
	VoiceComponent = CreateDefaultSubobject<UCompanionVoiceComponent>(TEXT("VoiceComponent"));
}

void ACompanionGuide::BeginPlay()
{
	Super::BeginPlay();

	// Create dynamic material instance for glow color shifts
	if (SphereMesh)
	{
		UMaterialInterface* CurrentMaterial = SphereMesh->GetMaterial(0);
		if (CurrentMaterial)
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(CurrentMaterial, this);
			if (DynamicMaterial)
			{
				DynamicMaterial->SetVectorParameterValue(FName(TEXT("Color")), DefaultLightColor);
				DynamicMaterial->SetVectorParameterValue(FName(TEXT("BaseColor")), DefaultLightColor);
				SphereMesh->SetMaterial(0, DynamicMaterial);
			}
		}
	}

	// Auto-acquire player
	if (!FollowTarget && GetWorld())
	{
		FollowTarget = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
		if (!FollowTarget)
		{
			FollowTarget = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		}
	}

	// Snap into initial shoulder position
	if (FollowTarget)
	{
		const FVector DesiredWorldOffset = FollowTarget->GetActorRotation().RotateVector(FollowOffset);
		SetActorLocation(FollowTarget->GetActorLocation() + DesiredWorldOffset);
	}
}

void ACompanionGuide::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Tick cooldowns
	if (ConcussCooldownRemaining > 0.0f)
	{
		ConcussCooldownRemaining = FMath::Max(0.0f, ConcussCooldownRemaining - DeltaTime);
	}

	// Tick temporary color timer
	if (TemporaryColorTimer > 0.0f)
	{
		TemporaryColorTimer -= DeltaTime;
		if (TemporaryColorTimer <= 0.0f)
		{
			ResetLightToDefault();
		}
	}

	// Acoustic sound wave diaphragm vibration & light dissipation
	if (SoundWaveVibrationTimer > 0.0f)
	{
		SoundWaveVibrationTimer -= DeltaTime;
		const float Progress = 1.0f - FMath::Clamp(SoundWaveVibrationTimer / 0.35f, 0.0f, 1.0f);
		// Dampened acoustic oscillation
		const float Oscillation = FMath::Sin(Progress * PI * 4.0f) * (1.0f - Progress);
		const float ScaleMult = 1.0f + (0.45f * Oscillation);
		if (SphereMesh)
		{
			SphereMesh->SetRelativeScale3D(FVector(0.35f * ScaleMult));
		}

		if (CompanionLight)
		{
			const float TargetIntensity = FMath::Lerp(5500.0f, 2000.0f, Progress);
			CompanionLight->SetIntensity(TargetIntensity);
		}

		if (SoundWaveVibrationTimer <= 0.0f)
		{
			if (SphereMesh)
			{
				SphereMesh->SetRelativeScale3D(FVector(0.35f));
			}
			if (CompanionLight)
			{
				CompanionLight->SetIntensity(2000.0f);
			}
		}
	}

	// Ensure follow target is acquired
	if (!FollowTarget && GetWorld())
	{
		FollowTarget = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
		if (!FollowTarget)
		{
			FollowTarget = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		}
	}

	// If in an active transient action (concuss, scout, distract), tick it
	if (ActionState != ECompanionActionState::None)
	{
		TickAction(DeltaTime);
		return;
	}

	// Normal primary state machine
	switch (CompanionState)
	{
	case ECompanionState::FollowingPlayer:
		TickFollowing(DeltaTime);
		break;

	case ECompanionState::GuidingToExit:
		TickGuiding(DeltaTime);
		break;

	case ECompanionState::WaitingForPlayer:
		RunningTime += DeltaTime;
		if (AnchorLocation.IsZero())
		{
			AnchorLocation = GetActorLocation();
		}
		if (BobAmplitude > 0.0f)
		{
			FVector BobLoc = AnchorLocation;
			BobLoc.Z += FMath::Sin(RunningTime * BobFrequency) * (BobAmplitude * 0.4f);
			SetActorLocation(BobLoc);
		}
		break;

	case ECompanionState::Idle:
	default:
		break;
	}
}

void ACompanionGuide::TickFollowing(float DeltaTime)
{
	if (!FollowTarget)
	{
		return;
	}

	RunningTime += DeltaTime;

	const FRotator TargetRot = FollowTarget->GetActorRotation();
	const FVector DesiredWorldOffset = TargetRot.RotateVector(FollowOffset);
	FVector DesiredLocation = FollowTarget->GetActorLocation() + DesiredWorldOffset;

	// Sinusoidal vertical bob
	const float BobOffset = FMath::Sin(RunningTime * BobFrequency) * BobAmplitude;
	DesiredLocation.Z += BobOffset;

	// Teleport snap check
	const float DistSq = FVector::DistSquared(GetActorLocation(), DesiredLocation);
	if (DistSq > FMath::Square(TeleportDistanceThreshold))
	{
		SetActorLocation(DesiredLocation);
	}
	else
	{
		const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), DesiredLocation, DeltaTime, FollowLagSpeed);
		SetActorLocation(NewLocation);
	}

	// Rotate towards facing direction
	const FRotator CurrentRot = GetActorRotation();
	const FRotator DesiredRot = FRotator(0.0f, TargetRot.Yaw, 0.0f);
	SetActorRotation(FMath::RInterpTo(CurrentRot, DesiredRot, DeltaTime, RotationInterpSpeed));
}

void ACompanionGuide::TickGuiding(float DeltaTime)
{
	if (GuideDestination.IsZero())
	{
		return;
	}

	RunningTime += DeltaTime;

	FVector CurrentLoc = GetActorLocation();
	FVector Direction = (GuideDestination - CurrentLoc);
	const float Distance = Direction.Size();

	// If player is too far behind while guiding, wait for them
	if (FollowTarget)
	{
		const float PlayerDist = FVector::Dist(FollowTarget->GetActorLocation(), CurrentLoc);
		if (PlayerDist > 900.0f)
		{
			// Hover in place and wait
			CurrentLoc.Z += FMath::Sin(RunningTime * BobFrequency) * (BobAmplitude * 0.3f);
			SetActorLocation(CurrentLoc);
			return;
		}
	}

	if (Distance > 60.0f)
	{
		Direction.Normalize();
		FVector NextLoc = CurrentLoc + Direction * (GuideMoveSpeed * DeltaTime);
		NextLoc.Z += FMath::Sin(RunningTime * BobFrequency) * (BobAmplitude * 0.4f);
		SetActorLocation(NextLoc);

		const FRotator DesiredRot = Direction.Rotation();
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredRot, DeltaTime, RotationInterpSpeed));
	}
	else
	{
		CompanionState = ECompanionState::WaitingForPlayer;
		OnSpeechRequested.Broadcast(TEXT("We have reached the destination!"));
	}
}

void ACompanionGuide::TickAction(float DeltaTime)
{
	ActionTimer += DeltaTime;

	switch (ActionState)
	{
	case ECompanionActionState::Concussing:
	{
		AActor* Target = ActionTargetActor.Get();
		const float DashInDuration = 0.35f;

		if (ActionTimer < DashInDuration && Target)
		{
			// Phase 1: Dash rapidly to enemy head
			const FVector TargetHead = Target->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
			SetActorLocation(FMath::VInterpTo(GetActorLocation(), TargetHead, DeltaTime, 18.0f));
		}
		else if (ActionTimer >= DashInDuration && ActionTimer < (DashInDuration + 0.15f))
		{
			// Phase 2: Impact flash & stun application
			if (Target)
			{
				if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(Target))
				{
					const FVector Impulse = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal() * 600.0f;
					Damageable->ApplyDamage(ConcussDamage, this, GetActorLocation(), Impulse);
					Damageable->NotifyDanger(GetActorLocation(), this);
				}
				else
				{
					UGameplayStatics::ApplyDamage(Target, ConcussDamage, nullptr, this, nullptr);
				}
			}

			// Flash strobe
			if (CompanionLight)
			{
				CompanionLight->SetIntensity(7000.0f);
			}
		}
		else
		{
			// Phase 3: Return to shoulder
			if (CompanionLight)
			{
				CompanionLight->SetIntensity(2000.0f);
			}

			if (FollowTarget)
			{
				const FVector ReturnLoc = FollowTarget->GetActorLocation() + FollowTarget->GetActorRotation().RotateVector(FollowOffset);
				SetActorLocation(FMath::VInterpTo(GetActorLocation(), ReturnLoc, DeltaTime, 10.0f));

				if (FVector::Dist(GetActorLocation(), ReturnLoc) < 80.0f || ActionTimer >= ActionDuration)
				{
					ActionState = ECompanionActionState::None;
					ActionTargetActor.Reset();
					ResetLightToDefault();
				}
			}
			else
			{
				ActionState = ECompanionActionState::None;
			}
		}
		break;
	}

	case ECompanionActionState::Scouting:
	{
		const float OutboundDuration = ActionDuration * 0.5f;

		if (ActionTimer < OutboundDuration)
		{
			SetActorLocation(FMath::VInterpTo(GetActorLocation(), ActionTargetLocation, DeltaTime, 4.0f));
		}
		else
		{
			// Return to player
			if (FollowTarget)
			{
				const FVector ReturnLoc = FollowTarget->GetActorLocation() + FollowTarget->GetActorRotation().RotateVector(FollowOffset);
				SetActorLocation(FMath::VInterpTo(GetActorLocation(), ReturnLoc, DeltaTime, 5.0f));

				if (FVector::Dist(GetActorLocation(), ReturnLoc) < 80.0f || ActionTimer >= ActionDuration)
				{
					ActionState = ECompanionActionState::None;
					ResetLightToDefault();
				}
			}
			else
			{
				ActionState = ECompanionActionState::None;
			}
		}
		break;
	}

	case ECompanionActionState::Distracting:
	{
		// Strobe light rapidly between White and Red
		if (CompanionLight)
		{
			const bool bStrobe = (FMath::FloorToInt(ActionTimer * 10.0f) % 2) == 0;
			CompanionLight->SetLightColor(bStrobe ? FLinearColor::White : DangerAlertColor);
			CompanionLight->SetIntensity(bStrobe ? 5000.0f : 1000.0f);
		}

		// Hold at distraction location
		SetActorLocation(FMath::VInterpTo(GetActorLocation(), ActionTargetLocation, DeltaTime, 8.0f));

		if (ActionTimer >= ActionDuration)
		{
			ActionState = ECompanionActionState::None;
			if (CompanionLight)
			{
				CompanionLight->SetIntensity(2000.0f);
			}
			ResetLightToDefault();
		}
		break;
	}

	default:
		ActionState = ECompanionActionState::None;
		break;
	}
}

// -----------------------------------------------------------------------------
// Step 2A Actions
// -----------------------------------------------------------------------------

bool ACompanionGuide::ConcussTarget(AActor* Target, FString& OutMessage)
{
	if (ConcussCooldownRemaining > 0.0f)
	{
		OutMessage = FString::Printf(TEXT("Concuss is on cooldown (%.1f seconds remaining)."), ConcussCooldownRemaining);
		OnSpeechRequested.Broadcast(OutMessage);
		return false;
	}

	if (!Target)
	{
		Target = FindClosestHostile(ConcussRange);
	}

	if (!Target)
	{
		OutMessage = TEXT("No hostile target in range to concuss!");
		OnSpeechRequested.Broadcast(OutMessage);
		return false;
	}

	ActionState = ECompanionActionState::Concussing;
	ActionTargetActor = Target;
	ActionStartLocation = GetActorLocation();
	ActionTimer = 0.0f;
	ActionDuration = 1.1f;
	ConcussCooldownRemaining = ConcussCooldown;

	SetTemporaryLightColor(FLinearColor::White, 0.6f);
	OutMessage = TEXT("Concussing the enemy!");
	OnSpeechRequested.Broadcast(OutMessage);
	OnActionTriggered.Broadcast(FName(TEXT("Concuss")));
	return true;
}

bool ACompanionGuide::ScanForTraps(float ScanRadius, int32& OutTrapsFound, FString& OutMessage)
{
	OutTrapsFound = 0;
	SetTemporaryLightColor(TrapWarningColor, 4.0f);

	if (GetWorld())
	{
		TArray<AActor*> AllActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);

		const FVector MyLoc = GetActorLocation();
		for (AActor* Act : AllActors)
		{
			if (!Act || Act == this || Act == FollowTarget)
			{
				continue;
			}

			const float Dist = FVector::Dist(MyLoc, Act->GetActorLocation());
			if (Dist <= ScanRadius)
			{
				const FString Name = Act->GetName();
				if (Act->ActorHasTag(TEXT("Trap")) || Name.Contains(TEXT("Trap")) || Name.Contains(TEXT("Spike")) || Name.Contains(TEXT("Hazard")))
				{
					OutTrapsFound++;
				}
			}
		}
	}

	if (OutTrapsFound > 0)
	{
		OutMessage = FString::Printf(TEXT("Warning! %d trap(s) detected in the vicinity. Tread carefully!"), OutTrapsFound);
	}
	else
	{
		OutMessage = TEXT("Sonar sweep complete. No traps detected ahead!");
	}

	OnSpeechRequested.Broadcast(OutMessage);
	OnActionTriggered.Broadcast(FName(TEXT("ScanTraps")));
	return (OutTrapsFound > 0);
}

void ACompanionGuide::QueryBossHealth(FString& OutStatusMessage, float& OutHealthPercent)
{
	OutHealthPercent = 0.0f;
	ADungeonGenerator* DG = GetDungeonGenerator();

	// Search for active boss enemy
	ACombatEnemy* ActiveBoss = nullptr;
	if (GetWorld())
	{
		TArray<AActor*> Enemies;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACombatEnemy::StaticClass(), Enemies);
		for (AActor* EnemyActor : Enemies)
		{
			if (ACombatEnemy* CE = Cast<ACombatEnemy>(EnemyActor))
			{
				if (CE->CurrentHP > 0.0f && (CE->ActorHasTag(TEXT("Boss")) || (DG && DG->IsInsideBossRoom(CE->GetActorLocation()))))
				{
					ActiveBoss = CE;
					break;
				}
			}
		}
	}

	if (ActiveBoss)
	{
		OutHealthPercent = (ActiveBoss->CurrentHP / FMath::Max(1.0f, ActiveBoss->CurrentHP)) * 100.0f;
		if (OutHealthPercent > 70.0f)
		{
			OutStatusMessage = FString::Printf(TEXT("The beast is strong at %.0f%% health! Stay vigilant!"), OutHealthPercent);
		}
		else if (OutHealthPercent > 30.0f)
		{
			OutStatusMessage = FString::Printf(TEXT("The boss is wounded at %.0f%% health! Keep up the assault!"), OutHealthPercent);
		}
		else
		{
			OutStatusMessage = FString::Printf(TEXT("The boss is critically weak at %.0f%%! Strike now!"), OutHealthPercent);
		}
	}
	else if (DG)
	{
		const bool b1 = DG->bBoss1Defeated;
		const bool b2 = DG->bBoss2Defeated;
		const bool b3 = DG->bBoss3Defeated;

		OutStatusMessage = FString::Printf(
			TEXT("No active boss fight currently. Arena status: Boss 1: %s, Boss 2: %s, Boss 3: %s."),
			b1 ? TEXT("Defeated") : TEXT("Alive"),
			b2 ? TEXT("Defeated") : TEXT("Alive"),
			b3 ? TEXT("Defeated") : TEXT("Alive")
		);
	}
	else
	{
		OutStatusMessage = TEXT("No boss presence detected in this chamber.");
	}

	OnSpeechRequested.Broadcast(OutStatusMessage);
	OnActionTriggered.Broadcast(FName(TEXT("QueryBossHealth")));
}

bool ACompanionGuide::OpenUpgradeInterface()
{
	SetTemporaryLightColor(UpgradeGlowColor, 4.0f);
	OnUpgradeRequested.Broadcast();
	OnSpeechRequested.Broadcast(TEXT("Opening item enhancement forge."));
	OnActionTriggered.Broadcast(FName(TEXT("UpgradeItems")));
	return true;
}

bool ACompanionGuide::TriggerEcholocationPulse()
{
	UEchoPulseComponent* PulseComp = nullptr;

	if (FollowTarget)
	{
		PulseComp = FollowTarget->FindComponentByClass<UEchoPulseComponent>();
	}

	if (!PulseComp && GetWorld())
	{
		PulseComp = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)->FindComponentByClass<UEchoPulseComponent>();
	}

	if (PulseComp)
	{
		const FVector CompanionLoc = GetActorLocation();

		// 1. Post-process echolocation wave centered precisely on the companion orb
		PulseComp->TriggerPulse(CompanionLoc);

		// 2. Acoustic diaphragm pulse & light wave flash (cyan sound wave)
		SoundWaveVibrationTimer = 0.35f;
		if (CompanionLight)
		{
			CompanionLight->SetIntensity(5500.0f);
		}
		SetTemporaryLightColor(FLinearColor(0.2f, 0.9f, 1.0f), 0.5f);

		// 3. Acoustic sound wave alert for AI hearing
		MakeNoise(1.0f, Cast<APawn>(FollowTarget), CompanionLoc);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(504, 2.5f, FColor::Cyan, TEXT("[Companion]: [PULSE] Echolocation Sound Wave Pulse!"));
		}

		OnSpeechRequested.Broadcast(TEXT("Echo pulse sent."));
		OnActionTriggered.Broadcast(FName(TEXT("Pulse")));
		return true;
	}

	OnSpeechRequested.Broadcast(TEXT("Echolocation module not found on host."));
	return false;
}

void ACompanionGuide::SetAnchorMode(bool bAnchor)
{
	if (bAnchor)
	{
		CompanionState = ECompanionState::WaitingForPlayer;
		ActionState = ECompanionActionState::None;
		AnchorLocation = GetActorLocation();
		SetTemporaryLightColor(FLinearColor(0.85f, 0.95f, 1.0f), 9999.0f);
		if (CompanionLight)
		{
			CompanionLight->SetAttenuationRadius(800.0f);
			CompanionLight->SetIntensity(3000.0f);
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(503, 4.0f, FColor::Yellow, TEXT("[Companion]: [STAY] Anchored. Holding position here."));
		}
		OnSpeechRequested.Broadcast(TEXT("Anchoring here as a light beacon."));
		OnActionTriggered.Broadcast(FName(TEXT("Stay")));
	}
	else
	{
		CompanionState = ECompanionState::FollowingPlayer;
		ActionState = ECompanionActionState::None;
		AnchorLocation = FVector::ZeroVector;
		FollowOffset = FVector(-30.0f, 55.0f, 75.0f); // Reset to close shoulder formation
		ResetLightToDefault();
		if (CompanionLight)
		{
			CompanionLight->SetAttenuationRadius(550.0f);
			CompanionLight->SetIntensity(2000.0f);
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(503, 4.0f, FColor::Green, TEXT("[Companion]: [FOLLOW] Resuming follow formation."));
		}
		OnSpeechRequested.Broadcast(TEXT("Resuming follow formation."));
		OnActionTriggered.Broadcast(FName(TEXT("Follow")));
	}
}

bool ACompanionGuide::RetreatToSafeDistance(FString& OutMessage)
{
	CompanionState = ECompanionState::FollowingPlayer;
	ActionState = ECompanionActionState::None;
	AnchorLocation = FVector::ZeroVector;

	// Check if there is an active hostile enemy nearby to flee directly away from it
	AActor* ClosestHostile = FindClosestHostile(1500.0f);
	if (ClosestHostile && FollowTarget)
	{
		// Flee away from hostile direction relative to player
		FVector AwayDir = (FollowTarget->GetActorLocation() - ClosestHostile->GetActorLocation()).GetSafeNormal2D();
		if (AwayDir.IsNearlyZero())
		{
			AwayDir = -FollowTarget->GetActorForwardVector();
		}
		// Convert world offset to local player orientation
		const FRotator TargetRot = FollowTarget->GetActorRotation();
		const FVector WorldSafeOffset = (AwayDir * 500.0f) + FVector(0.0f, 0.0f, 180.0f);
		FollowOffset = TargetRot.UnrotateVector(WorldSafeOffset);
	}
	else
	{
		// Standard safe standoff: 5.0 meters behind, elevated safely out of reach
		FollowOffset = FVector(-500.0f, 0.0f, 180.0f);
	}

	SetTemporaryLightColor(FLinearColor(0.4f, 0.8f, 1.0f), 4.0f); // Cautious soft cyan
	if (CompanionLight)
	{
		CompanionLight->SetAttenuationRadius(700.0f);
	}
	OutMessage = TEXT("Falling back to safe distance behind you!");
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(503, 4.0f, FColor::Cyan, TEXT("[Companion]: [RUN] Falling back to safe standoff distance (5m back)."));
	}
	OnSpeechRequested.Broadcast(OutMessage);
	OnActionTriggered.Broadcast(FName(TEXT("SafeDistance")));
	return true;
}

bool ACompanionGuide::ScoutAhead(float Distance)
{
	if (ActionState != ECompanionActionState::None)
	{
		return false;
	}

	const FVector Dir = FollowTarget ? FollowTarget->GetActorForwardVector() : GetActorForwardVector();
	ActionState = ECompanionActionState::Scouting;
	ActionStartLocation = GetActorLocation();
	ActionTargetLocation = ActionStartLocation + (Dir * Distance);
	ActionTimer = 0.0f;
	ActionDuration = 3.2f;

	// Check if enemies exist ahead
	AActor* Threat = FindClosestHostile(Distance + 400.0f);
	if (Threat)
	{
		SetTemporaryLightColor(DangerAlertColor, 3.2f);
		OnSpeechRequested.Broadcast(TEXT("Hostile detected ahead! Scouting carefully."));
	}
	else
	{
		SetTemporaryLightColor(ClearStatusColor, 3.2f);
		OnSpeechRequested.Broadcast(TEXT("Scouting the corridor ahead... path looks clear."));
	}

	OnActionTriggered.Broadcast(FName(TEXT("Scout")));
	return true;
}

bool ACompanionGuide::DistractEnemies(FVector DistractLocation)
{
	if (ActionState != ECompanionActionState::None)
	{
		return false;
	}

	if (DistractLocation.IsZero())
	{
		const FVector Fwd = FollowTarget ? FollowTarget->GetActorForwardVector() : GetActorForwardVector();
		const FVector Right = FollowTarget ? FollowTarget->GetActorRightVector() : GetActorRightVector();
		DistractLocation = GetActorLocation() + (Fwd * 500.0f) + (Right * 450.0f);
	}

	ActionState = ECompanionActionState::Distracting;
	ActionStartLocation = GetActorLocation();
	ActionTargetLocation = DistractLocation;
	ActionTimer = 0.0f;
	ActionDuration = 4.0f;

	// Alert nearby enemies
	if (GetWorld())
	{
		TArray<AActor*> Enemies;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACombatEnemy::StaticClass(), Enemies);
		for (AActor* Act : Enemies)
		{
			if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(Act))
			{
				Damageable->NotifyDanger(DistractLocation, this);
			}
		}
	}

	OnSpeechRequested.Broadcast(TEXT("Drawing their attention away from you!"));
	OnActionTriggered.Broadcast(FName(TEXT("Distract")));
	return true;
}

void ACompanionGuide::SenseDanger(bool& bThreatsDetected, int32& ThreatCount, FString& OutThreatSummary)
{
	ThreatCount = 0;
	if (GetWorld())
	{
		TArray<AActor*> Enemies;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACombatEnemy::StaticClass(), Enemies);
		const FVector MyLoc = GetActorLocation();
		for (AActor* Act : Enemies)
		{
			if (ACombatEnemy* CE = Cast<ACombatEnemy>(Act))
			{
				if (CE->CurrentHP > 0.0f && FVector::Dist(MyLoc, CE->GetActorLocation()) < 1800.0f)
				{
					ThreatCount++;
				}
			}
		}
	}

	bThreatsDetected = (ThreatCount > 0);
	if (bThreatsDetected)
	{
		SetTemporaryLightColor(DangerAlertColor, 3.5f);
		OutThreatSummary = FString::Printf(TEXT("Danger! %d hostile entity(ies) detected lurking nearby!"), ThreatCount);
	}
	else
	{
		SetTemporaryLightColor(ClearStatusColor, 2.5f);
		OutThreatSummary = TEXT("The shadows are quiet. No immediate threats detected.");
	}

	OnSpeechRequested.Broadcast(OutThreatSummary);
	OnActionTriggered.Broadcast(FName(TEXT("SenseDanger")));
}

bool ACompanionGuide::GuideToExit(FString& OutMessage)
{
	ADungeonGenerator* DG = GetDungeonGenerator();
	if (DG && !DG->GetExitDoorLocation().IsZero())
	{
		GuideDestination = DG->GetExitDoorLocation();
		CompanionState = ECompanionState::GuidingToExit;
		SetTemporaryLightColor(ClearStatusColor, 5.0f);
		OutMessage = TEXT("Follow me! Navigating to the catacomb exit.");
		OnSpeechRequested.Broadcast(OutMessage);
		OnActionTriggered.Broadcast(FName(TEXT("GuideToExit")));
		return true;
	}

	OutMessage = TEXT("Cannot locate the catacomb exit. The maze remains locked!");
	OnSpeechRequested.Broadcast(OutMessage);
	return false;
}

bool ACompanionGuide::GuideToBoss(int32 BossIndex, FString& OutMessage)
{
	ADungeonGenerator* DG = GetDungeonGenerator();
	if (!DG)
	{
		OutMessage = TEXT("Dungeon layout unknown.");
		return false;
	}

	FVector TargetDoor = FVector::ZeroVector;
	switch (BossIndex)
	{
	case 1: TargetDoor = DG->Boss1EntranceDoorLocation; break;
	case 2: TargetDoor = DG->Boss2EntranceDoorLocation; break;
	case 3: TargetDoor = DG->Boss3EntranceDoorLocation; break;
	default: TargetDoor = DG->Boss1EntranceDoorLocation; break;
	}

	if (!TargetDoor.IsZero())
	{
		GuideDestination = TargetDoor;
		CompanionState = ECompanionState::GuidingToExit;
		SetTemporaryLightColor(TrapWarningColor, 5.0f);
		OutMessage = FString::Printf(TEXT("Leading the way to Boss %d arena!"), BossIndex);
		OnSpeechRequested.Broadcast(OutMessage);
		OnActionTriggered.Broadcast(FName(TEXT("GuideToBoss")));
		return true;
	}

	OutMessage = FString::Printf(TEXT("Could not locate Boss %d entrance."), BossIndex);
	OnSpeechRequested.Broadcast(OutMessage);
	return false;
}

// -----------------------------------------------------------------------------
// Utility Helpers
// -----------------------------------------------------------------------------

void ACompanionGuide::SetFollowTarget(AActor* NewTarget)
{
	FollowTarget = NewTarget;
}

void ACompanionGuide::SetCompanionState(ECompanionState NewState)
{
	CompanionState = NewState;
}

void ACompanionGuide::SetTemporaryLightColor(FLinearColor NewColor, float Duration)
{
	TemporaryColorTimer = Duration;
	if (CompanionLight)
	{
		CompanionLight->SetLightColor(NewColor);
	}
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(FName(TEXT("Color")), NewColor);
		DynamicMaterial->SetVectorParameterValue(FName(TEXT("BaseColor")), NewColor);
	}
	OnColorChanged.Broadcast(NewColor, Duration);
}

void ACompanionGuide::ResetLightToDefault()
{
	TemporaryColorTimer = 0.0f;
	if (CompanionLight)
	{
		CompanionLight->SetLightColor(DefaultLightColor);
	}
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(FName(TEXT("Color")), DefaultLightColor);
		DynamicMaterial->SetVectorParameterValue(FName(TEXT("BaseColor")), DefaultLightColor);
	}
}

AActor* ACompanionGuide::FindClosestHostile(float SearchRadius) const
{
	if (!GetWorld())
	{
		return nullptr;
	}

	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACombatEnemy::StaticClass(), Enemies);

	AActor* Closest = nullptr;
	float ClosestDistSq = FMath::Square(SearchRadius);
	const FVector MyLoc = GetActorLocation();

	for (AActor* Act : Enemies)
	{
		if (ACombatEnemy* CE = Cast<ACombatEnemy>(Act))
		{
			if (CE->CurrentHP > 0.0f)
			{
				const float DistSq = FVector::DistSquared(MyLoc, CE->GetActorLocation());
				if (DistSq < ClosestDistSq)
				{
					ClosestDistSq = DistSq;
					Closest = CE;
				}
			}
		}
	}

	return Closest;
}

ADungeonGenerator* ACompanionGuide::GetDungeonGenerator() const
{
	if (!GetWorld())
	{
		return nullptr;
	}
	return Cast<ADungeonGenerator>(UGameplayStatics::GetActorOfClass(GetWorld(), ADungeonGenerator::StaticClass()));
}
