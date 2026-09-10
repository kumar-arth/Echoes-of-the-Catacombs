// Copyright Epic Games, Inc. All Rights Reserved.

#include "DungeonGenerator.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h"
#include "Components/PointLightComponent.h"
#include "Containers/Queue.h"
#include "EngineUtils.h"

ADungeonGenerator::ADungeonGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	// Grid Defaults
	TileSize = 500;
	GridSize = 31;
	MinRoomSize = 3;
	MaxRoomSize = 5;
	Seed = 12345;
	bRandomizeSeed = true;
	MazeDensity = 0.85f;
	CorridorWidth = 1;
	WallHeight = 450.0f;
	RoomPadding = 2;
	LoopProbability = 0.12f;
	DecorativeWallRatio = 0.25f;
	TorchInterval = 4;

	// Set Root Component
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Default Medieval Dungeon Mesh Asset References
	FloorMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Floor.SM_Crypt_Floor")));
	WallMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Wall.SM_Crypt_Wall")));
	DecorativeWallMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Wall_Decorative_A.SM_Crypt_Wall_Decorative_A")));
	WallTrimMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Wall_Floor_Trim.SM_Crypt_Wall_Floor_Trim")));
	DoorwayMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Doorway.SM_Crypt_Doorway")));
	CeilingMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Ceiling_Flat.SM_Crypt_Ceiling_Flat")));
	PillarMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Pillar.SM_Pillar")));
	TorchMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Torch.SM_Torch")));
	TorchBlueprintClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Blueprints/BP_Torch.BP_Torch_C"))).LoadSynchronous();
	AltarMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Alter.SM_Alter")));
	CoffinMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Stone_Coffin.SM_Stone_Coffin")));
	PotMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Pot_A_Complete.SM_Pot_A_Complete")));
	StatueMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Gargoyle_Statue_On_Stand.SM_Gargoyle_Statue_On_Stand")));

	// Helper lambda for creating ISM components with collision enabled
	auto CreateISM = [this, SceneRoot](const TCHAR* Name) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* ISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(FName(Name));
		ISM->SetupAttachment(SceneRoot);
		ISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		ISM->SetCollisionObjectType(ECC_WorldStatic);
		ISM->SetCollisionResponseToAllChannels(ECR_Block);
		ISM->bDisableCollision = false;
		ISM->SetGenerateOverlapEvents(false);
		return ISM;
	};

	FloorInstances = CreateISM(TEXT("FloorInstances"));
	CeilingInstances = CreateISM(TEXT("CeilingInstances"));
	RoofInstances = CreateISM(TEXT("RoofInstances"));
	WallInstances = CreateISM(TEXT("WallInstances"));
	DecorativeWallInstances = CreateISM(TEXT("DecorativeWallInstances"));
	WallTrimInstances = CreateISM(TEXT("WallTrimInstances"));
	DoorwayInstances = CreateISM(TEXT("DoorwayInstances"));
	PillarInstances = CreateISM(TEXT("PillarInstances"));
	TorchInstances = CreateISM(TEXT("TorchInstances"));
	AltarInstances = CreateISM(TEXT("AltarInstances"));
	CoffinInstances = CreateISM(TEXT("CoffinInstances"));
	PotInstances = CreateISM(TEXT("PotInstances"));
	StatueInstances = CreateISM(TEXT("StatueInstances"));
}

void ADungeonGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	GenerateDungeon();
}

void ADungeonGenerator::PostLoad()
{
	Super::PostLoad();

	// Automatically build the maze when opening Level1 or reopening Unreal
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

	// Automatically update when any property changes in Details panel
	GenerateDungeon();
}
#endif

void ADungeonGenerator::BeginPlay()
{
	Super::BeginPlay();

	// Always generate in PIE world to ensure full grid state, geometry, and collision
	GenerateDungeon();

	// Teleport local player pawn to the generated PlayerSpawnLocation
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (PlayerPawn && !PlayerSpawnLocation.IsZero())
	{
		PlayerPawn->SetActorLocation(PlayerSpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogTemp, Log, TEXT("DungeonGenerator: Teleported player pawn to spawn location: %s"), *PlayerSpawnLocation.ToString());
	}
}

