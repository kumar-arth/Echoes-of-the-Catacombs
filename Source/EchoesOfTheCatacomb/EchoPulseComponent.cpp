// Copyright Epic Games, Inc. All Rights Reserved.

#include "EchoPulseComponent.h"
#include "EchoesOfTheCatacombCharacter.h"
#include "CompanionGuide.h"
#include "DungeonGenerator.h"
#include "SpikeTrap.h"
#include "WallDartTrap.h"
#include "BossMinotaur.h"
#include "BossGrux.h"
#include "BossShadowKnight.h"
#include "EngineUtils.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

UEchoPulseComponent::UEchoPulseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Automatically locate MPC_Echo if it exists in Content/Materials
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> MPCFinder(
		TEXT("/Game/Materials/MPC_Echo.MPC_Echo")
	);
	if (MPCFinder.Succeeded())
	{
		EchoMPC = MPCFinder.Object;
	}
}

void UEchoPulseComponent::BeginPlay()
{
	Super::BeginPlay();

	// Ensure EchoMPC is loaded
	if (!EchoMPC)
	{
		EchoMPC = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Materials/MPC_Echo.MPC_Echo"));
	}

	StopPulse(); // Reset parameters on level start
	if (EchoMPC)
	{
		// Maze is lit (dimly lit with torches), keep BossRoomLight at 1.0f so post-process does not black out corridors
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("BossRoomLight")), 1.0f);
		if (GetOwner() && GetWorld())
		{
			const FVector OwnerLoc = GetOwner()->GetActorLocation();
			UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), EchoMPC, FName(TEXT("PlayerLocation")), FLinearColor(OwnerLoc.X, OwnerLoc.Y, OwnerLoc.Z, 1.0f));
			UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), EchoMPC, FName(TEXT("PulseOrigin")), FLinearColor(OwnerLoc.X, OwnerLoc.Y, OwnerLoc.Z, 1.0f));
		}
	}

	// Automatically cache DungeonGenerator if not explicitly assigned
	if (!DungeonGenerator.IsValid() && GetWorld())
	{
		AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), ADungeonGenerator::StaticClass());
		DungeonGenerator = Cast<ADungeonGenerator>(Found);
	}
}

void UEchoPulseComponent::SetPulseEnabled(bool bEnabled)
{
	if (bIsPulseEnabled == bEnabled)
	{
		return;
	}

	bIsPulseEnabled = bEnabled;

	// If disabled while a wave is currently expanding, kill it immediately!
	if (!bIsPulseEnabled && bIsPulseActive)
	{
		StopPulse();
	}

	OnPulseStateChanged.Broadcast(bIsPulseEnabled);
}

void UEchoPulseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Periodic check for boss room boundaries (runs every 0.1s for zero CPU overhead)
	if (bAutoManageBossRoomPulse)
	{
		BossRoomCheckTimer += DeltaTime;
		if (BossRoomCheckTimer >= 0.1f)
		{
			BossRoomCheckTimer = 0.0f;
			CheckBossRoomStatus();
		}
	}

	// Keep maze lit (dimly lit via ambient/torches), keep BossRoomLight at 1.0f so post-process never blacks out corridors
	if (EchoMPC)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("BossRoomLight")), 1.0f);
	}

	// Continuously update live Player Location for post-process character visibility / spotlight sphere mask
	if (EchoMPC && GetOwner() && GetWorld())
	{
		const FVector OwnerLoc = GetOwner()->GetActorLocation();
		UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), EchoMPC, FName(TEXT("PlayerLocation")), FLinearColor(OwnerLoc.X, OwnerLoc.Y, OwnerLoc.Z, 1.0f));
	}

	// Update active enemy pawn highlights
	UpdateHighlightedPawns(DeltaTime);

	if (!bIsPulseActive)
	{
		return;
	}

	// Advance wave radius each frame
	CurrentRadius += ActiveSpeed * DeltaTime;

	if (CurrentRadius >= ActiveMaxRadius)
	{
		StopPulse();
		return;
	}

	UpdateMaterialParameters();

	// Process reveals in distance order as the acoustic wave expands across them
	ProcessSequentialReveals(CurrentRadius);

	// Process acoustic wave reflections off catacomb walls
	if (bEnableWallReflections)
	{
		ProcessWallReflections(CurrentRadius);
	}
}

