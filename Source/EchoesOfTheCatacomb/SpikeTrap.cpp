// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpikeTrap.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

ASpikeTrap::ASpikeTrap()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// 1. Root scene component (centered at grid cell)
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 2. Base Plate (Small 100x100 cm stone pressure plate embedded flush in the floor)
	BasePlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BasePlateMesh"));
	BasePlateMesh->SetupAttachment(RootComponent);
	BasePlateMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.2f));
	BasePlateMesh->SetRelativeRotation(FRotator::ZeroRotator);
	BasePlateMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.015f));
	BasePlateMesh->SetCollisionProfileName(TEXT("BlockAll"));
	BasePlateMesh->bCastDynamicShadow = true;
	BasePlateMesh->CastShadow = true;

	// 3. Spikes Container (moves along Z axis)
	SpikesContainer = CreateDefaultSubobject<USceneComponent>(TEXT("SpikesContainer"));
	SpikesContainer->SetupAttachment(RootComponent);
	SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, SpikeRetractedZ));

	// 4. Instanced Spikes Mesh (sharp dark steel spikes)
	SpikesISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SpikesISM"));
	SpikesISM->SetupAttachment(SpikesContainer);
	SpikesISM->SetCollisionProfileName(TEXT("NoCollision"));
	SpikesISM->SetGenerateOverlapEvents(false);
	SpikesISM->bCastDynamicShadow = true;
	SpikesISM->CastShadow = true;

	// 5. Trigger Box (compactly covering the 100x100 cm pressure plate)
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(RootComponent);
	TriggerBox->SetBoxExtent(FVector(50.0f, 50.0f, 30.0f));
	TriggerBox->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->SetGenerateOverlapEvents(true);

	// 6. Damage Box (active when spikes are sprung)
	DamageBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageBox"));
	DamageBox->SetupAttachment(SpikesContainer);
	DamageBox->SetBoxExtent(FVector(45.0f, 45.0f, 30.0f));
	DamageBox->SetRelativeLocation(FVector(0.0f, 0.0f, 15.0f));
	DamageBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	DamageBox->SetGenerateOverlapEvents(true);
	DamageBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Basic Shapes & Materials
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		BasePlateMesh->SetStaticMesh(CubeMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshFinder(
		TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMeshFinder.Succeeded())
	{
		SpikesISM->SetStaticMesh(ConeMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FloorMatFinder(
		TEXT("/Game/MedievalDungeon/Materials/M_BrickFloor_Clean_Inst.M_BrickFloor_Clean_Inst"));
	if (FloorMatFinder.Succeeded())
	{
		BasePlateMesh->SetMaterial(0, FloorMatFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> IronMatFinder(
		TEXT("/Game/MedievalDungeon/Materials/M_Metal_Decor_Inst.M_Metal_Decor_Inst"));
	if (IronMatFinder.Succeeded())
	{
		SpikesISM->SetMaterial(0, IronMatFinder.Object);
	}

	// Populate compact 3x3 spike grid (spaced 30 cm apart inside the 100x100 cm plate)
	const float GridSpacing = 30.0f;
	const FVector SpikeScale(0.08f, 0.08f, 0.45f); // Sharp forged spikes
	for (int32 X = -1; X <= 1; ++X)
	{
		for (int32 Y = -1; Y <= 1; ++Y)
		{
			FTransform SpikeTransform(FRotator::ZeroRotator, FVector(X * GridSpacing, Y * GridSpacing, 0.0f), SpikeScale);
			SpikesISM->AddInstance(SpikeTransform);
		}
	}

	CurrentPlateZ = 0.2f;
	CurrentSpikeZ = SpikeRetractedZ;
	bIsPeriodic = false; // Pure pressure-plate by default!
	Tags.AddUnique(TEXT("DungeonTrap"));
}

void ASpikeTrap::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ASpikeTrap::OnTriggerOverlapBegin);
	DamageBox->OnComponentBeginOverlap.AddDynamic(this, &ASpikeTrap::OnDamageOverlapBegin);

	CurrentPlateZ = 0.2f;
	CurrentSpikeZ = SpikeRetractedZ;
	BasePlateMesh->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentPlateZ));
	SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentSpikeZ));
	TrapState = ESpikeTrapState::Dormant;

	// Scale the material tiling so the brick size on the 100cm plate matches the 500cm floor bricks!
	if (BasePlateMesh)
	{
		UMaterialInterface* FloorMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MedievalDungeon/Materials/M_BrickFloor_Clean_Inst.M_BrickFloor_Clean_Inst"));
		if (FloorMat)
		{
			UMaterialInstanceDynamic* PlateMID = UMaterialInstanceDynamic::Create(FloorMat, this);
			if (PlateMID)
			{
				PlateMID->SetScalarParameterValue(FName(TEXT("Tiling")), 0.20f);
				BasePlateMesh->SetMaterial(0, PlateMID);
			}
		}
	}
}

