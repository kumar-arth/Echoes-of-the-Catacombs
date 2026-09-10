// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Math/RandomStream.h"
#include "DungeonGenerator.generated.h"

UENUM(BlueprintType)
enum class EDungeonCellType : uint8
{
	Solid,
	Corridor,
	Room,
	SpecialRoom_Jewel,
	SpecialRoom_Beast,
	SpecialRoom_Exit
};

struct FDungeonRoom
{
	int32 X = 0;
	int32 Y = 0;
	int32 Width = 0;
	int32 Height = 0;
	EDungeonCellType Type = EDungeonCellType::Room;

	FIntPoint GetCenter() const
	{
		return FIntPoint(X + Width / 2, Y + Height / 2);
	}

	bool Overlaps(const FDungeonRoom& Other, int32 Padding = 2) const
	{
		return (X - Padding < Other.X + Other.Width) && (X + Width + Padding > Other.X) &&
			   (Y - Padding < Other.Y + Other.Height) && (Y + Height + Padding > Other.Y);
	}
};

UCLASS(Blueprintable, ClassGroup = (Dungeon), meta = (BlueprintSpawnableComponent))
class ECHOESOFTHECATACOMB_API ADungeonGenerator : public AActor
{
	GENERATED_BODY()

public:
	ADungeonGenerator();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;
	virtual void PostActorCreated() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Generates a complete procedural maze dungeon inside the current level */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Dungeon")
	void GenerateDungeon();

	/** Clears all generated instances from the scene */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Dungeon")
	void ClearDungeon();

	/** Regenerates dungeon with a fresh random seed */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Dungeon")
	void RegenerateDungeon();

	/** Regenerates dungeon with a fresh random seed */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Dungeon")
	void RegenerateWithNewSeed();

	// --- Configurable Settings ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings")
	int32 TileSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings", meta = (ClampMin = "15", ClampMax = "63"))
	int32 GridSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings", meta = (ClampMin = "3", ClampMax = "7"))
	int32 MinRoomSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings", meta = (ClampMin = "3", ClampMax = "9"))
	int32 MaxRoomSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings")
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings")
	bool bRandomizeSeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float MazeDensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings", meta = (ClampMin = "1", ClampMax = "3"))
	int32 CorridorWidth;

	/** Wall height in Unreal units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings")
	float WallHeight;

	/** Minimum padding (in cells) maintained between rooms */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings", meta = (ClampMin = "1", ClampMax = "4"))
	int32 RoomPadding;

	/** Probability of opening extra passages to create loops and alternate routes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float LoopProbability;

	/** Probability of placing a decorative wall instead of standard wall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DecorativeWallRatio;

	/** Approximate distance between wall torches */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings", meta = (ClampMin = "2", ClampMax = "10"))
	int32 TorchInterval;

	// --- Gameplay Locations ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Locations")
	FVector PlayerSpawnLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Locations")
	FVector JewelRoomLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Locations")
	FVector JewelLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Locations")
	FVector JewelBeastLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Locations")
	FVector BeastRoomLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Locations")
	FVector BeastLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Locations")
	FVector ExitRoomLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Locations")
	FVector ExitDoorLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Locations")
	FVector ExitBeastLocation;

	// --- Blueprint Getters ---

	UFUNCTION(BlueprintPure, Category = "Dungeon|Locations")
	FVector GetPlayerSpawnLocation() const { return PlayerSpawnLocation; }

	UFUNCTION(BlueprintPure, Category = "Dungeon|Locations")
	FVector GetJewelRoomLocation() const { return JewelRoomLocation; }

	UFUNCTION(BlueprintPure, Category = "Dungeon|Locations")
	FVector GetJewelLocation() const { return JewelLocation; }

	UFUNCTION(BlueprintPure, Category = "Dungeon|Locations")
	FVector GetJewelBeastLocation() const { return JewelBeastLocation; }

	UFUNCTION(BlueprintPure, Category = "Dungeon|Locations")
	FVector GetBeastRoomLocation() const { return BeastRoomLocation; }

	UFUNCTION(BlueprintPure, Category = "Dungeon|Locations")
	FVector GetBeastLocation() const { return BeastLocation; }

	UFUNCTION(BlueprintPure, Category = "Dungeon|Locations")
	FVector GetExitRoomLocation() const { return ExitRoomLocation; }

	UFUNCTION(BlueprintPure, Category = "Dungeon|Locations")
	FVector GetExitDoorLocation() const { return ExitDoorLocation; }

	UFUNCTION(BlueprintPure, Category = "Dungeon|Locations")
	FVector GetExitBeastLocation() const { return ExitBeastLocation; }

