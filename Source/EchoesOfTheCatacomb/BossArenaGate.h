// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossArenaGate.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USoundBase;

UCLASS()
class ECHOESOFTHECATACOMB_API ABossArenaGate : public AActor
{
	GENERATED_BODY()

public:
	ABossArenaGate();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	/** Arched doorway frame */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* FrameMesh;

	/** Heavy iron cell door / portcullis that seals the passage */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* GateMesh;

	/** Player entry detection box on the room side of the entrance */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* TriggerBox;

	/** If true, this is the room entrance gate that slams shut behind player */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate")
	bool bIsEntranceGate = true;

	/** Current open state */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gate")
	bool bIsOpen = true;

	/** Z offset when gate is raised up open */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate")
	float OpenZOffset = 260.0f;

	/** Target relative Z */
	float TargetZ = 0.0f;
	float CurrentZ = 0.0f;

	/** Gate drop sound */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate|Audio")
	USoundBase* GateSound;

	UFUNCTION(BlueprintCallable, Category = "Gate")
	void CloseGate();

	UFUNCTION(BlueprintCallable, Category = "Gate")
	void OpenGate();

	UFUNCTION()
	void OnTriggerOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                           UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                           bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY()
	TWeakObjectPtr<class ABossMinotaur> LinkedBoss;
};

