// Copyright Epic Games, Inc. All Rights Reserved.

#include "DungeonGenerator.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Containers/Queue.h"

ADungeonGenerator::ADungeonGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Default Generation Settings
	TileSize = 500;
	GridSize = 31;
	MinRoomSize = 5;
	MaxRoomSize = 7;
	Seed = 821797;
	bRandomizeSeed = false;
	bSpawnAsIndividualActors = true;
	MazeDensity = 0.85f;
	CorridorWidth = 1;
	WallHeight = 450.0f;
	RoomPadding = 2;
	LoopProbability = 0.12f;
	DecorativeWallRatio = 0.25f;
	TorchInterval = 4;

	// Asset Paths from MedievalDungeon
	FloorMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Floor_Trim_01.SM_Crypt_Floor_Trim_01")));
	CeilingMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Ceiling_Arched.SM_Crypt_Ceiling_Arched")));
	WallMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Wall.SM_Crypt_Wall")));
	DecorativeWallMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Wall_Decorative_A.SM_Crypt_Wall_Decorative_A")));
	WallTrimMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Floor_Trim_02.SM_Crypt_Floor_Trim_02")));
	PillarMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Pillar.SM_Crypt_Pillar")));
	DoorwayMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Doorway.SM_Crypt_Doorway")));
	AltarMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Altar.SM_Altar")));
	CoffinMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Coffin.SM_Coffin")));
	PotMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Urn_01.SM_Urn_01")));
	StatueMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Gargoyle_Statue.SM_Gargoyle_Statue")));

	static ConstructorHelpers::FClassFinder<AActor> TorchBPClass(TEXT("/Game/MedievalDungeon/Blueprints/BP_Torch"));
	if (TorchBPClass.Succeeded())
	{
		TorchBlueprintClass = TorchBPClass.Class;
	}

	InitializeComponents();
}

void ADungeonGenerator::InitializeComponents()
{
	auto CreateISM = [this](const TCHAR* CompName) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* Comp = CreateDefaultSubobject<UInstancedStaticMeshComponent>(CompName);
		if (Comp)
		{
			Comp->SetupAttachment(RootComponent);
			Comp->SetMobility(EComponentMobility::Static);
			Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Comp->SetCollisionObjectType(ECC_WorldStatic);
			Comp->SetCollisionResponseToAllChannels(ECR_Block);
			Comp->SetGenerateOverlapEvents(false);
			Comp->bCastDynamicShadow = true;
			Comp->CastShadow = true;
		}
		return Comp;
	};

	FloorInstances = CreateISM(TEXT("FloorInstances"));
	CeilingInstances = CreateISM(TEXT("CeilingInstances"));
	RoofInstances = CreateISM(TEXT("RoofInstances"));
	WallInstances = CreateISM(TEXT("WallInstances"));
	DecorativeWallInstances = CreateISM(TEXT("DecorativeWallInstances"));
	WallTrimInstances = CreateISM(TEXT("WallTrimInstances"));
	PillarInstances = CreateISM(TEXT("PillarInstances"));
	DoorwayInstances = CreateISM(TEXT("DoorwayInstances"));
	TorchInstances = CreateISM(TEXT("TorchInstances"));
	AltarInstances = CreateISM(TEXT("AltarInstances"));
	CoffinInstances = CreateISM(TEXT("CoffinInstances"));
	PotInstances = CreateISM(TEXT("PotInstances"));
	StatueInstances = CreateISM(TEXT("StatueInstances"));
}

void ADungeonGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Do not call GenerateDungeon() here: spawning actors inside OnConstruction
	// causes Unreal Engine to automatically parent all spawned actors under DungeonGenerator.
}

void ADungeonGenerator::PostLoad()
{
	Super::PostLoad();
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		GenerateDungeon();
	}
}

void ADungeonGenerator::PostActorCreated()
{
	Super::PostActorCreated();
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		GenerateDungeon();
	}
}

#if WITH_EDITOR
void ADungeonGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	GenerateDungeon();

	TArray<AActor*> Attached;
	GetAttachedActors(Attached);
	for (AActor* Act : Attached)
	{
		if (Act)
		{
			Act->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Act->SetFolderPath(FName(TEXT("Dungeon_Torches")));
		}
	}
}
#endif

void ADungeonGenerator::BeginPlay()
{
	Super::BeginPlay();
	GenerateDungeon();

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (PlayerPawn && !PlayerSpawnLocation.IsZero())
	{
		PlayerPawn->SetActorLocation(PlayerSpawnLocation + FVector(0.0f, 0.0f, 50.0f));
	}
}

void ADungeonGenerator::EnsureMeshesLoaded()
{
	auto AssignMesh = [](UInstancedStaticMeshComponent* Comp, TSoftObjectPtr<UStaticMesh>& SoftMesh)
	{
		if (Comp && SoftMesh.IsValid())
		{
			UStaticMesh* Loaded = SoftMesh.Get();
			if (!Loaded)
			{
				Loaded = SoftMesh.LoadSynchronous();
			}
			if (Loaded)
			{
				Comp->SetStaticMesh(Loaded);
			}
		}
	};

	AssignMesh(FloorInstances, FloorMesh);
	AssignMesh(CeilingInstances, CeilingMesh);
	AssignMesh(RoofInstances, FloorMesh);
	AssignMesh(WallInstances, WallMesh);
	AssignMesh(DecorativeWallInstances, DecorativeWallMesh);
	AssignMesh(WallTrimInstances, WallTrimMesh);
	AssignMesh(PillarInstances, PillarMesh);
	AssignMesh(DoorwayInstances, DoorwayMesh);
	AssignMesh(AltarInstances, AltarMesh);
	AssignMesh(CoffinInstances, CoffinMesh);
	AssignMesh(PotInstances, PotMesh);
	AssignMesh(StatueInstances, StatueMesh);
}