void ASpikeTrap::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (TrapState)
	{
	case ESpikeTrapState::Dormant:
		CurrentPlateZ = FMath::FInterpConstantTo(CurrentPlateZ, 0.2f, DeltaTime, PlateRiseSpeed);
		BasePlateMesh->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentPlateZ));
		if (bIsPeriodic)
		{
			PeriodicTimer += DeltaTime;
			if (PeriodicTimer >= PeriodicCycleInterval)
			{
				PeriodicTimer = 0.0f;
				TriggerTrap();
			}
		}
		break;

	case ESpikeTrapState::Warning:
		StateTimer += DeltaTime;
		// Stone pressure plate presses DOWN into the floor when stepped on!
		CurrentPlateZ = FMath::FInterpConstantTo(CurrentPlateZ, 0.2f - PlateSinkDepth, DeltaTime, PlateSinkSpeed);
		BasePlateMesh->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentPlateZ));

		// Subtle pre-spring vibration jitter of the spikes container beneath
		{
			const float Jitter = FMath::Sin(StateTimer * 70.0f) * 1.5f;
			SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, SpikeRetractedZ + Jitter));
		}
		if (StateTimer >= WarningDelay)
		{
			StateTimer = 0.0f;
			SpringSpikes();
		}
		break;

	case ESpikeTrapState::Springing:
		CurrentSpikeZ = FMath::FInterpConstantTo(CurrentSpikeZ, SpikeExtendedZ, DeltaTime, SpringSpeed);
		SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentSpikeZ));
		if (FMath::IsNearlyEqual(CurrentSpikeZ, SpikeExtendedZ, 1.0f))
		{
			CurrentSpikeZ = SpikeExtendedZ;
			SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentSpikeZ));
			TrapState = ESpikeTrapState::Sprung;
			StateTimer = 0.0f;

			// Enable damage box and hurt overlapping pawns
			DamageBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			TArray<AActor*> OverlappingActors;
			DamageBox->GetOverlappingActors(OverlappingActors);
			for (AActor* Overlapped : OverlappingActors)
			{
				ApplyDamageToActor(Overlapped);
			}
		}
		break;

	case ESpikeTrapState::Sprung:
		StateTimer += DeltaTime;
		if (StateTimer >= SprungDuration)
		{
			StateTimer = 0.0f;
			TrapState = ESpikeTrapState::Retracting;
			DamageBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			DamagedActorsThisCycle.Empty();
		}
		break;

	case ESpikeTrapState::Retracting:
		CurrentSpikeZ = FMath::FInterpConstantTo(CurrentSpikeZ, SpikeRetractedZ, DeltaTime, RetractSpeed);
		SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentSpikeZ));
		CurrentPlateZ = FMath::FInterpConstantTo(CurrentPlateZ, 0.2f, DeltaTime, PlateRiseSpeed);
		BasePlateMesh->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentPlateZ));
		if (FMath::IsNearlyEqual(CurrentSpikeZ, SpikeRetractedZ, 1.0f) && FMath::IsNearlyEqual(CurrentPlateZ, 0.2f, 0.1f))
		{
			CurrentSpikeZ = SpikeRetractedZ;
			CurrentPlateZ = 0.2f;
			SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentSpikeZ));
			BasePlateMesh->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentPlateZ));
			TrapState = ESpikeTrapState::Dormant;
			PeriodicTimer = 0.0f;
		}
		break;
	}
}

