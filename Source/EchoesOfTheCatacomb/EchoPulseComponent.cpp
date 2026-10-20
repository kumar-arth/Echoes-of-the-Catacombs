// Copyright Epic Games, Inc. All Rights Reserved.

#include "EchoPulseComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "GameFramework/Actor.h"

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
	StopPulse(); // Reset parameters on level start
}

void UEchoPulseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

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

void UEchoPulseComponent::TriggerPulse(FVector Origin, float InMaxRadius, float InSpeed, FLinearColor InColor)
{
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
