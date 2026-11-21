// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossArenaGate.generated.h"

class UStaticMeshComponent;
class USceneComponent;
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

	/** Arched stone doorway frame matching dungeon walls */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* FrameMesh;

	/** Left door hinge pivot (at left side of doorway frame) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* HingeLeft;

	/** Heavy wooden left door with iron straps */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* DoorLeftMesh;

	/** Right door hinge pivot (at right side of doorway frame) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* HingeRight;

	/** Heavy wooden right door with iron straps */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* DoorRightMesh;

	/** Detects player approaching from corridor to swing doors open */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* ApproachTrigger;

	/** Detects player stepping into the arena to slam doors shut and engage boss */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* ArenaEntryTrigger;

	/** If true, this is an entrance gate; otherwise exit gate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate")
	bool bIsEntranceGate = true;

	/** Current open state */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gate")
	bool bIsOpen = false;

	/** True once player has entered and entrance gate is permanently locked shut */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gate")
	bool bHasSealed = false;

	/** Target door rotation angles */
	float TargetLeftYaw = 0.0f;
	float TargetRightYaw = 0.0f;
	float CurrentLeftYaw = 0.0f;
	float CurrentRightYaw = 0.0f;

	/** Door slam and creak sounds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate|Audio")
	USoundBase* SlamSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate|Audio")
	USoundBase* CreakSound;

	UFUNCTION(BlueprintCallable, Category = "Gate")
	void CloseGate();

	UFUNCTION(BlueprintCallable, Category = "Gate")
	void OpenGate();

	UFUNCTION()
	void OnApproachOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                           UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                           bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnArenaEntryOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                             UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                             bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY()
	TWeakObjectPtr<class ABossMinotaur> LinkedBoss;

	UPROPERTY()
	TWeakObjectPtr<class ABossGrux> LinkedBossGrux;

	UPROPERTY()
	TWeakObjectPtr<class ABossShadowKnight> LinkedBossShadowKnight;
};
