// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallDartTrap.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DartProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

AWallDartTrap::AWallDartTrap()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. Root scene component
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	const float HalfSpan = CorridorSpan * 0.5f; // 250.0f for 500cm corridor

	// 2. Wall Dispenser (embedded recessed inside negative X wall at tripwire height)
	DispenserMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DispenserMesh"));
	DispenserMesh->SetupAttachment(RootComponent);
	DispenserMesh->SetRelativeLocation(FVector(-HalfSpan - 3.0f, 0.0f, TripwireHeight));
	DispenserMesh->SetRelativeScale3D(FVector(0.08f, 0.22f, 0.22f));
	DispenserMesh->SetCollisionProfileName(TEXT("NoCollision"));

	// 3. Wall Receiver (embedded recessed inside positive X wall at tripwire height)
	ReceiverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReceiverMesh"));
	ReceiverMesh->SetupAttachment(RootComponent);
	ReceiverMesh->SetRelativeLocation(FVector(HalfSpan + 3.0f, 0.0f, TripwireHeight));
	ReceiverMesh->SetRelativeScale3D(FVector(0.06f, 0.18f, 0.18f));
	ReceiverMesh->SetCollisionProfileName(TEXT("NoCollision"));

	// 4. Tripwire Mesh (thin 2.5mm cord stretched cleanly between dispenser and receiver at tripwire height)
	TripwireMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TripwireMesh"));
	TripwireMesh->SetupAttachment(RootComponent);
	TripwireMesh->SetRelativeLocation(FVector(0.0f, 0.0f, TripwireHeight));
	TripwireMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	TripwireMesh->SetRelativeScale3D(FVector(0.0025f, 0.0025f, 5.08f));
	TripwireMesh->SetCollisionProfileName(TEXT("NoCollision"));

	// 5. Tripwire Trigger Box (spans across the alley at tripwire height)
	TripwireBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TripwireBox"));
	TripwireBox->SetupAttachment(RootComponent);
	TripwireBox->SetRelativeLocation(FVector(0.0f, 0.0f, TripwireHeight));
	TripwireBox->SetBoxExtent(FVector(245.0f, 25.0f, 25.0f));
	TripwireBox->SetCollisionProfileName(TEXT("Trigger"));
	TripwireBox->SetGenerateOverlapEvents(true);

	// Find default engine shapes & dungeon materials
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		DispenserMesh->SetStaticMesh(CubeMeshFinder.Object);
		ReceiverMesh->SetStaticMesh(CubeMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		TripwireMesh->SetStaticMesh(CylinderFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StoneMatFinder(
		TEXT("/Game/MedievalDungeon/Materials/M_StoneWall_Inst.M_StoneWall_Inst"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> IronMatFinder(
		TEXT("/Game/MedievalDungeon/Materials/M_Metal_Decor_Inst.M_Metal_Decor_Inst"));

	if (StoneMatFinder.Succeeded())
	{
		DispenserMesh->SetMaterial(0, StoneMatFinder.Object);
		ReceiverMesh->SetMaterial(0, StoneMatFinder.Object);
	}
	else if (IronMatFinder.Succeeded())
	{
		DispenserMesh->SetMaterial(0, IronMatFinder.Object);
		ReceiverMesh->SetMaterial(0, IronMatFinder.Object);
	}

	if (IronMatFinder.Succeeded())
	{
		TripwireMesh->SetMaterial(0, IronMatFinder.Object);
	}

	// Mechanical trip sound
	static ConstructorHelpers::FObjectFinder<USoundBase> ClickSoundFinder(
		TEXT("/Engine/VREditor/Sounds/VR_click1_Cue.VR_click1_Cue"));
	if (ClickSoundFinder.Succeeded())
	{
		TriggerSound = ClickSoundFinder.Object;
	}

	// High speed whoosh / volley sound
	static ConstructorHelpers::FObjectFinder<USoundBase> WhooshSoundFinder(
		TEXT("/Game/ParagonGrux/Characters/Heroes/Grux/Sounds/SoundCues/Grux_Effort_Swing.Grux_Effort_Swing"));
	if (WhooshSoundFinder.Succeeded())
	{
		FireSound = WhooshSoundFinder.Object;
	}

	DartProjectileClass = ADartProjectile::StaticClass();
	Tags.AddUnique(TEXT("DungeonTrap"));
}

void AWallDartTrap::BeginPlay()
{
	Super::BeginPlay();

	const float HalfSpan = CorridorSpan * 0.5f;
	if (DispenserMesh)
	{
		DispenserMesh->SetRelativeLocation(FVector(-HalfSpan - 3.0f, 0.0f, TripwireHeight));
	}
	if (ReceiverMesh)
	{
		ReceiverMesh->SetRelativeLocation(FVector(HalfSpan + 3.0f, 0.0f, TripwireHeight));
	}
	if (TripwireMesh)
	{
		TripwireMesh->SetRelativeLocation(FVector(0.0f, 0.0f, TripwireHeight));
		TripwireMesh->SetRelativeScale3D(FVector(0.0025f, 0.0025f, (CorridorSpan + 8.0f) * 0.01f));
	}
	if (TripwireBox)
	{
		TripwireBox->SetRelativeLocation(FVector(0.0f, 0.0f, TripwireHeight));
		TripwireBox->SetBoxExtent(FVector(FMath::Max(10.0f, HalfSpan - 5.0f), 25.0f, 25.0f));
	}

	TripwireBox->OnComponentBeginOverlap.AddDynamic(this, &AWallDartTrap::OnTriggerOverlapBegin);
	TrapState = EDartTrapState::Armed;
	TripwireMesh->SetVisibility(true);
}

void AWallDartTrap::OnTriggerOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                         UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                         bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;

	if (Cast<APawn>(OtherActor) && TrapState == EDartTrapState::Armed)
	{
		TriggerTrap();
	}
}

void AWallDartTrap::TriggerTrap()
{
	if (TrapState != EDartTrapState::Armed) return;

	TrapState = EDartTrapState::Triggered;

	// Tripwire visually snaps
	if (TripwireMesh)
	{
		TripwireMesh->SetVisibility(false);
	}

	// Play click/snap sound
	if (TriggerSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, TriggerSound, GetActorLocation() + FVector(0.0f, 0.0f, TripwireHeight));
	}

	// Instant fire response
	if (FireDelay <= 0.01f)
	{
		FireDartVolley();
	}
	else
	{
		GetWorldTimerManager().SetTimer(FireTimerHandle, this, &AWallDartTrap::FireDartVolley, FireDelay, false);
	}
}

void AWallDartTrap::FireDartVolley()
{
	UWorld* World = GetWorld();
	if (!World) return;

	TSubclassOf<ADartProjectile> ProjectileClass =
		DartProjectileClass ? DartProjectileClass : TSubclassOf<ADartProjectile>(ADartProjectile::StaticClass());

	const FVector ForwardDir = GetActorForwardVector(); // Shoots from -X wall toward +X wall
	const FVector RightDir = GetActorRightVector();     // Corridor direction (along hallway)
	const FRotator SpawnRot = ForwardDir.Rotation();

	const float HalfSpan = CorridorSpan * 0.5f;
	// Spawn safely 35 cm out from dispenser face into the open corridor
	const FVector BaseSpawnLoc = GetActorLocation() + (ForwardDir * (-HalfSpan + 35.0f)) + FVector(0.0f, 0.0f, TripwireHeight);

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, BaseSpawnLoc);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();

	// Wave 1: Immediate spread across the hallway path to catch player who tripped wire
	const float Wave1Offsets[] = { -20.0f, 0.0f, 25.0f, 50.0f };
	for (int32 i = 0; i < 4; ++i)
	{
		const float YOffset = Wave1Offsets[i];
		const float ZOffset = (i % 2 == 0) ? -6.0f : 6.0f;
		const FVector DartLoc = BaseSpawnLoc + (RightDir * YOffset) + FVector(0.0f, 0.0f, ZOffset);

		ADartProjectile* Dart = World->SpawnActor<ADartProjectile>(ProjectileClass, DartLoc, SpawnRot, SpawnParams);
		if (Dart && Dart->CollisionComp)
		{
			Dart->CollisionComp->IgnoreActorWhenMoving(this, true);
		}
	}

	// Wave 2: Staggered 0.08s later lead-aimed down the hallway to catch sprinting players
	GetWorldTimerManager().SetTimer(FollowupTimerHandle, this, &AWallDartTrap::FireFollowupVolley, 0.08f, false);

	// Begin reload cycle
	TrapState = EDartTrapState::Reloading;
	GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &AWallDartTrap::ResetTrap, ReloadTime, false);
}