void ADungeonGenerator::GenerateDungeon()
{
	if (bRandomizeSeed)
	{
		Seed = FMath::RandRange(1, 9999999);
	}
	RandomStream.Initialize(Seed);

	EnsureMeshesLoaded();

	const int32 MaxAttempts = 15;
	bool bSuccess = false;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		ClearDungeon();

		if (GenerateDungeonLayout())
		{
			BuildGeometryInstances();
			PlaceEnvironmentalProps();
			if (SelectAndSetPlayerSpawn())
			{
				bSuccess = true;
				break;
			}
		}
	}

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("DungeonGenerator: Failed to generate valid connected dungeon after %d attempts!"), MaxAttempts);
		return;
	}

	// Comprehensive UE_LOG summary
	int32 WalkableCount = 0;
	for (EDungeonCellType Cell : Grid)
	{
		if (Cell != EDungeonCellType::Solid)
		{
			WalkableCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("=================================================="));
	UE_LOG(LogTemp, Log, TEXT("           DUNGEON GENERATION COMPLETE            "));
	UE_LOG(LogTemp, Log, TEXT("=================================================="));
	UE_LOG(LogTemp, Log, TEXT("Seed:                    %d (Randomized: %s)"), Seed, bRandomizeSeed ? TEXT("True") : TEXT("False"));
	UE_LOG(LogTemp, Log, TEXT("Grid Size:               %dx%d (%d units tile)"), GridSize, GridSize, TileSize);
	UE_LOG(LogTemp, Log, TEXT("Number of Rooms:         %d (Jewel, Beast, Exit + Generics)"), Rooms.Num());
	UE_LOG(LogTemp, Log, TEXT("Walkable Cells:          %d"), WalkableCount);
	UE_LOG(LogTemp, Log, TEXT("Floor Instances:         %d"), FloorInstances->GetInstanceCount());
	UE_LOG(LogTemp, Log, TEXT("Wall Instances:          %d"), WallInstances->GetInstanceCount() + DecorativeWallInstances->GetInstanceCount());
	UE_LOG(LogTemp, Log, TEXT("Doorway Instances:       %d"), DoorwayInstances->GetInstanceCount());
	UE_LOG(LogTemp, Log, TEXT("Pillar Instances:        %d"), PillarInstances->GetInstanceCount());
	UE_LOG(LogTemp, Log, TEXT("Torch Instances:         %d"), TorchInstances->GetInstanceCount());
	UE_LOG(LogTemp, Log, TEXT("Prop Instances:          %d"), AltarInstances->GetInstanceCount() + CoffinInstances->GetInstanceCount() + PotInstances->GetInstanceCount() + StatueInstances->GetInstanceCount());
	UE_LOG(LogTemp, Log, TEXT("Player Spawn Location:   %s"), *PlayerSpawnLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("Jewel Room Location:     %s"), *JewelRoomLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("  - Jewel Location:      %s"), *JewelLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("  - Jewel Beast:         %s"), *JewelBeastLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("Beast Room Location:     %s"), *BeastRoomLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("  - Beast Location:      %s"), *BeastLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("Exit Room Location:      %s"), *ExitRoomLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("  - Exit Door Location:  %s"), *ExitDoorLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("  - Exit Beast Location: %s"), *ExitBeastLocation.ToString());
	UE_LOG(LogTemp, Log, TEXT("Connectivity Result:     VALID (All special rooms connected)"));
	UE_LOG(LogTemp, Log, TEXT("=================================================="));
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

	JewelRoomIndex = INDEX_NONE;
	BeastRoomIndex = INDEX_NONE;
	ExitRoomIndex = INDEX_NONE;

	ClearSpawnedTorches();
}

void ADungeonGenerator::ClearSpawnedTorches()
{
	// 1. Destroy any in-memory tracked torches
	for (TWeakObjectPtr<AActor>& TorchPtr : SpawnedTorches)
	{
		if (TorchPtr.IsValid())
		{
			TorchPtr->Destroy();
		}
	}
	SpawnedTorches.Empty();

	// 2. Destroy all actors attached to this generator tagged as DungeonTorch
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for (AActor* Child : AttachedActors)
	{
		if (Child && (Child->ActorHasTag(TEXT("DungeonTorch")) || (TorchBlueprintClass && Child->IsA(TorchBlueprintClass))))
		{
			Child->Destroy();
		}
	}

	// 3. Scan world to completely purge any torches from previous generation sessions
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && Actor != this)
			{
				bool bIsTorch = Actor->ActorHasTag(TEXT("DungeonTorch")) ||
				                Actor->GetOwner() == this ||
				                Actor->GetAttachParentActor() == this;
#if WITH_EDITOR
				if (!bIsTorch && Actor->GetFolderPath() == FName(TEXT("Dungeon_Torches")))
				{
					bIsTorch = true;
				}
#endif
				if (bIsTorch)
				{
					Actor->Destroy();
				}
			}
		}
	}
}

void ADungeonGenerator::SpawnTorchActor(const FTransform& Transform)
{
	// Spawn functional BP_Torch actor with fire particles and flickering point light
	UWorld* World = GetWorld();
	bool bSpawnedActor = false;
	if (World && TorchBlueprintClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* TorchActor = World->SpawnActor<AActor>(TorchBlueprintClass, Transform, SpawnParams);
		if (TorchActor)
		{
			TorchActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			TorchActor->Tags.AddUnique(FName(TEXT("DungeonTorch")));
#if WITH_EDITOR
			TorchActor->SetFolderPath(FName(TEXT("Dungeon_Torches")));
#endif
			TArray<UPointLightComponent*> Lights;
			TorchActor->GetComponents<UPointLightComponent>(Lights);
			for (UPointLightComponent* LightComp : Lights)
			{
				if (LightComp)
				{
					LightComp->SetIntensity(40.0f);
					LightComp->SetAttenuationRadius(750.0f);
				}
			}
			SpawnedTorches.Add(TorchActor);
			bSpawnedActor = true;
		}
	}

	// Fallback to static mesh instance only if actor could not be spawned
	if (!bSpawnedActor && TorchInstances)
	{
		TorchInstances->AddInstance(Transform);
	}
}