void ASpikeTrap::OnTriggerOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                      bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;

	// Only trigger when a pawn (Player or Enemy) steps on the plate
	if (Cast<APawn>(OtherActor))
	{
		if (TrapState == ESpikeTrapState::Dormant)
		{
			TriggerTrap();
		}
		else if (TrapState == ESpikeTrapState::Sprung)
		{
			ApplyDamageToActor(OtherActor);
		}
	}
}

void ASpikeTrap::OnDamageOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                     UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                     bool bFromSweep, const FHitResult& SweepResult)
{
	if (TrapState == ESpikeTrapState::Sprung)
	{
		ApplyDamageToActor(OtherActor);
	}
}

void ASpikeTrap::TriggerTrap()
{
	if (TrapState != ESpikeTrapState::Dormant) return;

	TrapState = ESpikeTrapState::Warning;
	StateTimer = 0.0f;
	DamagedActorsThisCycle.Empty();
}

void ASpikeTrap::SpringSpikes()
{
	TrapState = ESpikeTrapState::Springing;
	DamagedActorsThisCycle.Empty();
}

void ASpikeTrap::ResetTrap()
{
	TrapState = ESpikeTrapState::Dormant;
	CurrentSpikeZ = SpikeRetractedZ;
	CurrentPlateZ = 0.2f;
	SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentSpikeZ));
	if (BasePlateMesh)
	{
		BasePlateMesh->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentPlateZ));
	}
	DamageBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DamagedActorsThisCycle.Empty();
	PeriodicTimer = 0.0f;
	StateTimer = 0.0f;
}

void ASpikeTrap::HighlightTrap(float Duration)
{
	static UMaterialInterface* OverlayMat = nullptr;
	if (!OverlayMat)
	{
		OverlayMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/GeomMaterial.GeomMaterial"));
	}
	UMaterialInstanceDynamic* HighlightMID = nullptr;
	if (OverlayMat)
	{
		HighlightMID = UMaterialInstanceDynamic::Create(OverlayMat, this);
		if (HighlightMID)
		{
			HighlightMID->SetVectorParameterValue(FName(TEXT("Color")), FLinearColor(0.15f, 0.85f, 1.0f, 1.0f));
			HighlightMID->SetScalarParameterValue(FName(TEXT("Opacity")), 0.75f);
		}
	}

	if (BasePlateMesh)
	{
		BasePlateMesh->SetRenderCustomDepth(true);
		BasePlateMesh->SetCustomDepthStencilValue(2);
		if (HighlightMID)
		{
			BasePlateMesh->SetOverlayMaterial(HighlightMID);
		}
	}
	if (SpikesISM)
	{
		SpikesISM->SetRenderCustomDepth(true);
		SpikesISM->SetCustomDepthStencilValue(2);
	}

	GetWorldTimerManager().ClearTimer(HighlightTimerHandle);
	GetWorldTimerManager().SetTimer(HighlightTimerHandle, this, &ASpikeTrap::OnHighlightExpired, Duration, false);
}

void ASpikeTrap::OnHighlightExpired()
{
	if (BasePlateMesh)
	{
		BasePlateMesh->SetRenderCustomDepth(false);
		BasePlateMesh->SetOverlayMaterial(nullptr);
	}
	if (SpikesISM)
	{
		SpikesISM->SetRenderCustomDepth(false);
	}
}

void ASpikeTrap::ApplyDamageToActor(AActor* TargetActor)
{
	if (!TargetActor || TargetActor == this || DamagedActorsThisCycle.Contains(TargetActor))
	{
		return;
	}

	DamagedActorsThisCycle.Add(TargetActor);

	// Apply gameplay damage
	const float ActualDamage = UGameplayStatics::ApplyDamage(TargetActor, DamageAmount, nullptr, this, UDamageType::StaticClass());

	// If it's a character, apply physical jolt/knockback only if damage was actually taken (not blocked by shield)
	if (ACharacter* Char = Cast<ACharacter>(TargetActor))
	{
		if (ActualDamage > 0.0f)
		{
			Char->LaunchCharacter(FVector(0.0f, 0.0f, LaunchImpulseZ), false, true);
		}
	}
}