void AWallDartTrap::FireFollowupVolley()
{
	UWorld* World = GetWorld();
	if (!World) return;

	TSubclassOf<ADartProjectile> ProjectileClass =
		DartProjectileClass ? DartProjectileClass : TSubclassOf<ADartProjectile>(ADartProjectile::StaticClass());

	const FVector ForwardDir = GetActorForwardVector();
	const FVector RightDir = GetActorRightVector();
	const FRotator SpawnRot = ForwardDir.Rotation();
	const float HalfSpan = CorridorSpan * 0.5f;
	const FVector BaseSpawnLoc = GetActorLocation() + (ForwardDir * (-HalfSpan + 35.0f)) + FVector(0.0f, 0.0f, TripwireHeight);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();

	// Wave 2 leads further down the corridor to catch running players
	const float Wave2Offsets[] = { -40.0f, 65.0f, 95.0f };
	for (int32 i = 0; i < 3; ++i)
	{
		const float YOffset = Wave2Offsets[i];
		const float ZOffset = (i == 1) ? 8.0f : -4.0f;
		const FVector DartLoc = BaseSpawnLoc + (RightDir * YOffset) + FVector(0.0f, 0.0f, ZOffset);

		ADartProjectile* Dart = World->SpawnActor<ADartProjectile>(ProjectileClass, DartLoc, SpawnRot, SpawnParams);
		if (Dart && Dart->CollisionComp)
		{
			Dart->CollisionComp->IgnoreActorWhenMoving(this, true);
		}
	}
}

