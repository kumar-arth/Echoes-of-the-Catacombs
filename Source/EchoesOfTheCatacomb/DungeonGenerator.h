
#pragma once

// clang-format off
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Math/RandomStream.h"

#include "DungeonGenerator.generated.h"
// clang-format on

UENUM(BlueprintType)
enum class EDungeonCellType : uint8 {
  Solid,
  Corridor,
  Room,
  BossRoom_1,
  BossRoom_2,
  BossRoom_3,
  BossDoor_1_Enter,
  BossDoor_1_Exit,
  BossDoor_2_Enter,
  BossDoor_2_Exit,
  BossDoor_3_Enter,
  BossDoor_3_Exit,
  SpecialRoom_Jewel,
  SpecialRoom_Beast,
  SpecialRoom_Exit
};

class ASpikeTrap;
class AWallDartTrap;
class ABossMinotaur;
class ABossGrux;
class ABossShadowKnight;
class ABossArenaGate;

struct FDungeonRoom {
  int32 X = 0;
  int32 Y = 0;
  int32 Width = 0;
  int32 Height = 0;
  EDungeonCellType Type = EDungeonCellType::Room;

  FIntPoint GetCenter() const {
    return FIntPoint(X + Width / 2, Y + Height / 2);
  }

  bool Overlaps(const FDungeonRoom &Other, int32 Padding = 2) const {
    return (X - Padding < Other.X + Other.Width) &&
           (X + Width + Padding > Other.X) &&
           (Y - Padding < Other.Y + Other.Height) &&
           (Y + Height + Padding > Other.Y);
  }
};

struct ECHOESOFTHECATACOMB_API FDungeonGameState {
  static bool HasGem(int32 GemIndex);
  static void CollectGem(int32 GemIndex);
  static int32 GetGemCount();
  static bool IsAltarActivated();
  static void SetAltarActivated(bool bActive);
  static bool HasPlayerWon();
  static void SetPlayerWon(bool bWon);
  static FString GetPromptText();
  static void SetPromptText(const FString &InPrompt);
  static void Reset();
};

UCLASS(Blueprintable, ClassGroup = (Dungeon),
       meta = (BlueprintSpawnableComponent))
class ECHOESOFTHECATACOMB_API ADungeonGenerator : public AActor {
  GENERATED_BODY()

public:
  ADungeonGenerator();

  virtual void BeginPlay() override;
  virtual void OnConstruction(const FTransform &Transform) override;
  virtual void PostLoad() override;
  virtual void PostActorCreated() override;

#if WITH_EDITOR
  virtual void
  PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
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

  /** Bakes all instanced components into individual StaticMeshActors for
   * granular per-mesh editing */
  UFUNCTION(BlueprintCallable, CallInEditor, Category = "Dungeon|Baking")
  void BakeToStaticMeshActors();

  /** Clears all baked StaticMeshActors from the level */
  UFUNCTION(BlueprintCallable, CallInEditor, Category = "Dungeon|Baking")
  void ClearBakedActors();

  /** Randomizes and respawns the boss encounters across all 3 arenas */
  UFUNCTION(BlueprintCallable, CallInEditor, Category = "Dungeon|Boss")
  void RandomizeBosses();

