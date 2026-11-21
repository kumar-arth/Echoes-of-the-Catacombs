// Copyright Epic Games, Inc. All Rights Reserved.

#include "EchoPulseComponent.h"
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
		// Maze is now lit (dimly lit with torches), keep BossRoomLight at 1.0f so post-process does not black out corridors
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

void UEchoPulseComponent::TriggerEcholocationScan()
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

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		Owner = UGameplayStatics::GetPlayerPawn(World, 0);
	}
	if (!Owner) return;

	const FVector Origin = Owner->GetActorLocation();

	// Safeguard: ensure origin isn't inside a boss room
	if (CurrentBossRoomIndex > 0 || (DungeonGenerator.IsValid() && DungeonGenerator->IsInsideBossRoom(Origin)))
	{
		if (bShowDebugMessages && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1002, 1.5f, FColor::Yellow, TEXT("Echolocation is Disabled inside Boss Rooms!"));
		}
		return;
	}

	const float RadiusSq = FMath::Square(ScanRadius);
	int32 RevealedHazards = 0;

	// 1. Scan and highlight all Spike Traps in range
	for (TActorIterator<ASpikeTrap> It(World); It; ++It)
	{
		ASpikeTrap* Trap = *It;
		if (IsValid(Trap) && FVector::DistSquared(Origin, Trap->GetActorLocation()) <= RadiusSq)
		{
			Trap->HighlightTrap(HighlightDuration);
			RevealedHazards++;
		}
	}

	// 2. Scan and highlight all Wall Dart Traps in range
	for (TActorIterator<AWallDartTrap> It(World); It; ++It)
	{
		AWallDartTrap* DartTrap = *It;
		if (IsValid(DartTrap) && FVector::DistSquared(Origin, DartTrap->GetActorLocation()) <= RadiusSq)
		{
			DartTrap->HighlightTrap(HighlightDuration);
			RevealedHazards++;
		}
	}

	// 3. Scan and highlight all non-boss enemy pawns (mobs) in range
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

		if (FVector::DistSquared(Origin, Pawn->GetActorLocation()) <= RadiusSq)
		{
			static UMaterialInterface* OverlayMat = nullptr;
			if (!OverlayMat)
			{
				OverlayMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/GeomMaterial.GeomMaterial"));
			}
			UMaterialInstanceDynamic* HighlightMID = nullptr;
			if (OverlayMat)
			{
				HighlightMID = UMaterialInstanceDynamic::Create(OverlayMat, Pawn);
				if (HighlightMID)
				{
					HighlightMID->SetVectorParameterValue(FName(TEXT("Color")), DefaultPulseColor);
					HighlightMID->SetScalarParameterValue(FName(TEXT("Opacity")), 0.75f);
				}
			}

			TArray<UMeshComponent*> MeshComponents;
			Pawn->GetComponents<UMeshComponent>(MeshComponents);
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
			RevealedHazards++;
		}
	}

	// Emit acoustic noise event to alert enemy AI hearing perception
	Owner->MakeNoise(NoiseLoudness, Cast<APawn>(Owner), Origin);

	if (bShowDebugMessages && GEngine)
	{
		const FString Msg = FString::Printf(TEXT("ECHOLOCATION [F]: %d Hazard(s) Revealed through walls (%.0fm range)!"), RevealedHazards, ScanRadius / 100.0f);
		GEngine->AddOnScreenDebugMessage(1003, 2.5f, FLinearColor(0.15f, 0.85f, 1.0f).ToFColor(true), Msg);
	}
}

void UEchoPulseComponent::TriggerPulse(FVector Origin, float InMaxRadius, float InSpeed, FLinearColor InColor)
{
	TriggerEcholocationScan();
}

void UEchoPulseComponent::TriggerPulseFromOwner()
{
	TriggerEcholocationScan();
}

void UEchoPulseComponent::StopPulse()
{
	bIsPulseActive = false;
	CurrentRadius = 0.0f;

	if (EchoMPC && GetWorld())
	{
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("PulseActive")), 0.0f);
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("PulseRadius")), 0.0f);
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("PulseFade")), 0.0f);
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