void ADungeonGenerator::EnsureMeshesLoaded()
{
	auto AssignMesh = [](UInstancedStaticMeshComponent* ISM, TSoftObjectPtr<UStaticMesh>& SoftMesh)
	{
		if (ISM && SoftMesh.IsValid())
		{
			ISM->SetStaticMesh(SoftMesh.Get());
		}
		else if (ISM)
		{
			UStaticMesh* Loaded = SoftMesh.LoadSynchronous();
			if (Loaded)
			{
				ISM->SetStaticMesh(Loaded);
			}
		}
	};

	AssignMesh(FloorInstances, FloorMesh);
	AssignMesh(CeilingInstances, CeilingMesh);
	AssignMesh(RoofInstances, FloorMesh);
	AssignMesh(WallInstances, WallMesh);
	AssignMesh(DecorativeWallInstances, DecorativeWallMesh);
	AssignMesh(WallTrimInstances, WallTrimMesh);
	AssignMesh(DoorwayInstances, DoorwayMesh);
	AssignMesh(PillarInstances, PillarMesh);
	AssignMesh(TorchInstances, TorchMesh);
	AssignMesh(AltarInstances, AltarMesh);
	AssignMesh(CoffinInstances, CoffinMesh);
	AssignMesh(PotInstances, PotMesh);
	AssignMesh(StatueInstances, StatueMesh);

	if (!TorchBlueprintClass)
	{
		TorchBlueprintClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/MedievalDungeon/Blueprints/BP_Torch.BP_Torch_C"))).LoadSynchronous();
	}
}

bool ADungeonGenerator::GenerateDungeonLayout()
{
	// Enforce odd grid size for maze algorithms
	if (GridSize % 2 == 0)
	{
		GridSize += 1;
	}

	Grid.Init(EDungeonCellType::Solid, GridSize * GridSize);
	Rooms.Empty();

	// 1. Place the 3 Mandatory Special Rooms
	PlaceSpecialRooms();

	// 2. Place Additional Medieval Chambers for exploration
	PlaceGenericRooms();

	// 3. Carve dense maze corridors via Recursive Backtracker
	CarveMazeCorridors();

	// 4. Connect rooms to the maze via doorways
	ConnectRoomsToMaze();

	// 5. Add loops and alternate routes
	AddLoopConnections();

	// 6. Validate full connectivity
	return ValidateConnectivity();
}

void ADungeonGenerator::PlaceSpecialRooms()
{
	// Subdivide grid into 3 distant regions for the 3 special rooms
	// Jewel Room: Center / South region (e.g. Cols 12-18, Rows 5-11)
	// Beast Room: West region (e.g. Cols 4-10, Rows 16-22)
	// Exit Room:  North-East region (e.g. Cols 20-26, Rows 20-26)

	auto TryPlaceSpecialRoom = [this](int32 MinX, int32 MaxX, int32 MinY, int32 MaxY, int32 RoomW, int32 RoomH, EDungeonCellType RoomType) -> int32
	{
		// Make sure width and height are odd or even-consistent
		int32 BestX = -1;
		int32 BestY = -1;

		for (int32 Attempt = 0; Attempt < 20; ++Attempt)
		{
			int32 RX = RandomStream.RandRange(MinX, FMath::Max(MinX, MaxX - RoomW));
			int32 RY = RandomStream.RandRange(MinY, FMath::Max(MinY, MaxY - RoomH));

			// Align room origins to odd coordinates for maze grid alignment
			if (RX % 2 == 0) RX++;
			if (RY % 2 == 0) RY++;

			FDungeonRoom Candidate;
			Candidate.X = RX;
			Candidate.Y = RY;
			Candidate.Width = RoomW;
			Candidate.Height = RoomH;
			Candidate.Type = RoomType;

			bool bOverlap = false;
			for (const FDungeonRoom& R : Rooms)
			{
				if (Candidate.Overlaps(R, RoomPadding))
				{
					bOverlap = true;
					break;
				}
			}

			if (!bOverlap && (Candidate.X + Candidate.Width < GridSize - 1) && (Candidate.Y + Candidate.Height < GridSize - 1))
			{
				BestX = RX;
				BestY = RY;
				break;
			}
		}

		if (BestX == -1)
		{
			BestX = FMath::Clamp(MinX, 1, GridSize - RoomW - 2);
			BestY = FMath::Clamp(MinY, 1, GridSize - RoomH - 2);
		}

		FDungeonRoom Placed;
		Placed.X = BestX;
		Placed.Y = BestY;
		Placed.Width = RoomW;
		Placed.Height = RoomH;
		Placed.Type = RoomType;

		for (int32 X = Placed.X; X < Placed.X + Placed.Width; ++X)
		{
			for (int32 Y = Placed.Y; Y < Placed.Y + Placed.Height; ++Y)
			{
				SetCell(X, Y, RoomType);
			}
		}

		return Rooms.Add(Placed);
	};

	// 1. Jewel Room (Size 4x4 or 5x5) in Center-South
	int32 CenterMin = GridSize / 3;
	int32 CenterMax = (GridSize * 2) / 3;
	JewelRoomIndex = TryPlaceSpecialRoom(CenterMin, CenterMax, 3, GridSize / 2, 5, 5, EDungeonCellType::SpecialRoom_Jewel);

	// 2. Beast Room (Size 4x4) in West
	BeastRoomIndex = TryPlaceSpecialRoom(2, GridSize / 3, CenterMin, GridSize - 6, 4, 4, EDungeonCellType::SpecialRoom_Beast);

	// 3. Exit Room (Size 4x4) in North-East
	ExitRoomIndex = TryPlaceSpecialRoom(CenterMax, GridSize - 5, CenterMax, GridSize - 5, 4, 4, EDungeonCellType::SpecialRoom_Exit);

	// Cache locations
	if (Rooms.IsValidIndex(JewelRoomIndex))
	{
		FIntPoint C = Rooms[JewelRoomIndex].GetCenter();
		JewelRoomLocation = GridToWorldCenter(C.X, C.Y, 0.0f);
		JewelLocation = JewelRoomLocation + FVector(0.0f, 0.0f, 60.0f);
		JewelBeastLocation = GridToWorldCenter(C.X + 1, C.Y, 0.0f);
	}

	if (Rooms.IsValidIndex(BeastRoomIndex))
	{
		FIntPoint C = Rooms[BeastRoomIndex].GetCenter();
		BeastRoomLocation = GridToWorldCenter(C.X, C.Y, 0.0f);
		BeastLocation = BeastRoomLocation + FVector(0.0f, 0.0f, 50.0f);
	}

	if (Rooms.IsValidIndex(ExitRoomIndex))
	{
		const FDungeonRoom& ExitRoom = Rooms[ExitRoomIndex];
		FIntPoint C = ExitRoom.GetCenter();
		ExitRoomLocation = GridToWorldCenter(C.X, C.Y, 0.0f);
		ExitBeastLocation = GridToWorldCenter(C.X - 1, C.Y, 0.0f);
		// Exit door on the North wall of the Exit Room
		ExitDoorLocation = FVector((ExitRoom.X + ExitRoom.Width / 2) * TileSize, (ExitRoom.Y + ExitRoom.Height) * TileSize, 0.0f);
	}
}