  // --- Configurable Settings ---

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings")
  int32 TileSize;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings",
            meta = (ClampMin = "15", ClampMax = "63"))
  int32 GridSize;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings",
            meta = (ClampMin = "3", ClampMax = "7"))
  int32 MinRoomSize;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings",
            meta = (ClampMin = "3", ClampMax = "9"))
  int32 MaxRoomSize;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings")
  int32 Seed;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings")
  bool bRandomizeSeed;

  /** When enabled, dungeon generation immediately converts instances into
   * individual selectable StaticMeshActors */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings")
  bool bSpawnAsIndividualActors;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings",
            meta = (ClampMin = "0.1", ClampMax = "1.0"))
  float MazeDensity;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings",
            meta = (ClampMin = "1", ClampMax = "3"))
  int32 CorridorWidth;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings")
  float WallHeight;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings")
  int32 RoomPadding;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings",
            meta = (ClampMin = "0.0", ClampMax = "0.5"))
  float LoopProbability;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float DecorativeWallRatio;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Settings")
  int32 TorchInterval;

  // --- 3-Stage Canon Event Locations ---

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Locations")
  FVector PlayerSpawnLocation;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|BossRooms")
  FVector Boss1RoomLocation;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|BossRooms")
  FVector Boss1SpawnLocation;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|BossRooms")
  FVector Boss1EntranceDoorLocation;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|BossRooms")
  FVector Boss1ExitDoorLocation;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|BossRooms")
  FVector Boss2RoomLocation;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|BossRooms")
  FVector Boss2SpawnLocation;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|BossRooms")
  FVector Boss2EntranceDoorLocation;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|BossRooms")
  FVector Boss2ExitDoorLocation;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|BossRooms")
  FVector Boss3RoomLocation;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|BossRooms")
  FVector Boss3SpawnLocation;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|BossRooms")
  FVector Boss3EntranceDoorLocation;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|BossRooms")
  FVector Boss3ExitDoorLocation;

  // Backward-compatible location aliases
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

  UFUNCTION(BlueprintPure, Category = "Dungeon|BossRooms")
  FVector GetBoss1SpawnLocation() const { return Boss1SpawnLocation; }

  UFUNCTION(BlueprintPure, Category = "Dungeon|BossRooms")
  FVector GetBoss2SpawnLocation() const { return Boss2SpawnLocation; }

  UFUNCTION(BlueprintPure, Category = "Dungeon|BossRooms")
  FVector GetBoss3SpawnLocation() const { return Boss3SpawnLocation; }

  // --- Boss Defeat Tracking & Detection ---

  UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Dungeon|BossRooms")
  bool bBoss1Defeated = false;

  UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Dungeon|BossRooms")
  bool bBoss2Defeated = false;

  UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Dungeon|BossRooms")
  bool bBoss3Defeated = false;

  /** Sets defeat state for a specific boss (1, 2, or 3) */
  UFUNCTION(BlueprintCallable, Category = "Dungeon|BossRooms")
  void SetBossDefeated(int32 BossIndex, bool bDefeated = true);

  /** Checks whether a specific boss (1, 2, or 3) has been defeated */
  UFUNCTION(BlueprintPure, Category = "Dungeon|BossRooms")
  bool IsBossDefeated(int32 BossIndex) const;

  void SpawnBossGemDrop(int32 BossIndex);
  void SpawnExitAltar();
  void ActivateExitAltar();
  void SpawnVictoryPortal();

  /** Returns which boss room (1, 2, 3) the world location is in, or 0 if not in
   * any boss room */
  UFUNCTION(BlueprintPure, Category = "Dungeon|BossRooms")
  int32 GetBossRoomAtLocation(const FVector &WorldLocation) const;

  /** Returns true if the world location is inside ANY of the 3 boss arenas */
  UFUNCTION(BlueprintPure, Category = "Dungeon|BossRooms")
  bool IsInsideBossRoom(const FVector &WorldLocation) const;

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
  TSoftObjectPtr<UStaticMesh> PillarMesh;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
  TSoftObjectPtr<UStaticMesh> DoorwayMesh;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
  TSoftObjectPtr<UStaticMesh> AltarMesh;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
  TSoftObjectPtr<UStaticMesh> CoffinMesh;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
  TSoftObjectPtr<UStaticMesh> PotMesh;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Meshes")
  TSoftObjectPtr<UStaticMesh> StatueMesh;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Blueprints")
  TSubclassOf<AActor> TorchBlueprintClass;

  // --- Procedural Traps ---

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Traps")
  TSubclassOf<class ASpikeTrap> SpikeTrapClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Traps")
  float TrapDensitySector1 = 0.25f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Traps")
  float TrapDensitySector2 = 0.38f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Traps")
  float TrapDensitySector3 = 0.50f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Traps")
  float PeriodicTrapRatio = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Traps")
  TSubclassOf<class AWallDartTrap> WallDartTrapClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Traps")
  float WallDartTrapRatio = 0.35f;

  // --- Lighting & Atmosphere ---

  /** Brightness intensity of each dungeon torch in Lumens (default: 2500.0f) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting",
            meta = (ClampMin = "100.0", ClampMax = "10000.0"))
  float TorchIntensity = 2500.0f;

  /** Distance the torch light reaches along the corridors (default: 950.0f cm) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting",
            meta = (ClampMin = "200.0", ClampMax = "3000.0"))
  float TorchAttenuationRadius = 950.0f;

  /** Volumetric scattering intensity in fog (default: 0.0f - stops blinding fog glare and wall bleed) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float TorchVolumetricScattering = 0.0f;

  /** Overall volumetric catacomb fog density (default: 0.003f) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting",
            meta = (ClampMin = "0.0", ClampMax = "0.05"))
  float FogDensity = 0.003f;

  /** Bloom glow intensity around torch fire (default: 0.30f) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting",
            meta = (ClampMin = "0.0", ClampMax = "2.0"))
  float BloomIntensity = 0.30f;

  /** Bloom luminance threshold (default: 2.0f - prevents walls and ceilings from blooming into glare) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting",
            meta = (ClampMin = "0.5", ClampMax = "10.0"))
  float BloomThreshold = 2.0f;

  /** Visual brightness and emissive power of the torch fire flame (default: 0.60f) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting",
            meta = (ClampMin = "0.01", ClampMax = "2.0"))
  float TorchFlameIntensity = 0.60f;

  /** Physical size scale of the torch fire flame particle effect (default: 0.85f - fits cleanly on torch sconce) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting",
            meta = (ClampMin = "0.1", ClampMax = "2.0"))
  float TorchFlameScale = 0.85f;

  /** Shadow bias for torch point lights (default: 0.30f - higher values shrink torch holder bracket shadows) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting",
            meta = (ClampMin = "0.01", ClampMax = "1.0"))
  float TorchShadowBias = 0.30f;

  /** Source radius of the torch flame (default: 28.0f - larger area softens and diminishes holder shadow footprint) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting",
            meta = (ClampMin = "1.0", ClampMax = "100.0"))
  float TorchSourceRadius = 28.0f;

  /** Whether the torch wall bracket mesh itself casts shadows (default: false; reduces torch holder shadow) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting")
  bool bTorchHolderCastShadow = false;

  /** Whether corridor torches cast dynamic shadows (default: false for 60+ FPS; true for heavy shadows) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting")
  bool bCorridorTorchesCastShadow = false;

  /** Brightness intensity of Boss Arena central ceiling light in Lumens (default: 16000.0f) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting",
            meta = (ClampMin = "1000.0", ClampMax = "50000.0"))
  float BossArenaLightIntensity = 16000.0f;

  /** Distance the boss arena central light reaches (default: 2600.0f cm) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting",
            meta = (ClampMin = "500.0", ClampMax = "5000.0"))
  float BossArenaLightRadius = 2600.0f;

  /** Brightness intensity of perimeter wall torches inside Boss Rooms (default: 5000.0f Lumens) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting",
            meta = (ClampMin = "500.0", ClampMax = "20000.0"))
  float BossTorchIntensity = 5000.0f;

  /** Attenuation radius of torches inside Boss Rooms (default: 1800.0f cm) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Lighting",
            meta = (ClampMin = "500.0", ClampMax = "3000.0"))
  float BossTorchAttenuationRadius = 1800.0f;

  /** Button: Click in Details panel to instantly update all torches, flames,
   * fog, and bloom in real time */
  UFUNCTION(CallInEditor, Category = "Dungeon|Lighting")
  void UpdateLightingNow();

  // --- Boss Encounters ---

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Boss")
  TSubclassOf<ABossMinotaur> BossMinotaurClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Boss")
  TSubclassOf<ABossGrux> BossGruxClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Boss")
  TSubclassOf<ABossShadowKnight> BossShadowKnightClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Boss")
  TSubclassOf<ABossArenaGate> BossArenaGateClass;

  /** If true, randomly shuffles which boss appears in each arena (default: true) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Boss")
  bool bRandomizeBossRoster = true;

  /** If true, allows duplicate boss types across rooms instead of a unique shuffle (default: false) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Boss")
  bool bAllowDuplicateBosses = false;

  // --- Instanced Static Mesh Components ---

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
  UInstancedStaticMeshComponent *FloorInstances;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
  UInstancedStaticMeshComponent *CeilingInstances;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
  UInstancedStaticMeshComponent *RoofInstances;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
  UInstancedStaticMeshComponent *WallInstances;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
  UInstancedStaticMeshComponent *DecorativeWallInstances;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
  UInstancedStaticMeshComponent *WallTrimInstances;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
  UInstancedStaticMeshComponent *PillarInstances;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
  UInstancedStaticMeshComponent *DoorwayInstances;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
  UInstancedStaticMeshComponent *TorchInstances;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
  UInstancedStaticMeshComponent *AltarInstances;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
  UInstancedStaticMeshComponent *CoffinInstances;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
  UInstancedStaticMeshComponent *PotInstances;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Components")
  UInstancedStaticMeshComponent *StatueInstances;

protected:
  /** Deterministic pseudo-random number generator for seeded levels */
  FRandomStream RandomStream;

  /** 2D grid storing cell types */
  TArray<EDungeonCellType> Grid;

  /** Placed rooms */
  TArray<FDungeonRoom> Rooms;

  /** Room indices for the 3 boss arenas */
  int32 Boss1RoomIndex = INDEX_NONE;
  int32 Boss2RoomIndex = INDEX_NONE;
  int32 Boss3RoomIndex = INDEX_NONE;

  /** Set of cells blocked by props */
  TSet<FIntPoint> PropBlockedCells;

  /** Weak pointers to spawned torch actors */
  UPROPERTY()
  TArray<TWeakObjectPtr<AActor>> SpawnedTorches;

  /** Weak pointers to spawned trap actors */
  UPROPERTY()
  TArray<TWeakObjectPtr<AActor>> SpawnedTraps;

  /** Weak pointer to spawned Boss 1 (Malikor / Room 1) */
  UPROPERTY()
  TWeakObjectPtr<AActor> SpawnedBoss1;

  /** Weak pointer to spawned Boss 2 (Kragthor / Room 2) */
  UPROPERTY()
  TWeakObjectPtr<AActor> SpawnedBoss2;

  /** Weak pointer to spawned Boss 3 (Asterion / Room 3) */
  UPROPERTY()
  TWeakObjectPtr<AActor> SpawnedBoss3;

  /** Weak pointers to spawned arena gates */
  UPROPERTY()
  TArray<TWeakObjectPtr<AActor>> SpawnedArenaGates;

  TArray<TWeakObjectPtr<AActor>> SpawnedGemPickups;
  TWeakObjectPtr<AActor> SpawnedAltarActor;
  TWeakObjectPtr<AActor> SpawnedVictoryPortal;

  /** Weak pointers to baked static mesh actors */
  UPROPERTY()
  TArray<TWeakObjectPtr<AActor>> BakedActors;

  // Internal generation pipeline methods
  void InitializeComponents();
  void EnsureMeshesLoaded();
  bool GenerateDungeonLayout();
  void BuildBossArenas();
  void CarveCryptChamber(int32 OriginX, int32 OriginY, int32 W, int32 H);
  void CarveSectorMaze(int32 MinX, int32 MaxX, int32 MinY, int32 MaxY,
                       FIntPoint StartCell, FIntPoint EndCell);
  void ConnectPointToNearestCorridor(FIntPoint Pt, int32 MinX, int32 MaxX,
                                     int32 MinY, int32 MaxY);
  void AddSectorLoops(int32 MinX, int32 MaxX, int32 MinY, int32 MaxY,
                      float LoopProb);
  void BuildGeometryInstances();
  void PlaceWallPillars();
  void PlaceEnvironmentalProps();
  void SetupCatacombAtmosphere();
  void ApplyLightingSettings();
  void PlaceBossRoomTorches(const FDungeonRoom &Room);
  void PlaceMazeCorridorTorches();
  void ClearSpawnedTorches();
  void PlaceProceduralTraps();
  void ClearSpawnedTraps();
  void SpawnBossEncounters();
  void ClearBossEncounters();
  void SpawnTorchActor(const FTransform &Transform);
  void SpawnBossArenaLight(const FVector &Location, float Intensity,
                           float Radius, FLinearColor Color,
                           float SourceRadius = 80.0f);
  bool SelectAndSetPlayerSpawn();

  bool IsNearDoorway(const FVector &Location, float Threshold = 500.0f) const;
  bool IsNearPropOrAltar(const FVector &Location,
                         float Threshold = 500.0f) const;
  bool IsInsidePlayableMaze(int32 X, int32 Y) const;