void ADungeonGenerator::GenerateDungeon()
{
	if (bRandomizeSeed)
	{
		Seed = FMath::RandRange(1, 9999999);
	}
	RandomStream.Initialize(Seed);

	EnsureMeshesLoaded();
	ClearDungeon();

	if (GenerateDungeonLayout())
	{
		BuildGeometryInstances();
		PlaceEnvironmentalProps();
		SelectAndSetPlayerSpawn();
	}

	UE_LOG(LogTemp, Log, TEXT("=================================================="));
	UE_LOG(LogTemp, Log, TEXT("     3-STAGE CANON EVENT DUNGEON GENERATED        "));
	UE_LOG(LogTemp, Log, TEXT("=================================================="));
	UE_LOG(LogTemp, Log, TEXT("Seed:                    %d"), Seed);
	UE_LOG(LogTemp, Log, TEXT("Player Spawn Location:   %s"), *PlayerSpawnLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("Boss 1 Room Location:    %s"), *Boss1RoomLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("  - Boss 1 Spawn:        %s"), *Boss1SpawnLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("  - Boss 1 Entrance:     %s"), *Boss1EntranceDoorLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("  - Boss 1 Exit:         %s"), *Boss1ExitDoorLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("Boss 2 Room Location:    %s"), *Boss2RoomLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("  - Boss 2 Spawn:        %s"), *Boss2SpawnLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("  - Boss 2 Entrance:     %s"), *Boss2EntranceDoorLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("  - Boss 2 Exit:         %s"), *Boss2ExitDoorLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("Boss 3 Room Location:    %s"), *Boss3RoomLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("  - Boss 3 Spawn:        %s"), *Boss3SpawnLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("  - Boss 3 Entrance:     %s"), *Boss3EntranceDoorLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("  - Boss 3 Final Exit:   %s"), *Boss3ExitDoorLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("=================================================="));

		if (bSpawnAsIndividualActors)
	{
		BakeToStaticMeshActors();
	}

#if WITH_EDITOR
	for (TWeakObjectPtr<AActor>& TorchPtr : SpawnedTorches)
	{
		if (AActor* Torch = TorchPtr.Get())
		{
			if (USceneComponent* Root = Torch->GetRootComponent())
			{
				Root->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}
			Torch->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Torch->SetFolderPath(FName(TEXT("Dungeon_Torches")));
		}
	}
#endif
}

void ADungeonGenerator::RegenerateDungeon()
{
	GenerateDungeon();
}

void ADungeonGenerator::RegenerateWithNewSeed()
{
	Seed = FMath::RandRange(1000, 999999);
	GenerateDungeon();
}

void ADungeonGenerator::ClearDungeon()
{
	if (FloorInstances) FloorInstances->ClearInstances();
	if (CeilingInstances) CeilingInstances->ClearInstances();
	if (RoofInstances) RoofInstances->ClearInstances();
	if (WallInstances) WallInstances->ClearInstances();
	if (DecorativeWallInstances) DecorativeWallInstances->ClearInstances();
	if (WallTrimInstances) WallTrimInstances->ClearInstances();
	if (DoorwayInstances) DoorwayInstances->ClearInstances();
	if (PillarInstances) PillarInstances->ClearInstances();
	if (TorchInstances) TorchInstances->ClearInstances();
	if (AltarInstances) AltarInstances->ClearInstances();
	if (CoffinInstances) CoffinInstances->ClearInstances();
	if (PotInstances) PotInstances->ClearInstances();
	if (StatueInstances) StatueInstances->ClearInstances();

	Grid.Empty();
	Rooms.Empty();
	PropBlockedCells.Empty();

	Boss1RoomIndex = INDEX_NONE;
	Boss2RoomIndex = INDEX_NONE;
	Boss3RoomIndex = INDEX_NONE;

	ClearSpawnedTorches();
	ClearBakedActors();
}

void ADungeonGenerator::ClearSpawnedTorches()
{
	UWorld* World = GetWorld();
	if (!World) return;

	TSet<AActor*> TorchesToDestroy;

	for (TWeakObjectPtr<AActor>& TorchPtr : SpawnedTorches)
	{
		if (TorchPtr.IsValid())
		{
			TorchesToDestroy.Add(TorchPtr.Get());
		}
	}
	SpawnedTorches.Empty();

	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for (AActor* Child : AttachedActors)
	{
		if (IsValid(Child) && (Child->ActorHasTag(TEXT("DungeonTorch")) || (TorchBlueprintClass && Child->IsA(TorchBlueprintClass))))
		{
			TorchesToDestroy.Add(Child);
		}
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (IsValid(Actor) && Actor != this)
		{
			bool bIsTorch = Actor->ActorHasTag(TEXT("DungeonTorch")) ||
			                (TorchBlueprintClass && Actor->IsA(TorchBlueprintClass));
#if WITH_EDITOR
			if (!bIsTorch && Actor->GetFolderPath() == FName(TEXT("Dungeon_Torches")))
			{
				bIsTorch = true;
			}
#endif
			if (bIsTorch)
			{
				TorchesToDestroy.Add(Actor);
			}
		}
	}

	for (AActor* Torch : TorchesToDestroy)
	{
		if (IsValid(Torch))
		{
			// Safely detach first if attached to prevent detachment assertion
			if (Torch->GetAttachParentActor())
			{
				Torch->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			}
			World->DestroyActor(Torch, false, false);
		}
	}
}