void ADungeonGenerator::PlaceGenericRooms()
{
	const int32 NumGenericRooms = RandomStream.RandRange(2, 4);

	for (int32 i = 0; i < NumGenericRooms; ++i)
	{
		int32 RW = RandomStream.RandRange(MinRoomSize, MaxRoomSize);
		int32 RH = RandomStream.RandRange(MinRoomSize, MaxRoomSize);

		for (int32 Attempt = 0; Attempt < 15; ++Attempt)
		{
			int32 RX = RandomStream.RandRange(2, GridSize - RW - 3);
			int32 RY = RandomStream.RandRange(2, GridSize - RH - 3);

			if (RX % 2 == 0) RX++;
			if (RY % 2 == 0) RY++;

			FDungeonRoom Candidate;
			Candidate.X = RX;
			Candidate.Y = RY;
			Candidate.Width = RW;
			Candidate.Height = RH;
			Candidate.Type = EDungeonCellType::Room;

			bool bOverlap = false;
			for (const FDungeonRoom& R : Rooms)
			{
				if (Candidate.Overlaps(R, RoomPadding))
				{
					bOverlap = true;
					break;
				}
			}

			if (!bOverlap && (Candidate.X + Candidate.Width < GridSize - 1) && (Candidate.Y + Candidate.Height < GridSize - 1))
			{
				for (int32 X = Candidate.X; X < Candidate.X + Candidate.Width; ++X)
				{
					for (int32 Y = Candidate.Y; Y < Candidate.Y + Candidate.Height; ++Y)
					{
						SetCell(X, Y, EDungeonCellType::Room);
					}
				}
				Rooms.Add(Candidate);
				break;
			}
		}
	}
}

void ADungeonGenerator::CarveMazeCorridors()
{
	// Recursive Backtracker maze algorithm operating on odd grid coordinates with step size 2
	TArray<FIntPoint> Stack;

	// Find an unvisited odd cell that is solid
	auto FindUnvisitedCell = [this]() -> FIntPoint
	{
		TArray<FIntPoint> Candidates;
		for (int32 X = 1; X < GridSize - 1; X += 2)
		{
			for (int32 Y = 1; Y < GridSize - 1; Y += 2)
			{
				if (GetCell(X, Y) == EDungeonCellType::Solid)
				{
					Candidates.Add(FIntPoint(X, Y));
				}
			}
		}

		if (Candidates.Num() > 0)
		{
			int32 Idx = RandomStream.RandRange(0, Candidates.Num() - 1);
			return Candidates[Idx];
		}
		return FIntPoint(-1, -1);
	};

	const FIntPoint Dirs[4] = { FIntPoint(0, 2), FIntPoint(0, -2), FIntPoint(2, 0), FIntPoint(-2, 0) };

	FIntPoint StartCell = FindUnvisitedCell();
	while (StartCell.X != -1)
	{
		SetCell(StartCell.X, StartCell.Y, EDungeonCellType::Corridor);
		Stack.Push(StartCell);

		while (Stack.Num() > 0)
		{
			FIntPoint Current = Stack.Top();

			// Gather valid unvisited neighbors
			TArray<int32> ValidDirs;
			for (int32 d = 0; d < 4; ++d)
			{
				int32 NX = Current.X + Dirs[d].X;
				int32 NY = Current.Y + Dirs[d].Y;

				if (IsValidCell(NX, NY) && NX > 0 && NX < GridSize - 1 && NY > 0 && NY < GridSize - 1)
				{
					if (GetCell(NX, NY) == EDungeonCellType::Solid)
					{
						ValidDirs.Add(d);
					}
				}
			}

			if (ValidDirs.Num() > 0)
			{
				// Pick random direction
				int32 ChosenDir = ValidDirs[RandomStream.RandRange(0, ValidDirs.Num() - 1)];
				int32 NX = Current.X + Dirs[ChosenDir].X;
				int32 NY = Current.Y + Dirs[ChosenDir].Y;

				// Carve intermediate wall cell
				int32 WallX = Current.X + Dirs[ChosenDir].X / 2;
				int32 WallY = Current.Y + Dirs[ChosenDir].Y / 2;

				SetCell(WallX, WallY, EDungeonCellType::Corridor);
				SetCell(NX, NY, EDungeonCellType::Corridor);

				Stack.Push(FIntPoint(NX, NY));
			}
			else
			{
				Stack.Pop();
			}
		}

		StartCell = FindUnvisitedCell();
	}
}

