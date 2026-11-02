// Copyright Epic Games, Inc. All Rights Reserved.

#include "EchoPulseComponent.h"
#include "DungeonGenerator.h"
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
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("BossRoomLight")), 0.0f);
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

	// Dynamic Boss Room illumination transition (rises to 1.0 on enter, lowers to 0.0 in maze)
	const float TargetBossLight = (CurrentBossRoomIndex > 0) ? 1.0f : 0.0f;
	static float CurrentBossLight = 0.0f;
	if (FMath::Abs(CurrentBossLight - TargetBossLight) > 0.001f)
	{
		CurrentBossLight = FMath::FInterpTo(CurrentBossLight, TargetBossLight, DeltaTime, 4.0f);
		if (EchoMPC)
		{
			UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("BossRoomLight")), CurrentBossLight);
		}
	}
	else if (CurrentBossLight != TargetBossLight)
	{
		CurrentBossLight = TargetBossLight;
		if (EchoMPC)
		{
			UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), EchoMPC, FName(TEXT("BossRoomLight")), CurrentBossLight);
		}
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

void UEchoPulseComponent::TriggerPulse(FVector Origin, float InMaxRadius, float InSpeed, FLinearColor InColor)
{
	if (!bIsPulseEnabled)
	{
		if (bShowDebugMessages && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1002, 1.5f, FColor::Yellow, TEXT("Echo Pulse is Disabled inside Boss Room!"));
		}
		return;
	}

	// Safeguard: even if pulse was enabled, ensure origin isn't inside a boss room
	if (DungeonGenerator.IsValid() && DungeonGenerator->IsInsideBossRoom(Origin))
	{
		return;
	}

	bIsPulseActive = true;
	CurrentOrigin = Origin;
	CurrentRadius = 0.0f;
	ActiveMaxRadius = (InMaxRadius > 0.0f) ? InMaxRadius : DefaultMaxRadius;
	ActiveSpeed = (InSpeed > 0.0f) ? InSpeed : DefaultPulseSpeed;
	ActiveColor = InColor;

	UpdateMaterialParameters();

	// Emit acoustic noise event to alert enemy AI hearing perception
	AActor* Owner = GetOwner();
	if (Owner)
	{
		Owner->MakeNoise(NoiseLoudness, Cast<APawn>(Owner), CurrentOrigin);
	}
}

void UEchoPulseComponent::TriggerPulseFromOwner()
{
	AActor* Owner = GetOwner();
	if (Owner)
	{
		TriggerPulse(Owner->GetActorLocation(), DefaultMaxRadius, DefaultPulseSpeed, DefaultPulseColor);
	}
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