void UEchoPulseComponent::CheckBossRoomStatus()
{
	if (!DungeonGenerator.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			AActor* Found = UGameplayStatics::GetActorOfClass(World, ADungeonGenerator::StaticClass());
			DungeonGenerator = Cast<ADungeonGenerator>(Found);
		}
	}

	if (!DungeonGenerator.IsValid())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const FVector PlayerLoc = Owner->GetActorLocation();
	const int32 RoomIndex = DungeonGenerator->GetBossRoomAtLocation(PlayerLoc);

	if (RoomIndex > 0)
	{
		// Player is INSIDE a boss arena!
		CurrentBossRoomIndex = RoomIndex;
		LastVisitedBossRoom = RoomIndex;

		if (bIsPulseEnabled)
		{
			SetPulseEnabled(false);

			if (bShowDebugMessages && GEngine)
			{
				const FString Msg = FString::Printf(TEXT("Entered Boss Room %d: Echo Pulse Disabled!"), RoomIndex);
				GEngine->AddOnScreenDebugMessage(1001, 3.0f, FColor::Red, Msg);
			}
		}
	}
	else
	{
		// Player is OUTSIDE the boss arenas (in the maze corridors)
		const bool bWasInside = (CurrentBossRoomIndex > 0);
		CurrentBossRoomIndex = 0;

		if (!bIsPulseEnabled)
		{
			// Check if boss defeat is required to re-enable
			const bool bBossRequirementMet = !bRequireBossDefeatToReenable || DungeonGenerator->IsBossDefeated(LastVisitedBossRoom);

			if (bBossRequirementMet)
			{
				SetPulseEnabled(true);

				if (bWasInside && bShowDebugMessages && GEngine)
				{
					GEngine->AddOnScreenDebugMessage(1001, 3.0f, FColor::Green, TEXT("Returned to Maze: Echo Pulse Enabled!"));
				}
			}
		}
	}
}