void ADungeonGenerator::ConnectRoomsToMaze()
{
	// Connect each room to adjacent corridors by opening doorways
	for (const FDungeonRoom& Room : Rooms)
	{
		TArray<FIntPoint> PotentialDoors;

		// Check South wall (Y - 1)
		for (int32 X = Room.X; X < Room.X + Room.Width; ++X)
		{
			if (IsValidCell(X, Room.Y - 2) && GetCell(X, Room.Y - 2) == EDungeonCellType::Corridor)
			{
				PotentialDoors.Add(FIntPoint(X, Room.Y - 1));
			}
		}

		// Check North wall (Y + Height)
		for (int32 X = Room.X; X < Room.X + Room.Width; ++X)
		{
			if (IsValidCell(X, Room.Y + Room.Height + 1) && GetCell(X, Room.Y + Room.Height + 1) == EDungeonCellType::Corridor)
			{
				PotentialDoors.Add(FIntPoint(X, Room.Y + Room.Height));
			}
		}

		// Check West wall (X - 1)
		for (int32 Y = Room.Y; Y < Room.Y + Room.Height; ++Y)
		{
			if (IsValidCell(Room.X - 2, Y) && GetCell(Room.X - 2, Y) == EDungeonCellType::Corridor)
			{
				PotentialDoors.Add(FIntPoint(Room.X - 1, Y));
			}
		}

		// Check East wall (X + Width)
		for (int32 Y = Room.Y; Y < Room.Y + Room.Height; ++Y)
		{
			if (IsValidCell(Room.X + Room.Width + 1, Y) && GetCell(Room.X + Room.Width + 1, Y) == EDungeonCellType::Corridor)
			{
				PotentialDoors.Add(FIntPoint(Room.X + Room.Width, Y));
			}
		}

		if (PotentialDoors.Num() > 0)
		{
			// Open 1 or 2 doors per room
			int32 DoorsToOpen = FMath::Min(PotentialDoors.Num(), (Room.Width >= 4 || Room.Height >= 4) ? 2 : 1);
			for (int32 d = 0; d < DoorsToOpen; ++d)
			{
				int32 PickIdx = RandomStream.RandRange(0, PotentialDoors.Num() - 1);
				FIntPoint Door = PotentialDoors[PickIdx];
				SetCell(Door.X, Door.Y, EDungeonCellType::Corridor);
				PotentialDoors.RemoveAt(PickIdx);
			}
		}
		else
		{
			// Fallback: forcefully tunnel 1 cell outwards towards center
			FIntPoint C = Room.GetCenter();
			int32 TargetX = FMath::Clamp(Room.X - 1, 1, GridSize - 2);
			SetCell(TargetX, C.Y, EDungeonCellType::Corridor);
		}
	}
}

void ADungeonGenerator::AddLoopConnections()
{
	// Iterate through internal walls and remove a fraction of them to create cycles/loops
	for (int32 X = 2; X < GridSize - 2; ++X)
	{
		for (int32 Y = 2; Y < GridSize - 2; ++Y)
		{
			if (GetCell(X, Y) == EDungeonCellType::Solid)
			{
				bool bConnectsHoriz = IsWalkable(X - 1, Y) && IsWalkable(X + 1, Y) && !IsWalkable(X, Y - 1) && !IsWalkable(X, Y + 1);
				bool bConnectsVert = IsWalkable(X, Y - 1) && IsWalkable(X, Y + 1) && !IsWalkable(X - 1, Y) && !IsWalkable(X + 1, Y);

				if ((bConnectsHoriz || bConnectsVert) && RandomStream.FRand() < LoopProbability)
				{
					SetCell(X, Y, EDungeonCellType::Corridor);
				}
			}
		}
	}
}

bool ADungeonGenerator::ValidateConnectivity()
{
	// BFS reachability from any walkable cell
	FIntPoint Start(-1, -1);
	for (int32 X = 1; X < GridSize - 1; ++X)
	{
		for (int32 Y = 1; Y < GridSize - 1; ++Y)
		{
			if (GetCell(X, Y) == EDungeonCellType::Corridor)
			{
				Start = FIntPoint(X, Y);
				break;
			}
		}
		if (Start.X != -1) break;
	}

	if (Start.X == -1) return false;

	TArray<bool> Visited;
	Visited.Init(false, GridSize * GridSize);
	TQueue<FIntPoint> Queue;

	Queue.Enqueue(Start);
	Visited[GetGridIndex(Start.X, Start.Y)] = true;

	const FIntPoint Dirs[4] = { FIntPoint(0, 1), FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(-1, 0) };

	while (!Queue.IsEmpty())
	{
		FIntPoint Curr;
		Queue.Dequeue(Curr);

		for (int32 d = 0; d < 4; ++d)
		{
			int32 NX = Curr.X + Dirs[d].X;
			int32 NY = Curr.Y + Dirs[d].Y;

			if (IsValidCell(NX, NY) && IsWalkable(NX, NY))
			{
				int32 Idx = GetGridIndex(NX, NY);
				if (!Visited[Idx])
				{
					Visited[Idx] = true;
					Queue.Enqueue(FIntPoint(NX, NY));
				}
			}
		}
	}

	// Verify all 3 special rooms are reached
	auto IsRoomVisited = [&Visited, this](int32 RoomIdx) -> bool
	{
		if (!Rooms.IsValidIndex(RoomIdx)) return false;
		const FDungeonRoom& R = Rooms[RoomIdx];
		FIntPoint C = R.GetCenter();
		return Visited[GetGridIndex(C.X, C.Y)];
	};

	return IsRoomVisited(JewelRoomIndex) && IsRoomVisited(BeastRoomIndex) && IsRoomVisited(ExitRoomIndex);
}