void ADungeonGenerator::SpawnTorchActor(const FTransform& Transform)
{
	UWorld* World = GetWorld();
	if (!World || !TorchBlueprintClass) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = nullptr;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.OverrideLevel = World->GetCurrentLevel();

	AActor* TorchActor = World->SpawnActor<AActor>(TorchBlueprintClass, Transform, SpawnParams);
	if (TorchActor)
	{
		TorchActor->Tags.AddUnique(TEXT("DungeonTorch"));
		if (USceneComponent* Root = TorchActor->GetRootComponent())
		{
			Root->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
		TorchActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
#if WITH_EDITOR
		TorchActor->SetFolderPath(FName(TEXT("Dungeon_Torches")));
		TorchActor->SetActorLabel(FString::Printf(TEXT("Torch_%d"), SpawnedTorches.Num() + 1));
#endif
		SpawnedTorches.Add(TorchActor);
	}
}
bool ADungeonGenerator::GenerateDungeonLayout()
{
	GridSize = 31;
	Grid.Init(EDungeonCellType::Solid, GridSize * GridSize);
	Rooms.Empty();

	// 1. Build the 3 Boss Arenas (Unskippable Chokepoints)
	BuildBossArenas();

	// 2. Carve Sector 1 Maze (West Column: X in [1..9], Y in [1..18])
	// Carve two 3x3 pillared crypt chambers as branching decision hubs
	CarveCryptChamber(2, 3, 3, 3);
	CarveCryptChamber(5, 11, 3, 3);
	FIntPoint S1Start(3, 3);
	FIntPoint S1End(4, 18);
	CarveSectorMaze(1, 9, 1, 18, S1Start, S1End);
	// Dedicated 1-tile entrance approach to Boss 1 (no wide horizontal highway)
	SetCell(4, 19, EDungeonCellType::Corridor);
	SetCell(4, 20, EDungeonCellType::BossDoor_1_Enter);

	// 3. Connect Boss 1 Exit (8, 24) to Sector 2 start with stepped S-turn
	SetCell(8, 24, EDungeonCellType::BossDoor_1_Exit);
	SetCell(9, 24, EDungeonCellType::Corridor);
	SetCell(9, 25, EDungeonCellType::Corridor);
	SetCell(10, 25, EDungeonCellType::Corridor);
	SetCell(10, 24, EDungeonCellType::Corridor);
	SetCell(11, 24, EDungeonCellType::Corridor);

	// 4. Carve Sector 2 Maze (Center Column: X in [11..19], Y in [12..28])
	CarveCryptChamber(12, 14, 3, 3);
	CarveCryptChamber(15, 22, 3, 3);
	FIntPoint S2Start(11, 24);
	FIntPoint S2End(14, 12);
	CarveSectorMaze(11, 19, 12, 28, S2Start, S2End);
	// Dedicated 1-tile entrance approach to Boss 2
	SetCell(14, 11, EDungeonCellType::Corridor);
	SetCell(14, 10, EDungeonCellType::BossDoor_2_Enter);

	// 5. Connect Boss 2 Exit (18, 6) to Sector 3 start with stepped S-turn
	SetCell(18, 6, EDungeonCellType::BossDoor_2_Exit);
	SetCell(19, 6, EDungeonCellType::Corridor);
	SetCell(19, 5, EDungeonCellType::Corridor);
	SetCell(20, 5, EDungeonCellType::Corridor);
	SetCell(20, 6, EDungeonCellType::Corridor);
	SetCell(21, 6, EDungeonCellType::Corridor);

	// 6. Carve Sector 3 Maze (East Column: X in [21..29], Y in [1..18])
	CarveCryptChamber(22, 3, 3, 3);
	CarveCryptChamber(25, 11, 3, 3);
	FIntPoint S3Start(21, 6);
	FIntPoint S3End(24, 18);
	CarveSectorMaze(21, 29, 1, 18, S3Start, S3End);
	// Dedicated 1-tile entrance approach to Boss 3
	SetCell(24, 19, EDungeonCellType::Corridor);
	SetCell(24, 20, EDungeonCellType::BossDoor_3_Enter);

	// 7. Add controlled dead-end loops (Only connecting dead-ends, preventing 2x2 room mergers)
	AddSectorLoops(1, 9, 1, 18, 0.20f);
	AddSectorLoops(11, 19, 12, 28, 0.20f);
	AddSectorLoops(21, 29, 1, 18, 0.20f);

	// Setup player spawn
	PlayerSpawnLocation = GridToWorldCenter(3, 3, 100.0f);

	// Compatibility aliases
	JewelRoomLocation = Boss1RoomLocation;
	JewelLocation = Boss1SpawnLocation;
	BeastRoomLocation = Boss2RoomLocation;
	BeastLocation = Boss2SpawnLocation;
	ExitRoomLocation = Boss3RoomLocation;
	ExitDoorLocation = Boss3ExitDoorLocation;

	return true;
}
void ADungeonGenerator::BuildBossArenas()
{
	// Arena Dimensions: 7x7 tiles = 3500x3500 cm

	// --- BOSS ROOM 1 (Stage 1 Finale, Top-Left) ---
	FDungeonRoom R1;
	R1.X = 1; R1.Y = 21; R1.Width = 7; R1.Height = 7;
	R1.Type = EDungeonCellType::BossRoom_1;
	for (int32 X = R1.X; X < R1.X + R1.Width; ++X)
	{
		for (int32 Y = R1.Y; Y < R1.Y + R1.Height; ++Y)
		{
			SetCell(X, Y, EDungeonCellType::BossRoom_1);
		}
	}
	// Entrance (South wall) & Exit (East wall)
	SetCell(4, 20, EDungeonCellType::BossDoor_1_Enter);
	SetCell(8, 24, EDungeonCellType::BossDoor_1_Exit);

	Boss1RoomIndex = Rooms.Add(R1);
	Boss1RoomLocation = GridToWorldCenter(4, 24, 0.0f);
	Boss1SpawnLocation = GridToWorldCenter(4, 24, 50.0f);
	Boss1EntranceDoorLocation = GridToWorldCenter(4, 20, 0.0f);
	Boss1ExitDoorLocation = GridToWorldCenter(8, 24, 0.0f);

	// --- BOSS ROOM 2 (Stage 2 Finale, Bottom-Center) ---
	FDungeonRoom R2;
	R2.X = 11; R2.Y = 3; R2.Width = 7; R2.Height = 7;
	R2.Type = EDungeonCellType::BossRoom_2;
	for (int32 X = R2.X; X < R2.X + R2.Width; ++X)
	{
		for (int32 Y = R2.Y; Y < R2.Y + R2.Height; ++Y)
		{
			SetCell(X, Y, EDungeonCellType::BossRoom_2);
		}
	}
	// Entrance (North wall) & Exit (East wall)
	SetCell(14, 10, EDungeonCellType::BossDoor_2_Enter);
	SetCell(18, 6, EDungeonCellType::BossDoor_2_Exit);

	Boss2RoomIndex = Rooms.Add(R2);
	Boss2RoomLocation = GridToWorldCenter(14, 6, 0.0f);
	Boss2SpawnLocation = GridToWorldCenter(14, 6, 50.0f);
	Boss2EntranceDoorLocation = GridToWorldCenter(14, 10, 0.0f);
	Boss2ExitDoorLocation = GridToWorldCenter(18, 6, 0.0f);

	// --- BOSS ROOM 3 (Stage 3 Grand Climax, Top-Right) ---
	FDungeonRoom R3;
	R3.X = 21; R3.Y = 21; R3.Width = 7; R3.Height = 7;
	R3.Type = EDungeonCellType::BossRoom_3;
	for (int32 X = R3.X; X < R3.X + R3.Width; ++X)
	{
		for (int32 Y = R3.Y; Y < R3.Y + R3.Height; ++Y)
		{
			SetCell(X, Y, EDungeonCellType::BossRoom_3);
		}
	}
	// Entrance (South wall) & Final Exit Door (North wall, 3 boss relics required)
	SetCell(24, 20, EDungeonCellType::BossDoor_3_Enter);
	SetCell(24, 28, EDungeonCellType::BossDoor_3_Exit);

	Boss3RoomIndex = Rooms.Add(R3);
	Boss3RoomLocation = GridToWorldCenter(24, 24, 0.0f);
	Boss3SpawnLocation = GridToWorldCenter(24, 24, 50.0f);
	Boss3EntranceDoorLocation = GridToWorldCenter(24, 20, 0.0f);
	Boss3ExitDoorLocation = GridToWorldCenter(24, 28, 0.0f);
}

void ADungeonGenerator::CarveCryptChamber(int32 OriginX, int32 OriginY, int32 W, int32 H)
{
	for (int32 X = OriginX; X < OriginX + W; ++X)
	{
		for (int32 Y = OriginY; Y < OriginY + H; ++Y)
		{
			if (X >= 0 && X < GridSize && Y >= 0 && Y < GridSize)
			{
				SetCell(X, Y, EDungeonCellType::Corridor);
			}
		}
	}
}

void ADungeonGenerator::CarveSectorMaze(int32 MinX, int32 MaxX, int32 MinY, int32 MaxY, FIntPoint StartCell, FIntPoint EndCell)
{
	int32 SX = StartCell.X;
	int32 SY = StartCell.Y;
	if (SX % 2 == 0) SX = FMath::Min(MaxX - 1, SX + 1);
	if (SY % 2 == 0) SY = FMath::Min(MaxY - 1, SY + 1);

	TArray<FIntPoint> Stack;
	if (GetCell(SX, SY) == EDungeonCellType::Solid)
	{
		SetCell(SX, SY, EDungeonCellType::Corridor);
	}
	Stack.Push(FIntPoint(SX, SY));

	const FIntPoint Dirs[4] = { FIntPoint(0, 2), FIntPoint(0, -2), FIntPoint(2, 0), FIntPoint(-2, 0) };
	int32 LastDir = -1;
	int32 StraightSteps = 0;

	while (Stack.Num() > 0)
	{
		FIntPoint Current = Stack.Top();
		TArray<int32> ValidDirs;

		for (int32 d = 0; d < 4; ++d)
		{
			int32 NX = Current.X + Dirs[d].X;
			int32 NY = Current.Y + Dirs[d].Y;

			if (NX >= MinX && NX <= MaxX && NY >= MinY && NY <= MaxY)
			{
				if (GetCell(NX, NY) == EDungeonCellType::Solid)
				{
					// Avoid running straight for more than 1 step
					if (d == LastDir && StraightSteps >= 1)
					{
						continue;
					}
					ValidDirs.Add(d);
				}
			}
		}

		// Fallback if no direction passed the straight limiter
		if (ValidDirs.Num() == 0)
		{
			for (int32 d = 0; d < 4; ++d)
			{
				int32 NX = Current.X + Dirs[d].X;
				int32 NY = Current.Y + Dirs[d].Y;
				if (NX >= MinX && NX <= MaxX && NY >= MinY && NY <= MaxY && GetCell(NX, NY) == EDungeonCellType::Solid)
				{
					ValidDirs.Add(d);
				}
			}
		}

		if (ValidDirs.Num() > 0)
		{
			int32 Chosen = -1;
			TArray<int32> TurnDirs;
			for (int32 d : ValidDirs)
			{
				if (d != LastDir) TurnDirs.Add(d);
			}

			// 85% chance to TURN, forcing winding S-bends, zigzags, and corners
			if (TurnDirs.Num() > 0 && (LastDir == -1 || RandomStream.FRand() < 0.85f))
			{
				Chosen = TurnDirs[RandomStream.RandRange(0, TurnDirs.Num() - 1)];
			}
			else
			{
				Chosen = ValidDirs[RandomStream.RandRange(0, ValidDirs.Num() - 1)];
			}

			if (Chosen == LastDir)
			{
				StraightSteps++;
			}
			else
			{
				StraightSteps = 0;
			}

			int32 MidX = Current.X + Dirs[Chosen].X / 2;
			int32 MidY = Current.Y + Dirs[Chosen].Y / 2;
			int32 NxtX = Current.X + Dirs[Chosen].X;
			int32 NxtY = Current.Y + Dirs[Chosen].Y;

			SetCell(MidX, MidY, EDungeonCellType::Corridor);
			SetCell(NxtX, NxtY, EDungeonCellType::Corridor);

			LastDir = Chosen;
			Stack.Push(FIntPoint(NxtX, NxtY));
		}
		else
		{
			Stack.Pop();
			LastDir = -1;
			StraightSteps = 0;
		}
	}

	ConnectPointToNearestCorridor(StartCell, MinX, MaxX, MinY, MaxY);
	ConnectPointToNearestCorridor(EndCell, MinX, MaxX, MinY, MaxY);
}

void ADungeonGenerator::ConnectPointToNearestCorridor(FIntPoint Pt, int32 MinX, int32 MaxX, int32 MinY, int32 MaxY)
{
	if (GetCell(Pt.X, Pt.Y) == EDungeonCellType::Corridor)
	{
		return;
	}

	// BFS from Pt to find the nearest Corridor within [MinX..MaxX, MinY..MaxY]
	TQueue<FIntPoint> Queue;
	TMap<FIntPoint, FIntPoint> ParentMap;
	TSet<FIntPoint> Visited;

	Queue.Enqueue(Pt);
	Visited.Add(Pt);

	const FIntPoint Dirs[4] = { FIntPoint(0, 1), FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(-1, 0) };
	TOptional<FIntPoint> Target;

	while (!Queue.IsEmpty())
	{
		FIntPoint Curr;
		Queue.Dequeue(Curr);

		if (GetCell(Curr.X, Curr.Y) == EDungeonCellType::Corridor && Curr != Pt)
		{
			Target = Curr;
			break;
		}

		for (int32 d = 0; d < 4; ++d)
		{
			FIntPoint Next = Curr + Dirs[d];
			if (Next.X >= MinX && Next.X <= MaxX && Next.Y >= MinY && Next.Y <= MaxY && !Visited.Contains(Next))
			{
				Visited.Add(Next);
				ParentMap.Add(Next, Curr);
				Queue.Enqueue(Next);
			}
		}
	}

	if (Target.IsSet())
	{
		FIntPoint Curr = Target.GetValue();
		while (ParentMap.Contains(Curr))
		{
			if (GetCell(Curr.X, Curr.Y) == EDungeonCellType::Solid)
			{
				SetCell(Curr.X, Curr.Y, EDungeonCellType::Corridor);
			}
			Curr = ParentMap[Curr];
		}
		if (GetCell(Pt.X, Pt.Y) == EDungeonCellType::Solid)
		{
			SetCell(Pt.X, Pt.Y, EDungeonCellType::Corridor);
		}
	}
}

void ADungeonGenerator::AddSectorLoops(int32 MinX, int32 MaxX, int32 MinY, int32 MaxY, float LoopProb)
{
	const FIntPoint Dirs[4] = { FIntPoint(0, 1), FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(-1, 0) };

	for (int32 X = MinX + 1; X < MaxX; ++X)
	{
		for (int32 Y = MinY + 1; Y < MaxY; ++Y)
		{
			if (GetCell(X, Y) == EDungeonCellType::Solid)
			{
				int32 CorrNeighbors = 0;
				for (int32 d = 0; d < 4; ++d)
				{
					int32 NX = X + Dirs[d].X;
					int32 NY = Y + Dirs[d].Y;
					if (NX >= MinX && NX <= MaxX && NY >= MinY && NY <= MaxY && GetCell(NX, NY) == EDungeonCellType::Corridor)
					{
						CorrNeighbors++;
					}
				}

				if (CorrNeighbors == 2 && RandomStream.FRand() < LoopProb)
				{
					// Ensure we don't merge walls into a 2x2 open room
					bool bCreates2x2 = false;
					for (int32 ox = -1; ox <= 0; ++ox)
					{
						for (int32 oy = -1; oy <= 0; ++oy)
						{
							int32 OpenCount = 0;
							for (int32 cx = 0; cx <= 1; ++cx)
							{
								for (int32 cy = 0; cy <= 1; ++cy)
								{
									if (GetCell(X + ox + cx, Y + oy + cy) != EDungeonCellType::Solid || (ox + cx == 0 && oy + cy == 0))
									{
										OpenCount++;
									}
								}
							}
							if (OpenCount == 4)
							{
								bCreates2x2 = true;
								break;
							}
						}
						if (bCreates2x2) break;
					}

					if (!bCreates2x2)
					{
						SetCell(X, Y, EDungeonCellType::Corridor);
					}
				}
			}
		}
	}
}
void ADungeonGenerator::BuildGeometryInstances()
{
	const float FTile = (float)TileSize;

	// 1. Continuous Stone Floor (-1..GridSize)
	if (FloorInstances)
	{
		for (int32 X = -1; X <= GridSize; ++X)
		{
			for (int32 Y = -1; Y <= GridSize; ++Y)
			{
				FVector FloorPos((X + 1) * FTile, (Y + 1) * FTile, 0.0f);
				FloorInstances->AddInstance(FTransform(FRotator::ZeroRotator, FloorPos, FVector(1.02f, 1.02f, 1.0f)));
			}
		}
	}

	// 2. Continuous Stone Ceiling & Roof (-1..GridSize)
	for (int32 X = -1; X <= GridSize; ++X)
	{
		for (int32 Y = -1; Y <= GridSize; ++Y)
		{
			FVector TilePos((X + 1) * FTile, (Y + 1) * FTile, WallHeight);
			if (CeilingInstances)
			{
				CeilingInstances->AddInstance(FTransform(FRotator::ZeroRotator, TilePos, FVector(1.02f, 1.02f, 1.0f)));
			}
			if (RoofInstances)
			{
				RoofInstances->AddInstance(FTransform(FRotator::ZeroRotator, TilePos + FVector(0.0f, 0.0f, 5.0f), FVector(1.02f, 1.02f, 1.0f)));
			}
		}
	}

	// 3. Spawn Walls
	for (int32 X = 0; X < GridSize; ++X)
	{
		for (int32 Y = 0; Y < GridSize; ++Y)
		{
			if (IsWalkable(X, Y))
			{
				auto PlaceWall = [this](FVector Loc, FRotator Rot, FVector DecLoc, FRotator DecRot, bool bCanBeDecorative)
				{
					bool bDecorative = bCanBeDecorative && (RandomStream.FRand() < DecorativeWallRatio) && DecorativeWallInstances;
					if (bDecorative)
					{
						FTransform DecTransform(DecRot, DecLoc, FVector(1.01f, 1.0f, 1.0f));
						DecorativeWallInstances->AddInstance(DecTransform);
					}
					else if (WallInstances)
					{
						FTransform WallTransform(Rot, Loc, FVector(1.01f, 1.0f, 1.0f));
						WallInstances->AddInstance(WallTransform);
					}

					if (WallTrimInstances)
					{
						FTransform TrimTransform(Rot, Loc, FVector(1.01f, 1.0f, 1.0f));
						WallTrimInstances->AddInstance(TrimTransform);
					}
				};

				// South Edge
				if (!IsWalkable(X, Y - 1))
				{
					bool bStraight = (!IsWalkable(X - 1, Y - 1) && IsWalkable(X - 1, Y)) &&
					                 (!IsWalkable(X + 1, Y - 1) && IsWalkable(X + 1, Y));
					PlaceWall(
						FVector(X * FTile, Y * FTile, 0.0f), FRotator(0.0f, 180.0f, 0.0f),
						FVector((X + 1) * FTile, Y * FTile, 0.0f), FRotator(0.0f, 0.0f, 0.0f),
						bStraight
					);
				}

				// North Edge
				if (!IsWalkable(X, Y + 1))
				{
					bool bStraight = (!IsWalkable(X - 1, Y + 1) && IsWalkable(X - 1, Y)) &&
					                 (!IsWalkable(X + 1, Y + 1) && IsWalkable(X + 1, Y));
					PlaceWall(
						FVector((X + 1) * FTile, (Y + 1) * FTile, 0.0f), FRotator(0.0f, 0.0f, 0.0f),
						FVector(X * FTile, (Y + 1) * FTile, 0.0f), FRotator(0.0f, 180.0f, 0.0f),
						bStraight
					);
				}

				// West Edge
				if (!IsWalkable(X - 1, Y))
				{
					bool bStraight = (!IsWalkable(X - 1, Y - 1) && IsWalkable(X, Y - 1)) &&
					                 (!IsWalkable(X - 1, Y + 1) && IsWalkable(X, Y + 1));
					PlaceWall(
						FVector(X * FTile, (Y + 1) * FTile, 0.0f), FRotator(0.0f, 90.0f, 0.0f),
						FVector(X * FTile, Y * FTile, 0.0f), FRotator(0.0f, -90.0f, 0.0f),
						bStraight
					);
				}

				// East Edge
				if (!IsWalkable(X + 1, Y))
				{
					bool bStraight = (!IsWalkable(X + 1, Y - 1) && IsWalkable(X, Y - 1)) &&
					                 (!IsWalkable(X + 1, Y + 1) && IsWalkable(X, Y + 1));
					PlaceWall(
						FVector((X + 1) * FTile, Y * FTile, 0.0f), FRotator(0.0f, -90.0f, 0.0f),
						FVector((X + 1) * FTile, (Y + 1) * FTile, 0.0f), FRotator(0.0f, 90.0f, 0.0f),
						bStraight
					);
				}
			}
		}
	}

	PlaceWallPillars();
}

void ADungeonGenerator::PlaceWallPillars()
{
	const float FTile = (float)TileSize;
	if (!PillarInstances) return;

	// SM_Crypt_Pillar native mesh height is 330.84cm. WallHeight is 450cm.
	// Scale Z so the pillar extends all the way from Z=0 to the roof/ceiling at Z=WallHeight + 2cm.
	const float PillarScaleZ = (WallHeight + 2.0f) / 330.84f; // 1.366f

	for (int32 X = 0; X <= GridSize; ++X)
	{
		for (int32 Y = 0; Y <= GridSize; ++Y)
		{
			bool bTL = IsWalkable(X - 1, Y);
			bool bTR = IsWalkable(X, Y);
			bool bBL = IsWalkable(X - 1, Y - 1);
			bool bBR = IsWalkable(X, Y - 1);

			int32 WalkableCornerCount = (bTL ? 1 : 0) + (bTR ? 1 : 0) + (bBL ? 1 : 0) + (bBR ? 1 : 0);
			if (WalkableCornerCount >= 1 && WalkableCornerCount <= 3)
			{
				FVector PillarPos(X * FTile, Y * FTile, 0.0f);
				PillarInstances->AddInstance(FTransform(FRotator::ZeroRotator, PillarPos, FVector(1.0f, 1.0f, PillarScaleZ)));
			}
		}
	}
}

void ADungeonGenerator::PlaceEnvironmentalProps()
{
	const float FTile = (float)TileSize;

	// 1. Fully illuminate all 3 Boss Rooms on ALL 4 SIDES with torches
	for (const FDungeonRoom& Room : Rooms)
	{
		PlaceBossRoomTorches(Room);
	}

	// 2. Place arched doorway frames at all 6 entrance/exit doors
	auto PlaceDoorway = [this, FTile](FVector DoorLoc, FRotator DoorRot)
	{
		if (DoorwayInstances)
		{
			DoorwayInstances->AddInstance(FTransform(DoorRot, DoorLoc, FVector(1.0f, 1.0f, 1.0f)));
		}
	};

	// Boss 1 Entrance (South) & Exit (East)
	PlaceDoorway(Boss1EntranceDoorLocation, FRotator(0.0f, 0.0f, 0.0f));
	PlaceDoorway(Boss1ExitDoorLocation, FRotator(0.0f, 90.0f, 0.0f));

	// Boss 2 Entrance (North) & Exit (East)
	PlaceDoorway(Boss2EntranceDoorLocation, FRotator(0.0f, 180.0f, 0.0f));
	PlaceDoorway(Boss2ExitDoorLocation, FRotator(0.0f, 90.0f, 0.0f));

	// Boss 3 Entrance (South) & Final Exit Door (North)
	PlaceDoorway(Boss3EntranceDoorLocation, FRotator(0.0f, 0.0f, 0.0f));
	PlaceDoorway(Boss3ExitDoorLocation, FRotator(0.0f, 180.0f, 0.0f));

	// 3. Boss Room 3 Final Altar / Relic Pedestals (for the 3 collected boss items)
	if (AltarInstances)
	{
		FVector Altar1Pos = Boss3RoomLocation + FVector(-300.0f, 800.0f, 0.0f);
		FVector Altar2Pos = Boss3RoomLocation + FVector(0.0f, 800.0f, 0.0f);
		FVector Altar3Pos = Boss3RoomLocation + FVector(300.0f, 800.0f, 0.0f);

		AltarInstances->AddInstance(FTransform(FRotator::ZeroRotator, Altar1Pos, FVector(0.8f, 0.8f, 0.8f)));
		AltarInstances->AddInstance(FTransform(FRotator::ZeroRotator, Altar2Pos, FVector(1.0f, 1.0f, 1.0f)));
		AltarInstances->AddInstance(FTransform(FRotator::ZeroRotator, Altar3Pos, FVector(0.8f, 0.8f, 0.8f)));
	}

	// 4. Corridor Torches spaced along hallways for mood and navigation
	int32 StepCounter = 0;
	for (int32 X = 1; X < GridSize - 1; ++X)
	{
		for (int32 Y = 1; Y < GridSize - 1; ++Y)
		{
			if (GetCell(X, Y) == EDungeonCellType::Corridor)
			{
				StepCounter++;
				if (StepCounter % TorchInterval == 0)
				{
					if (!IsWalkable(X, Y - 1))
					{
						SpawnTorchActor(FTransform(FRotator(0.0f, 0.0f, 0.0f), FVector((X + 0.5f) * FTile, Y * FTile + 15.0f, 220.0f), FVector::OneVector));
					}
					else if (!IsWalkable(X, Y + 1))
					{
						SpawnTorchActor(FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector((X + 0.5f) * FTile, (Y + 1) * FTile - 15.0f, 220.0f), FVector::OneVector));
					}
				}
			}
		}
	}
}