public:
  // Grid Helper Methods
  FORCEINLINE int32 GetGridIndex(int32 X, int32 Y) const {
    return Y * GridSize + X;
  }

  FORCEINLINE bool IsValidCell(int32 X, int32 Y) const {
    return X >= 0 && X < GridSize && Y >= 0 && Y < GridSize;
  }

  FORCEINLINE EDungeonCellType GetCell(int32 X, int32 Y) const {
    if (!IsValidCell(X, Y))
      return EDungeonCellType::Solid;
    return Grid[GetGridIndex(X, Y)];
  }

  FORCEINLINE void SetCell(int32 X, int32 Y, EDungeonCellType Type) {
    if (IsValidCell(X, Y)) {
      Grid[GetGridIndex(X, Y)] = Type;
    }
  }

  FORCEINLINE bool IsWalkable(int32 X, int32 Y) const {
    if (!IsValidCell(X, Y))
      return false;
    return Grid[GetGridIndex(X, Y)] != EDungeonCellType::Solid;
  }

  FORCEINLINE FVector GridToWorldCenter(int32 X, int32 Y,
                                        float Z = 0.0f) const {
    const float FTile = (float)TileSize;
    return FVector((X + 0.5f) * FTile, (Y + 0.5f) * FTile, Z);
  }
};