void UEchoPulseComponent::TriggerPulse(FVector Origin, float InMaxRadius, float InSpeed, FLinearColor InColor)
{
	if (!bIsPulseEnabled)
	{
		if (bShowDebugMessages && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1002, 1.5f, FColor::Yellow, TEXT("Echolocation is Disabled inside Boss Rooms!"));
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	// Safeguard: ensure origin isn't inside a boss room
	if (CurrentBossRoomIndex > 0 || (DungeonGenerator.IsValid() && DungeonGenerator->IsInsideBossRoom(Origin)))
	{
		if (bShowDebugMessages && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1002, 1.5f, FColor::Yellow, TEXT("Echolocation is Disabled inside Boss Rooms!"));
		}
		return;
	}

	bIsPulseActive = true;
	CurrentOrigin = Origin;
	CurrentRadius = 0.0f;
	ActiveMaxRadius = (InMaxRadius > 0.0f) ? InMaxRadius : DefaultMaxRadius;

	// Trigger the 3-phase acoustic sound wave diaphragm animation on the companion orb
	if (AActor* OwnerActor = GetOwner())
	{
		if (AEchoesOfTheCatacombCharacter* Char = Cast<AEchoesOfTheCatacombCharacter>(OwnerActor))
		{
			if (ACompanionGuide* Guide = Char->GetCompanionInstance())
			{
				Guide->StartSoundWaveAnimation();
			}
		}
	}
	ActiveSpeed = (InSpeed > 0.0f) ? InSpeed : DefaultPulseSpeed;
	ActiveColor = InColor;

	PendingReveals.Reset();
	WallReflections.Reset();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		Owner = UGameplayStatics::GetPlayerPawn(World, 0);
	}

	const float RadiusSq = FMath::Square(ActiveMaxRadius);

	// 1. Gather all Spike Traps within range
	for (TActorIterator<ASpikeTrap> It(World); It; ++It)
	{
		ASpikeTrap* Trap = *It;
		if (IsValid(Trap))
		{
			const float DistSq = FVector::DistSquared(CurrentOrigin, Trap->GetActorLocation());
			if (DistSq <= RadiusSq)
			{
				FEchoPendingHazardReveal Reveal;
				Reveal.TargetActor = Trap;
				Reveal.Location = Trap->GetActorLocation();
				Reveal.Distance = FMath::Sqrt(DistSq);
				Reveal.bIsPawn = false;
				Reveal.bHasRevealed = false;
				PendingReveals.Add(Reveal);
			}
		}
	}

	// 2. Gather all Wall Dart Traps within range
	for (TActorIterator<AWallDartTrap> It(World); It; ++It)
	{
		AWallDartTrap* DartTrap = *It;
		if (IsValid(DartTrap))
		{
			const float DistSq = FVector::DistSquared(CurrentOrigin, DartTrap->GetActorLocation());
			if (DistSq <= RadiusSq)
			{
				FEchoPendingHazardReveal Reveal;
				Reveal.TargetActor = DartTrap;
				Reveal.Location = DartTrap->GetActorLocation();
				Reveal.Distance = FMath::Sqrt(DistSq);
				Reveal.bIsPawn = false;
				Reveal.bHasRevealed = false;
				PendingReveals.Add(Reveal);
			}
		}
	}

	// 3. Gather all non-boss enemy pawns within range
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		if (!IsValid(Pawn) || Pawn == Owner) continue;

		// Strictly exclude all 3 bosses!
		if (Pawn->IsA(ABossMinotaur::StaticClass()) ||
			Pawn->IsA(ABossGrux::StaticClass()) ||
			Pawn->IsA(ABossShadowKnight::StaticClass()))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(CurrentOrigin, Pawn->GetActorLocation());
		if (DistSq <= RadiusSq)
		{
			FEchoPendingHazardReveal Reveal;
			Reveal.TargetActor = Pawn;
			Reveal.Location = Pawn->GetActorLocation();
			Reveal.Distance = FMath::Sqrt(DistSq);
			Reveal.bIsPawn = true;
			Reveal.bHasRevealed = false;
			PendingReveals.Add(Reveal);
		}
	}

	// 4. SORT ASCENDING BY DISTANCE (nearest to player reveals first, furthest reveals last!)
	PendingReveals.Sort([](const FEchoPendingHazardReveal& A, const FEchoPendingHazardReveal& B)
	{
		return A.Distance < B.Distance;
	});

	// 5. Cast radial wall reflection rays (realistic Snell's law acoustic bounces)
	if (bEnableWallReflections && NumReflectionRays > 0)
	{
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(EchoWallTrace), false, Owner);
		const FVector TraceStart = CurrentOrigin + FVector(0.0f, 0.0f, 40.0f);

		for (int32 i = 0; i < NumReflectionRays; ++i)
		{
			const float AngleDeg = (360.0f / (float)NumReflectionRays) * (float)i;
			const FVector RayDir = FRotator(0.0f, AngleDeg, 0.0f).Vector();
			const FVector TraceEnd = TraceStart + (RayDir * ActiveMaxRadius);

			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
			{
				FEchoWallReflection Reflection;
				Reflection.ImpactPoint = Hit.ImpactPoint;
				Reflection.ImpactNormal = Hit.ImpactNormal;
				Reflection.ReflectedDirection = FMath::GetReflectionVector(RayDir, Hit.ImpactNormal);
				Reflection.DistanceFromOrigin = Hit.Distance;
				Reflection.bHasTriggered = false;
				WallReflections.Add(Reflection);
			}
		}
	}

	// Emit acoustic noise to alert enemy AI hearing perception
	if (Owner)
	{
		Owner->MakeNoise(NoiseLoudness, Cast<APawn>(Owner), Origin);
	}

	UpdateMaterialParameters();

	if (bShowDebugMessages && GEngine)
	{
		const FString Msg = FString::Printf(TEXT("ECHOLOCATION PULSE: Expanding acoustic wave (%.0fm, %d hazards tracked)..."),
			ActiveMaxRadius / 100.0f, PendingReveals.Num());
		GEngine->AddOnScreenDebugMessage(1003, 2.5f, FLinearColor(0.15f, 0.85f, 1.0f).ToFColor(true), Msg);
	}
}

void UEchoPulseComponent::TriggerPulseFromOwner()
{
	AActor* Owner = GetOwner();
	if (!Owner && GetWorld())
	{
		Owner = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	}
	if (!Owner)
	{
		return;
	}

	// If owner character has an active companion orb, pulse from the companion with the orb sound wave animation!
	if (AEchoesOfTheCatacombCharacter* Char = Cast<AEchoesOfTheCatacombCharacter>(Owner))
	{
		if (Char->GetCompanionInstance())
		{
			Char->GetCompanionInstance()->TriggerEcholocationPulse();
			return;
		}
	}

	TriggerPulse(Owner->GetActorLocation(), DefaultMaxRadius, DefaultPulseSpeed, DefaultPulseColor);
}

void UEchoPulseComponent::TriggerEcholocationScan()
{
	TriggerPulseFromOwner();
}