void ADungeonGenerator::PlaceBossRoomTorches(const FDungeonRoom& Room)
{
	const float FTile = (float)TileSize;

	// South Wall (facing North into room)
	for (int32 X = Room.X + 1; X < Room.X + Room.Width - 1; X += 2)
	{
		FVector Pos((X + 0.5f) * FTile, Room.Y * FTile + 15.0f, 220.0f);
		SpawnTorchActor(FTransform(FRotator(0.0f, 0.0f, 0.0f), Pos, FVector::OneVector));
	}

	// North Wall (facing South into room)
	for (int32 X = Room.X + 1; X < Room.X + Room.Width - 1; X += 2)
	{
		FVector Pos((X + 0.5f) * FTile, (Room.Y + Room.Height) * FTile - 15.0f, 220.0f);
		SpawnTorchActor(FTransform(FRotator(0.0f, 180.0f, 0.0f), Pos, FVector::OneVector));
	}

	// West Wall (facing East into room)
	for (int32 Y = Room.Y + 1; Y < Room.Y + Room.Height - 1; Y += 2)
	{
		FVector Pos(Room.X * FTile + 15.0f, (Y + 0.5f) * FTile, 220.0f);
		SpawnTorchActor(FTransform(FRotator(0.0f, 90.0f, 0.0f), Pos, FVector::OneVector));
	}

	// East Wall (facing West into room)
	for (int32 Y = Room.Y + 1; Y < Room.Y + Room.Height - 1; Y += 2)
	{
		FVector Pos((Room.X + Room.Width) * FTile - 15.0f, (Y + 0.5f) * FTile, 220.0f);
		SpawnTorchActor(FTransform(FRotator(0.0f, -90.0f, 0.0f), Pos, FVector::OneVector));
	}
}

