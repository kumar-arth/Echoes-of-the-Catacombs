// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "EchoPulseComponent.generated.h"

UCLASS(ClassGroup = (Echoes), meta = (BlueprintSpawnableComponent))
class ECHOESOFTHECATACOMB_API UEchoPulseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEchoPulseComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Triggers an expanding pulse from a specific world location */
	UFUNCTION(BlueprintCallable, Category = "Echo|Pulse")
	void TriggerPulse(FVector Origin, float InMaxRadius = 3500.0f, float InSpeed = 1800.0f, FLinearColor InColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f));

	/** Convenience function: pulses outward from the owning player/companion location */
	UFUNCTION(BlueprintCallable, Category = "Echo|Pulse")
	void TriggerPulseFromOwner();

	/** Stops and resets the pulse */
	UFUNCTION(BlueprintCallable, Category = "Echo|Pulse")
	void StopPulse();

	/** Check if a pulse wave is currently active */
	UFUNCTION(BlueprintPure, Category = "Echo|Pulse")
	bool IsPulseActive() const { return bIsPulseActive; }

public:
	// ========================================================================
	// PARAMETERS
	// ========================================================================

	/** Reference to the MPC_Echo asset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Shader")
	UMaterialParameterCollection* EchoMPC;

	/** Wave propagation speed (cm/s). 1800 = 18 meters/second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Parameters")
	float DefaultPulseSpeed = 1800.0f;

	/** Maximum distance the wave travels before fading out */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Parameters")
	float DefaultMaxRadius = 3500.0f;

	/** Thickness of the visual pulse ring band */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Parameters")
	float PulseBandwidth = 250.0f;

	/** Default glowing color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Parameters")
	FLinearColor DefaultPulseColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f);

	/** Noise loudness to alert AI perception (0.0 to 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Acoustics")
	float NoiseLoudness = 0.85f;

private:
	bool bIsPulseActive = false;
	FVector CurrentOrigin = FVector::ZeroVector;
	float CurrentRadius = 0.0f;
	float ActiveMaxRadius = 3500.0f;
	float ActiveSpeed = 1800.0f;
	FLinearColor ActiveColor;

	void UpdateMaterialParameters();
};