void UEchoPulseComponent::StopPulse()
{
	bIsPulseActive = false;
	CurrentRadius = 0.0f;
	PendingReveals.Empty();
	WallReflections.Empty();

	if (EchoMPC && GetWorld())
	{
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("PulseActive")), 0.0f);
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("PulseRadius")), 0.0f);
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("PulseFade")), 0.0f);
	}
}

void UEchoPulseComponent::ProcessSequentialReveals(float CurrentWaveRadius)
{
	for (FEchoPendingHazardReveal& Item : PendingReveals)
	{
		if (!Item.bHasRevealed && CurrentWaveRadius >= Item.Distance)
		{
			Item.bHasRevealed = true;
			RevealTarget(Item);
		}
	}
}

void UEchoPulseComponent::RevealTarget(FEchoPendingHazardReveal& Item)
{
	AActor* Target = Item.TargetActor.Get();
	if (!IsValid(Target))
	{
		return;
	}

	FString TargetName = Target->GetName();

	if (ASpikeTrap* Spike = Cast<ASpikeTrap>(Target))
	{
		Spike->HighlightTrap(HighlightDuration);
		TargetName = TEXT("Spike Trap");
	}
	else if (AWallDartTrap* Dart = Cast<AWallDartTrap>(Target))
	{
		Dart->HighlightTrap(HighlightDuration);
		TargetName = TEXT("Wall Dart Trap");
	}
	else if (APawn* Mob = Cast<APawn>(Target))
	{
		static UMaterialInterface* OverlayMat = nullptr;
		if (!OverlayMat)
		{
			OverlayMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/GeomMaterial.GeomMaterial"));
		}
		UMaterialInstanceDynamic* HighlightMID = nullptr;
		if (OverlayMat)
		{
			HighlightMID = UMaterialInstanceDynamic::Create(OverlayMat, Mob);
			if (HighlightMID)
			{
				HighlightMID->SetVectorParameterValue(FName(TEXT("Color")), ActiveColor);
				HighlightMID->SetScalarParameterValue(FName(TEXT("Opacity")), 0.75f);
			}
		}

		TArray<UMeshComponent*> MeshComponents;
		Mob->GetComponents<UMeshComponent>(MeshComponents);
		for (UMeshComponent* Mesh : MeshComponents)
		{
			if (Mesh)
			{
				Mesh->SetRenderCustomDepth(true);
				Mesh->SetCustomDepthStencilValue(2);
				if (HighlightMID)
				{
					Mesh->SetOverlayMaterial(HighlightMID);
				}
			}
		}

		FEchoHighlightedPawn HighlightedRecord;
		HighlightedRecord.Pawn = Mob;
		HighlightedRecord.TimeRemaining = HighlightDuration;
		ActiveHighlightedPawns.Add(HighlightedRecord);

		TargetName = FString::Printf(TEXT("Enemy (%s)"), *Mob->GetName());
	}

	// 3D Spatial Audio Ping
	if (SonarPingSound && GetWorld())
	{
		const float PitchMultiplier = FMath::Clamp(1.3f - (Item.Distance / ActiveMaxRadius) * 0.5f, 0.7f, 1.4f);
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), SonarPingSound, Item.Location, 1.0f, PitchMultiplier);
	}

	// Visual acoustic ping burst
	if (GetWorld())
	{
		DrawDebugCircle(GetWorld(), Item.Location + FVector(0.0f, 0.0f, 20.0f), 55.0f, 24, FColor::Cyan, false, 0.8f, 0, 2.5f, FVector(0, 0, 1), FVector(1, 0, 0), false);
		DrawDebugPoint(GetWorld(), Item.Location + FVector(0.0f, 0.0f, 40.0f), 12.0f, FColor(50, 220, 255), false, 0.8f);
	}

	if (bShowDebugMessages && GEngine)
	{
		const FString PingMsg = FString::Printf(TEXT("[SONAR PING]: %s pinged at %.1fm!"), *TargetName, Item.Distance / 100.0f);
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(Target), 2.2f, FColor::Cyan, PingMsg);
	}
}

void UEchoPulseComponent::ProcessWallReflections(float CurrentWaveRadius)
{
	for (FEchoWallReflection& Reflection : WallReflections)
	{
		if (!Reflection.bHasTriggered && CurrentWaveRadius >= Reflection.DistanceFromOrigin)
		{
			Reflection.bHasTriggered = true;
			TriggerWallReflection(Reflection);
		}
	}
}