bool ADungeonGenerator::SelectAndSetPlayerSpawn()
{
	PlayerSpawnLocation = GridToWorldCenter(3, 3, 100.0f);
	return true;
}

void ADungeonGenerator::BakeToStaticMeshActors()
{
	UWorld* World = GetWorld();
	if (!World) return;

	ClearBakedActors();

	auto SpawnActorsFromISM = [this, World](UInstancedStaticMeshComponent* ISM, TSoftObjectPtr<UStaticMesh>& SoftMesh, const FName& FolderName, const FString& LabelPrefix)
	{
		if (!ISM) return;
		UStaticMesh* Mesh = SoftMesh.Get();
		if (!Mesh)
		{
			Mesh = SoftMesh.LoadSynchronous();
		}
		if (!Mesh) return;

		const int32 Count = ISM->GetInstanceCount();
		for (int32 i = 0; i < Count; ++i)
		{
			FTransform InstanceTransform;
			ISM->GetInstanceTransform(i, InstanceTransform, true);

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = nullptr;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AStaticMeshActor* SMActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), InstanceTransform, SpawnParams);
			if (SMActor)
			{
				UStaticMeshComponent* SMC = SMActor->GetStaticMeshComponent();
				if (SMC)
				{
					SMC->SetStaticMesh(Mesh);
					SMC->SetMobility(EComponentMobility::Static);
					SMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					SMC->SetCollisionObjectType(ECC_WorldStatic);
					SMC->SetCollisionResponseToAllChannels(ECR_Block);
				}
#if WITH_EDITOR
				SMActor->SetFolderPath(FolderName);
				SMActor->SetActorLabel(FString::Printf(TEXT("%s_%d"), *LabelPrefix, i + 1));
#endif
				BakedActors.Add(SMActor);
			}
		}
		ISM->ClearInstances();
	};

	// Pillars
	SpawnActorsFromISM(PillarInstances, PillarMesh, FName(TEXT("Dungeon_Pillars")), TEXT("Pillar"));

	// Walls
	SpawnActorsFromISM(WallInstances, WallMesh, FName(TEXT("Dungeon_Walls")), TEXT("Wall"));
	SpawnActorsFromISM(DecorativeWallInstances, DecorativeWallMesh, FName(TEXT("Dungeon_Walls")), TEXT("Wall_Decorative"));

	// Doorways
	SpawnActorsFromISM(DoorwayInstances, DoorwayMesh, FName(TEXT("Dungeon_Doors")), TEXT("Doorway"));

	// Props
	SpawnActorsFromISM(AltarInstances, AltarMesh, FName(TEXT("Dungeon_Props")), TEXT("Altar"));
	SpawnActorsFromISM(CoffinInstances, CoffinMesh, FName(TEXT("Dungeon_Props")), TEXT("Coffin"));
	SpawnActorsFromISM(PotInstances, PotMesh, FName(TEXT("Dungeon_Props")), TEXT("Pot"));
	SpawnActorsFromISM(StatueInstances, StatueMesh, FName(TEXT("Dungeon_Props")), TEXT("Statue"));

	UE_LOG(LogTemp, Log, TEXT("DungeonGenerator: Successfully baked %d static mesh actors into individual objects!"), BakedActors.Num());
}

void ADungeonGenerator::ClearBakedActors()
{
	for (TWeakObjectPtr<AActor>& ActorPtr : BakedActors)
	{
		if (ActorPtr.IsValid())
		{
			ActorPtr->Destroy();
		}
	}
	BakedActors.Empty();

	UWorld* World = GetWorld();
	if (World && !World->IsGameWorld())
	{
		TArray<AActor*> AllActors;
		UGameplayStatics::GetAllActorsOfClass(World, AStaticMeshActor::StaticClass(), AllActors);
		for (AActor* Actor : AllActors)
		{
			if (IsValid(Actor))
			{
#if WITH_EDITOR
				FName Folder = Actor->GetFolderPath();
				if (Folder == FName(TEXT("Dungeon_Pillars")) ||
					Folder == FName(TEXT("Dungeon_Walls")) ||
					Folder == FName(TEXT("Dungeon_Doors")) ||
					Folder == FName(TEXT("Dungeon_Floors")) ||
					Folder == FName(TEXT("Dungeon_Ceilings")) ||
					Folder == FName(TEXT("Dungeon_Props")))
				{
					World->DestroyActor(Actor, false, false);
				}
#endif
			}
		}
	}
}