void AWallDartTrap::ResetTrap()
{
	TrapState = EDartTrapState::Armed;
	if (TripwireMesh)
	{
		TripwireMesh->SetVisibility(true);
	}
}

void AWallDartTrap::HighlightTrap(float Duration)
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

	if (DispenserMesh)
	{
		DispenserMesh->SetRenderCustomDepth(true);
		DispenserMesh->SetCustomDepthStencilValue(2);
		if (HighlightMID)
		{
			DispenserMesh->SetOverlayMaterial(HighlightMID);
		}
	}
	if (ReceiverMesh)
	{
		ReceiverMesh->SetRenderCustomDepth(true);
		ReceiverMesh->SetCustomDepthStencilValue(2);
		if (HighlightMID)
		{
			ReceiverMesh->SetOverlayMaterial(HighlightMID);
		}
	}
	if (TripwireMesh)
	{
		TripwireMesh->SetRenderCustomDepth(true);
		TripwireMesh->SetCustomDepthStencilValue(2);
		if (HighlightMID)
		{
			TripwireMesh->SetOverlayMaterial(HighlightMID);
		}
	}

	GetWorldTimerManager().ClearTimer(HighlightTimerHandle);
	GetWorldTimerManager().SetTimer(HighlightTimerHandle, this, &AWallDartTrap::OnHighlightExpired, Duration, false);
}

void AWallDartTrap::OnHighlightExpired()
{
	if (DispenserMesh)
	{
		DispenserMesh->SetRenderCustomDepth(false);
		DispenserMesh->SetOverlayMaterial(nullptr);
	}
	if (ReceiverMesh)
	{
		ReceiverMesh->SetRenderCustomDepth(false);
		ReceiverMesh->SetOverlayMaterial(nullptr);
	}
	if (TripwireMesh)
	{
		TripwireMesh->SetRenderCustomDepth(false);
		TripwireMesh->SetOverlayMaterial(nullptr);
	}
}