void ADungeonGenerator::BuildGeometryInstances()
{
	const float FTile = (float)TileSize;

	// 1. Spawn Continuous Solid Stone Foundation Floor over ENTIRE grid (-1..GridSize)
	// Completely prevents any white voids or gaps underneath walls, cupboards, or borders
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

	// 1b. Spawn Continuous Solid Stone Ceiling and Roof Cover over the ENTIRE grid (walkable, solid, and 1-tile perimeter border)
	// Completely seals the maze ceiling with zero gaps, holes, or exterior light leaks
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

	// 2. Spawn Walls & Doorways
	for (int32 X = 0; X < GridSize; ++X)
	{
		for (int32 Y = 0; Y < GridSize; ++Y)
		{
			if (IsWalkable(X, Y))
			{
				// Helper for placing wall instance with slight 1.01 scale in length to eliminate seams
				// DecLoc & DecRot are specifically computed so decorative cupboards always face their stone arch & shelves INTO the corridor
				auto PlaceWall = [this](FVector Loc, FRotator Rot, FVector DecLoc, FRotator DecRot, bool bCanBeDecorative)
				{
					bool bDecorative = bCanBeDecorative && (RandomStream.FRand() < DecorativeWallRatio) && DecorativeWallInstances;
					if (bDecorative)
					{
						FTransform DecTransform(DecRot, DecLoc, FVector(1.01f, 1.0f, 1.0f));
						DecorativeWallInstances->AddInstance(DecTransform);
					}
					else
					{
						FTransform WallTransform(Rot, Loc, FVector(1.01f, 1.0f, 1.0f));
						WallInstances->AddInstance(WallTransform);
					}

					// Spawn continuous sculpted stone floor trim along the base of all walls facing the corridor
					if (WallTrimInstances)
					{
						FTransform TrimTransform(Rot, Loc, FVector(1.01f, 1.0f, 1.0f));
						WallTrimInstances->AddInstance(TrimTransform);
					}
				};

				// South Edge (Y - 1): Corridor is at +Y (North). Cupboard must face North (DecRot = 0 deg)
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

				// North Edge (Y + 1): Corridor is at -Y (South). Cupboard must face South (DecRot = 180 deg)
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

				// West Edge (X - 1): Corridor is at +X (East). Cupboard must face East (DecRot = -90 deg)
				if (!IsWalkable(X - 1, Y))
				{
					bool bStraight = (!IsWalkable(X - 1, Y - 1) && IsWalkable(X, Y - 1)) &&
					                 (!IsWalkable(X - 1, Y + 1) && IsWalkable(X, Y + 1));
					PlaceWall(
						FVector(X * FTile, Y * FTile, 0.0f), FRotator(0.0f, -90.0f, 0.0f),
						FVector(X * FTile, Y * FTile, 0.0f), FRotator(0.0f, -90.0f, 0.0f),
						bStraight
					);
				}

				// East Edge (X + 1): Corridor is at -X (West). Cupboard must face West (DecRot = +90 deg)
				if (!IsWalkable(X + 1, Y))
				{
					bool bStraight = (!IsWalkable(X + 1, Y - 1) && IsWalkable(X, Y - 1)) &&
					                 (!IsWalkable(X + 1, Y + 1) && IsWalkable(X, Y + 1));
					PlaceWall(
						FVector((X + 1) * FTile, (Y + 1) * FTile, 0.0f), FRotator(0.0f, 90.0f, 0.0f),
						FVector((X + 1) * FTile, (Y + 1) * FTile, 0.0f), FRotator(0.0f, 90.0f, 0.0f),
						bStraight
					);
				}
			}
		}
	}

	// 3. Place Corner Pillars at all Wall Junctions and Corners to seal gaps completely
	PlaceWallPillars();

	// 4. Spawn Doorway Frame at the Exit Door
	DoorwayInstances->AddInstance(FTransform(FRotator(0.0f, 180.0f, 0.0f), ExitDoorLocation + FVector(TileSize * 0.5f, 0.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f)));
}