void UEchoPulseComponent::TriggerWallReflection(FEchoWallReflection& Reflection)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector WallImpact = Reflection.ImpactPoint;
	const FVector WallNormal = Reflection.ImpactNormal;
	const FVector ReflectedDir = Reflection.ReflectedDirection;

	// 1. Visual surface ripple: acoustic ring expanding on wall surface normal
	DrawDebugCircle(World, WallImpact + WallNormal * 4.0f, 65.0f, 16, FColor(80, 210, 255), false, 0.5f, 0, 2.0f, WallNormal, FVector::CrossProduct(WallNormal, FVector::UpVector).GetSafeNormal(), false);

	// 2. Play secondary wall echo reverberation SFX if assigned
	if (WallEchoSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, WallEchoSound, WallImpact, 0.6f, 0.9f);
	}

	// 3. Cast secondary reflected ray along ReflectedDir to reveal hazards around corners / behind walls
	const FVector SecStart = WallImpact + (WallNormal * 6.0f);
	const FVector SecEnd = SecStart + (ReflectedDir * ReflectedRayMaxDistance);

	// Draw subtle acoustic reflected trace line
	DrawDebugLine(World, SecStart, SecEnd, FColor(40, 180, 255, 180), false, 0.6f, 0, 1.5f);

	FCollisionQueryParams SecParams(SCENE_QUERY_STAT(EchoWallBounce), false, GetOwner());
	TArray<FHitResult> HitResults;
	World->LineTraceMultiByChannel(HitResults, SecStart, SecEnd, ECC_WorldDynamic, SecParams);

	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor)) continue;

		if (ASpikeTrap* Spike = Cast<ASpikeTrap>(HitActor))
		{
			Spike->HighlightTrap(HighlightDuration);
			if (bShowDebugMessages && GEngine)
			{
				GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(Spike), 2.5f, FColor(100, 230, 255), TEXT("[ECHO BOUNCE]: Hidden Spike Trap detected around corner!"));
			}
		}
		else if (AWallDartTrap* Dart = Cast<AWallDartTrap>(HitActor))
		{
			Dart->HighlightTrap(HighlightDuration);
			if (bShowDebugMessages && GEngine)
			{
				GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(Dart), 2.5f, FColor(100, 230, 255), TEXT("[ECHO BOUNCE]: Hidden Wall Dart Trap detected around corner!"));
			}
		}
	}

	// Make subtle acoustic noise at bounce location for enemy AI hearing
	if (AActor* Owner = GetOwner())
	{
		Owner->MakeNoise(NoiseLoudness * 0.45f, Cast<APawn>(Owner), WallImpact);
	}
}

void UEchoPulseComponent::UpdateHighlightedPawns(float DeltaTime)
{
	for (int32 i = ActiveHighlightedPawns.Num() - 1; i >= 0; --i)
	{
		FEchoHighlightedPawn& Record = ActiveHighlightedPawns[i];
		Record.TimeRemaining -= DeltaTime;

		if (Record.TimeRemaining <= 0.0f || !Record.Pawn.IsValid())
		{
			if (Record.Pawn.IsValid())
			{
				TArray<UMeshComponent*> MeshComponents;
				Record.Pawn->GetComponents<UMeshComponent>(MeshComponents);
				for (UMeshComponent* Mesh : MeshComponents)
				{
					if (Mesh)
					{
						Mesh->SetRenderCustomDepth(false);
						Mesh->SetOverlayMaterial(nullptr);
					}
				}
			}
			ActiveHighlightedPawns.RemoveAt(i);
		}
	}
}

void UEchoPulseComponent::UpdateMaterialParameters()
{
	if (!EchoMPC || !GetWorld())
	{
		return;
	}

	// Calculate smooth fade factor (1.0 at origin -> 0.0 at MaxRadius)
	const float Fade = FMath::Clamp(1.0f - (CurrentRadius / ActiveMaxRadius), 0.0f, 1.0f);

	UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("PulseActive")), 1.0f);
	UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("PulseRadius")), CurrentRadius);
	UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("PulseBandwidth")), PulseBandwidth);
	UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("PulseFade")), Fade);
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), EchoMPC, FName(TEXT("PulseOrigin")), FLinearColor(CurrentOrigin.X, CurrentOrigin.Y, CurrentOrigin.Z, 1.0f));
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), EchoMPC, FName(TEXT("PulseColor")), ActiveColor);
}