	// --- Mesh Assets (Configurable, defaults from MedievalDungeon) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
	TSoftObjectPtr<UStaticMesh> FloorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
	TSoftObjectPtr<UStaticMesh> CeilingMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
	TSoftObjectPtr<UStaticMesh> WallMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
	TSoftObjectPtr<UStaticMesh> DecorativeWallMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
	TSoftObjectPtr<UStaticMesh> WallTrimMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
	TSoftObjectPtr<UStaticMesh> DoorwayMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
	TSoftObjectPtr<UStaticMesh> PillarMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
	TSoftObjectPtr<UStaticMesh> TorchMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
	TSubclassOf<AActor> TorchBlueprintClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
	TSoftObjectPtr<UStaticMesh> AltarMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
	TSoftObjectPtr<UStaticMesh> CoffinMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
	TSoftObjectPtr<UStaticMesh> PotMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
	TSoftObjectPtr<UStaticMesh> StatueMesh;

	// --- Instanced Static Mesh Components ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
	TObjectPtr<UInstancedStaticMeshComponent> FloorInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
	TObjectPtr<UInstancedStaticMeshComponent> CeilingInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
	TObjectPtr<UInstancedStaticMeshComponent> RoofInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
	TObjectPtr<UInstancedStaticMeshComponent> WallInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
	TObjectPtr<UInstancedStaticMeshComponent> DecorativeWallInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
	TObjectPtr<UInstancedStaticMeshComponent> WallTrimInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
	TObjectPtr<UInstancedStaticMeshComponent> DoorwayInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
	TObjectPtr<UInstancedStaticMeshComponent> PillarInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
	TObjectPtr<UInstancedStaticMeshComponent> TorchInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
	TObjectPtr<UInstancedStaticMeshComponent> AltarInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
	TObjectPtr<UInstancedStaticMeshComponent> CoffinInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
	TObjectPtr<UInstancedStaticMeshComponent> PotInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
	TObjectPtr<UInstancedStaticMeshComponent> StatueInstances;

protected:
	/** Controlled random number stream for deterministic reproducibility */
	FRandomStream RandomStream;

	/** 2D grid storing cell types */
	TArray<EDungeonCellType> Grid;

	/** Placed rooms */
	TArray<FDungeonRoom> Rooms;

	/** Special room indices in Rooms array */
	int32 JewelRoomIndex = INDEX_NONE;
	int32 BeastRoomIndex = INDEX_NONE;
	int32 ExitRoomIndex = INDEX_NONE;

	/** Set of cells blocked by props to avoid player spawning inside props */
	TSet<FIntPoint> PropBlockedCells;

	/** Weak pointers to spawned torch actors for clean lifecycle management */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SpawnedTorches;

	// Internal generation pipeline methods
	void InitializeComponents();
	void EnsureMeshesLoaded();
	bool GenerateDungeonLayout();
	void PlaceSpecialRooms();
	void PlaceGenericRooms();
	void CarveMazeCorridors();
	void ConnectRoomsToMaze();
	void AddLoopConnections();
	bool ValidateConnectivity();
	void BuildGeometryInstances();
	void PlaceWallPillars();
	void PlaceEnvironmentalProps();
	void ClearSpawnedTorches();
	void SpawnTorchActor(const FTransform& Transform);
	bool SelectAndSetPlayerSpawn();

	// Grid Helper Methods
	FORCEINLINE int32 GetGridIndex(int32 X, int32 Y) const
	{
		return Y * GridSize + X;
	}

	FORCEINLINE bool IsValidCell(int32 X, int32 Y) const
	{
		return X >= 0 && X < GridSize && Y >= 0 && Y < GridSize;
	}

	FORCEINLINE EDungeonCellType GetCell(int32 X, int32 Y) const
	{
		int32 Index = GetGridIndex(X, Y);
		if (!IsValidCell(X, Y) || !Grid.IsValidIndex(Index)) return EDungeonCellType::Solid;
		return Grid[Index];
	}

	FORCEINLINE void SetCell(int32 X, int32 Y, EDungeonCellType Type)
	{
		int32 Index = GetGridIndex(X, Y);
		if (IsValidCell(X, Y) && Grid.IsValidIndex(Index))
		{
			Grid[Index] = Type;
		}
	}

	FORCEINLINE bool IsWalkable(int32 X, int32 Y) const
	{
		if (!IsValidCell(X, Y)) return false;
		EDungeonCellType Type = GetCell(X, Y);
		return Type != EDungeonCellType::Solid;
	}

	FVector GridToWorld(int32 X, int32 Y, float Z = 0.0f) const
	{
		return FVector(X * TileSize, Y * TileSize, Z);
	}

	FVector GridToWorldCenter(int32 X, int32 Y, float Z = 0.0f) const
	{
		return FVector(X * TileSize + TileSize * 0.5f, Y * TileSize + TileSize * 0.5f, Z);
	}
};