void ADungeonGenerator::PlaceWallPillars()
{
	const float FTile = (float)TileSize;
	if (!PillarInstances) return;

	TSet<FIntPoint> PlacedPillars;

	// Iterate all grid intersection vertices (X, Y) from 0 to GridSize
	for (int32 X = 0; X <= GridSize; ++X)
	{
		for (int32 Y = 0; Y <= GridSize; ++Y)
		{
			// Check the 4 quadrants meeting at vertex (X, Y)
			bool bNW = IsWalkable(X - 1, Y);
			bool bNE = IsWalkable(X, Y);
			bool bSW = IsWalkable(X - 1, Y - 1);
			bool bSE = IsWalkable(X, Y - 1);

			// Wall segment flags touching this vertex:
			bool bWallWest = (bNW != bSW);
			bool bWallEast = (bNE != bSE);
			bool bWallNorth = (bNE != bNW);
			bool bWallSouth = (bSE != bSW);

			int32 WallCount = (bWallWest ? 1 : 0) + (bWallEast ? 1 : 0) +
			                  (bWallNorth ? 1 : 0) + (bWallSouth ? 1 : 0);

			if (WallCount == 0) continue;

			// If at least one adjacent cell is walkable:
			bool bHasWalkable = bNW || bNE || bSW || bSE;
			if (!bHasWalkable) continue;

			bool bNeedsPillar = false;

			// Endpoint of a wall (dead-end wall)
			if (WallCount == 1)
			{
				bNeedsPillar = true;
			}
			// L-Corner: Two perpendicular walls meet
			else if (WallCount == 2)
			{
				bool bCollinear = (bWallWest && bWallEast) || (bWallNorth && bWallSouth);
				if (!bCollinear)
				{
					bNeedsPillar = true;
				}
			}
			// T-Junction or Cross (3 or 4 walls meet)
			else if (WallCount >= 3)
			{
				bNeedsPillar = true;
			}

			if (bNeedsPillar)
			{
				FIntPoint V(X, Y);
				if (!PlacedPillars.Contains(V))
				{
					PlacedPillars.Add(V);
					PillarInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X * FTile, Y * FTile, 0.0f), FVector(1.0f, 1.0f, 1.0f)));
				}
			}
		}
	}

	// Also ensure all room corners have pillars
	for (const FDungeonRoom& Room : Rooms)
	{
		TArray<FIntPoint> RoomCorners = {
			FIntPoint(Room.X, Room.Y),
			FIntPoint(Room.X + Room.Width, Room.Y),
			FIntPoint(Room.X, Room.Y + Room.Height),
			FIntPoint(Room.X + Room.Width, Room.Y + Room.Height)
		};
		for (const FIntPoint& Pt : RoomCorners)
		{
			if (!PlacedPillars.Contains(Pt))
			{
				PlacedPillars.Add(Pt);
				PillarInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Pt.X * FTile, Pt.Y * FTile, 0.0f), FVector(1.0f, 1.0f, 1.0f)));
			}
		}
	}
}

void ADungeonGenerator::PlaceEnvironmentalProps()
{
	const float FTile = (float)TileSize;

	// 1. Jewel Room: Central Altar / Pedestal for the Jewel & Flanking Statues
	if (Rooms.IsValidIndex(JewelRoomIndex))
	{
		const FDungeonRoom& JR = Rooms[JewelRoomIndex];
		FIntPoint C = JR.GetCenter();
		FVector AltarPos = GridToWorldCenter(C.X, C.Y, 0.0f);
		AltarInstances->AddInstance(FTransform(FRotator::ZeroRotator, AltarPos, FVector(1.2f, 1.2f, 1.2f)));
		PropBlockedCells.Add(C);

		// Two decorative statues flanking the altar
		StatueInstances->AddInstance(FTransform(FRotator(0.0f, 90.0f, 0.0f), AltarPos + FVector(-150.0f, 0.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f)));
		StatueInstances->AddInstance(FTransform(FRotator(0.0f, -90.0f, 0.0f), AltarPos + FVector(150.0f, 0.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f)));

		// 4 Room Torches illuminating the Jewel Altar
		SpawnTorchActor(FTransform(FRotator(0.0f, 0.0f, 0.0f), FVector(JR.X * FTile + FTile, JR.Y * FTile + 15.0f, 220.0f), FVector(1.0f, 1.0f, 1.0f)));
		SpawnTorchActor(FTransform(FRotator(0.0f, 0.0f, 0.0f), FVector((JR.X + JR.Width - 1) * FTile, JR.Y * FTile + 15.0f, 220.0f), FVector(1.0f, 1.0f, 1.0f)));
		SpawnTorchActor(FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(JR.X * FTile + FTile, (JR.Y + JR.Height) * FTile - 15.0f, 220.0f), FVector(1.0f, 1.0f, 1.0f)));
		SpawnTorchActor(FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector((JR.X + JR.Width - 1) * FTile, (JR.Y + JR.Height) * FTile - 15.0f, 220.0f), FVector(1.0f, 1.0f, 1.0f)));
	}

	// 2. Beast Room: Sarcophagus / Coffin placed along back wall & Flanking Torches
	if (Rooms.IsValidIndex(BeastRoomIndex))
	{
		const FDungeonRoom& BR = Rooms[BeastRoomIndex];
		FVector CoffinPos = FVector((BR.X + BR.Width - 1) * FTile + FTile * 0.5f, (BR.Y + 1) * FTile + FTile * 0.5f, 0.0f);
		CoffinInstances->AddInstance(FTransform(FRotator(0.0f, 90.0f, 0.0f), CoffinPos, FVector(1.0f, 1.0f, 1.0f)));
		PropBlockedCells.Add(FIntPoint(BR.X + BR.Width - 1, BR.Y + 1));

		// Torches illuminating the arena
		SpawnTorchActor(FTransform(FRotator(0.0f, 0.0f, 0.0f), FVector(BR.X * FTile + FTile, BR.Y * FTile + 15.0f, 220.0f), FVector(1.0f, 1.0f, 1.0f)));
		SpawnTorchActor(FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(BR.X * FTile + FTile, (BR.Y + BR.Height) * FTile - 15.0f, 220.0f), FVector(1.0f, 1.0f, 1.0f)));
	}

	// 3. Exit Room: Flanking Statues and Torches by the Exit Door
	StatueInstances->AddInstance(FTransform(FRotator(0.0f, 180.0f, 0.0f), ExitDoorLocation + FVector(-180.0f, -50.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f)));
	StatueInstances->AddInstance(FTransform(FRotator(0.0f, 180.0f, 0.0f), ExitDoorLocation + FVector(TileSize + 180.0f, -50.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f)));

	SpawnTorchActor(FTransform(FRotator(0.0f, 0.0f, 0.0f), ExitDoorLocation + FVector(-120.0f, 15.0f, 220.0f), FVector(1.0f, 1.0f, 1.0f)));
	SpawnTorchActor(FTransform(FRotator(0.0f, 0.0f, 0.0f), ExitDoorLocation + FVector(TileSize + 120.0f, 15.0f, 220.0f), FVector(1.0f, 1.0f, 1.0f)));

	// 4. Wall Torches along corridors and maze passages
	int32 StepCounter = 0;
	for (int32 X = 1; X < GridSize - 1; ++X)
	{
		for (int32 Y = 1; Y < GridSize - 1; ++Y)
		{
			if (IsWalkable(X, Y))
			{
				StepCounter++;
				if (StepCounter % TorchInterval == 0)
				{
					// Check for an adjacent wall to mount the torch with correct inward facing rotations
					// North Wall (Y + 1 is solid): corridor is -Y, torch faces -Y (Yaw = 180)
					if (!IsWalkable(X, Y + 1))
					{
						FVector TorchPos(X * FTile + FTile * 0.5f, (Y + 1) * FTile - 15.0f, 220.0f);
						SpawnTorchActor(FTransform(FRotator(0.0f, 180.0f, 0.0f), TorchPos, FVector(1.0f, 1.0f, 1.0f)));
					}
					// South Wall (Y - 1 is solid): corridor is +Y, torch faces +Y (Yaw = 0)
					else if (!IsWalkable(X, Y - 1))
					{
						FVector TorchPos(X * FTile + FTile * 0.5f, Y * FTile + 15.0f, 220.0f);
						SpawnTorchActor(FTransform(FRotator(0.0f, 0.0f, 0.0f), TorchPos, FVector(1.0f, 1.0f, 1.0f)));
					}
					// West Wall (X - 1 is solid): corridor is +X, torch faces +X (Yaw = -90)
					else if (!IsWalkable(X - 1, Y))
					{
						FVector TorchPos(X * FTile + 15.0f, Y * FTile + FTile * 0.5f, 220.0f);
						SpawnTorchActor(FTransform(FRotator(0.0f, -90.0f, 0.0f), TorchPos, FVector(1.0f, 1.0f, 1.0f)));
					}
					// East Wall (X + 1 is solid): corridor is -X, torch faces -X (Yaw = 90)
					else if (!IsWalkable(X + 1, Y))
					{
						FVector TorchPos((X + 1) * FTile - 15.0f, Y * FTile + FTile * 0.5f, 220.0f);
						SpawnTorchActor(FTransform(FRotator(0.0f, 90.0f, 0.0f), TorchPos, FVector(1.0f, 1.0f, 1.0f)));
					}
				}

				// Occasional decorative pots in dead ends or corridor corners
				int32 WallCount = (!IsWalkable(X + 1, Y) ? 1 : 0) + (!IsWalkable(X - 1, Y) ? 1 : 0) +
				                  (!IsWalkable(X, Y + 1) ? 1 : 0) + (!IsWalkable(X, Y - 1) ? 1 : 0);
				if (WallCount >= 3 && RandomStream.FRand() < 0.4f)
				{
					FVector PotPos = GridToWorldCenter(X, Y, 0.0f) + FVector(120.0f, 120.0f, 0.0f);
					PotInstances->AddInstance(FTransform(FRotator(0.0f, RandomStream.RandRange(0, 360), 0.0f), PotPos, FVector(0.9f, 0.9f, 0.9f)));
					PropBlockedCells.Add(FIntPoint(X, Y));
				}
			}
		}
	}
}

bool ADungeonGenerator::SelectAndSetPlayerSpawn()
{
	// Build list of valid walkable corridor cells
	TArray<FIntPoint> ValidSpawns;

	for (int32 X = 1; X < GridSize - 1; ++X)
	{
		for (int32 Y = 1; Y < GridSize - 1; ++Y)
		{
			// Must be a Corridor cell (not inside any room)
			if (GetCell(X, Y) != EDungeonCellType::Corridor)
			{
				continue;
			}

			// Not blocked by props
			if (PropBlockedCells.Contains(FIntPoint(X, Y)))
			{
				continue;
			}

			// Ensure distance from Special Rooms
			bool bTooClose = false;
			const int32 MinDistanceToSpecialRoom = 5;

			for (int32 RIdx : { JewelRoomIndex, BeastRoomIndex, ExitRoomIndex })
			{
				if (Rooms.IsValidIndex(RIdx))
				{
					FIntPoint C = Rooms[RIdx].GetCenter();
					int32 Dist = FMath::Abs(X - C.X) + FMath::Abs(Y - C.Y);
					if (Dist < MinDistanceToSpecialRoom)
					{
						bTooClose = true;
						break;
					}
				}
			}

			if (!bTooClose)
			{
				ValidSpawns.Add(FIntPoint(X, Y));
			}
		}
	}

	if (ValidSpawns.Num() == 0)
	{
		return false;
	}

	int32 PickIdx = RandomStream.RandRange(0, ValidSpawns.Num() - 1);
	FIntPoint SpawnCell = ValidSpawns[PickIdx];

	PlayerSpawnLocation = GridToWorldCenter(SpawnCell.X, SpawnCell.Y, 100.0f);
	return true;
}
