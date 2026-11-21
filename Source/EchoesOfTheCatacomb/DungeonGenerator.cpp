

#include "DungeonGenerator.h"
#include "BossArenaGate.h"
#include "BossGemPickup.h"
#include "BossGrux.h"
#include "BossMinotaur.h"
#include "BossShadowKnight.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Queue.h"
#include "Distributions/DistributionVectorConstantCurve.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/Color/ParticleModuleColor.h"
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Particles/Light/ParticleModuleLight.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "SpikeTrap.h"
#include "UObject/UObjectThreadContext.h"
#include "WallDartTrap.h"

namespace {
static bool g_bCollectedGem1 = false;
static bool g_bCollectedGem2 = false;
static bool g_bCollectedGem3 = false;
static bool g_bAltarGemsPlaced = false;
static bool g_bPlayerReachedVictory = false;
static FString g_CurrentDungeonPrompt;
} // namespace

bool FDungeonGameState::HasGem(int32 GemIndex) {
  switch (GemIndex) {
  case 1:
    return g_bCollectedGem1;
  case 2:
    return g_bCollectedGem2;
  case 3:
    return g_bCollectedGem3;
  default:
    return false;
  }
}

void FDungeonGameState::CollectGem(int32 GemIndex) {
  switch (GemIndex) {
  case 1:
    g_bCollectedGem1 = true;
    break;
  case 2:
    g_bCollectedGem2 = true;
    break;
  case 3:
    g_bCollectedGem3 = true;
    break;
  default:
    break;
  }
}

int32 FDungeonGameState::GetGemCount() {
  int32 Count = 0;
  if (g_bCollectedGem1)
    Count++;
  if (g_bCollectedGem2)
    Count++;
  if (g_bCollectedGem3)
    Count++;
  return Count;
}

bool FDungeonGameState::IsAltarActivated() { return g_bAltarGemsPlaced; }

void FDungeonGameState::SetAltarActivated(bool bActive) {
  g_bAltarGemsPlaced = bActive;
}

bool FDungeonGameState::HasPlayerWon() { return g_bPlayerReachedVictory; }

void FDungeonGameState::SetPlayerWon(bool bWon) {
  g_bPlayerReachedVictory = bWon;
}

FString FDungeonGameState::GetPromptText() { return g_CurrentDungeonPrompt; }

void FDungeonGameState::SetPromptText(const FString &InPrompt) {
  g_CurrentDungeonPrompt = InPrompt;
}

void FDungeonGameState::Reset() {
  g_bCollectedGem1 = false;
  g_bCollectedGem2 = false;
  g_bCollectedGem3 = false;
  g_bAltarGemsPlaced = false;
  g_bPlayerReachedVictory = false;
  g_CurrentDungeonPrompt.Empty();
}

ADungeonGenerator::ADungeonGenerator() {
  PrimaryActorTick.bCanEverTick = false;
  bReplicates = false;

  USceneComponent *SceneRoot =
      CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
  SceneRoot->SetMobility(EComponentMobility::Static);
  RootComponent = SceneRoot;

  // Default Generation Settings
  TileSize = 500;
  GridSize = 31;
  MinRoomSize = 5;
  MaxRoomSize = 7;
  Seed = 821797;
  bRandomizeSeed = false;
  bSpawnAsIndividualActors = false;
  MazeDensity = 0.85f;
  CorridorWidth = 1;
  WallHeight = 450.0f;
  RoomPadding = 2;
  LoopProbability = 0.12f;
  DecorativeWallRatio = 0.25f;
  TorchInterval = 4;
  TorchIntensity = 4500.0f;
  TorchAttenuationRadius = 1500.0f;
  TorchVolumetricScattering = 0.0f;
  FogDensity = 0.003f;
  BloomIntensity = 0.30f;
  BloomThreshold = 2.0f;
  TorchFlameIntensity = 0.60f;
  TorchFlameScale = 0.85f;
  TorchShadowBias = 0.30f;
  TorchSourceRadius = 28.0f;
  bTorchHolderCastShadow = true;
  BossArenaLightIntensity = 16000.0f;
  BossArenaLightRadius = 2600.0f;
  BossTorchIntensity = 5000.0f;
  BossTorchAttenuationRadius = 1800.0f;

  // Asset Paths from MedievalDungeon
  FloorMesh = TSoftObjectPtr<UStaticMesh>(
      FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/"
                           "SM_Crypt_Floor.SM_Crypt_Floor")));
  CeilingMesh = TSoftObjectPtr<UStaticMesh>(
      FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/"
                           "SM_Crypt_Ceiling_Flat.SM_Crypt_Ceiling_Flat")));
  WallMesh = TSoftObjectPtr<UStaticMesh>(
      FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/"
                           "SM_Crypt_Wall.SM_Crypt_Wall")));
  DecorativeWallMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
      TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/"
           "SM_Crypt_Wall_Decorative_A.SM_Crypt_Wall_Decorative_A")));
  WallTrimMesh = TSoftObjectPtr<UStaticMesh>(
      FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/"
                           "SM_Crypt_Wall_Floor_Trim.SM_Crypt_Wall_Floor_Trim")));
  PillarMesh = TSoftObjectPtr<UStaticMesh>(
      FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/"
                           "SM_Crypt_Pillar.SM_Crypt_Pillar")));
  DoorwayMesh = TSoftObjectPtr<UStaticMesh>(
      FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/"
                           "SM_Crypt_Doorway.SM_Crypt_Doorway")));
  AltarMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
      TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Alter.SM_Alter")));
  CoffinMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
      TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Coffin.SM_Coffin")));
  PotMesh = TSoftObjectPtr<UStaticMesh>(
      FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Props/"
                           "SM_Pot_A_Complete.SM_Pot_A_Complete")));
  StatueMesh = TSoftObjectPtr<UStaticMesh>(
      FSoftObjectPath(TEXT("/Game/MedievalDungeon/Meshes/Props/"
                           "SM_Gargoyle_Statue.SM_Gargoyle_Statue")));

  static ConstructorHelpers::FClassFinder<AActor> TorchBPClass(
      TEXT("/Game/MedievalDungeon/Blueprints/BP_Torch"));
  if (TorchBPClass.Succeeded()) {
    TorchBlueprintClass = TorchBPClass.Class;
  }

  SpikeTrapClass = ASpikeTrap::StaticClass();
  WallDartTrapClass = AWallDartTrap::StaticClass();
  BossMinotaurClass = ABossMinotaur::StaticClass();
  BossGruxClass = ABossGrux::StaticClass();
  BossShadowKnightClass = ABossShadowKnight::StaticClass();
  BossArenaGateClass = ABossArenaGate::StaticClass();
  bRandomizeBossRoster = true;
  bAllowDuplicateBosses = false;
  bTorchHolderCastShadow = false;
  bCorridorTorchesCastShadow = false;
  TrapDensitySector1 = 0.25f;
  TrapDensitySector2 = 0.38f;
  TrapDensitySector3 = 0.50f;
  WallDartTrapRatio = 0.35f;

  InitializeComponents();
}

void ADungeonGenerator::InitializeComponents() {
  auto CreateISM =
      [this](const TCHAR *CompName) -> UInstancedStaticMeshComponent * {
    UInstancedStaticMeshComponent *Comp =
        CreateDefaultSubobject<UInstancedStaticMeshComponent>(CompName);
    if (Comp) {
      Comp->SetupAttachment(RootComponent);
      Comp->SetMobility(EComponentMobility::Static);
      Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
      Comp->SetCollisionObjectType(ECC_WorldStatic);
      Comp->SetCollisionResponseToAllChannels(ECR_Block);
      Comp->SetGenerateOverlapEvents(false);
      Comp->bCastDynamicShadow = true;
      Comp->CastShadow = true;
      Comp->bCastShadowAsTwoSided = true;
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

void ADungeonGenerator::OnConstruction(const FTransform &Transform) {
  Super::OnConstruction(Transform);
  // Do not call GenerateDungeon() here: spawning actors inside OnConstruction
  // causes Unreal Engine to automatically parent all spawned actors under
  // DungeonGenerator.
}

void ADungeonGenerator::PostLoad() {
  Super::PostLoad();
  // Do not call GenerateDungeon() during PostLoad: Unreal Engine forbids
  // spawning actors or executing Blueprint AnimInstances while routing
  // PostLoad.
}

void ADungeonGenerator::PostActorCreated() {
  Super::PostActorCreated();
  if (GetWorld() && !GetWorld()->IsGameWorld()) {
    GenerateDungeon();
  }
}

#if WITH_EDITOR
void ADungeonGenerator::PostEditChangeProperty(
    FPropertyChangedEvent &PropertyChangedEvent) {
  Super::PostEditChangeProperty(PropertyChangedEvent);

  FName PropertyName = (PropertyChangedEvent.Property != nullptr)
                           ? PropertyChangedEvent.Property->GetFName()
                           : NAME_None;

  if (PropertyName ==
          GET_MEMBER_NAME_CHECKED(ADungeonGenerator, TorchIntensity) ||
      PropertyName ==
          GET_MEMBER_NAME_CHECKED(ADungeonGenerator, TorchAttenuationRadius) ||
      PropertyName == GET_MEMBER_NAME_CHECKED(ADungeonGenerator,
                                              TorchVolumetricScattering) ||
      PropertyName ==
          GET_MEMBER_NAME_CHECKED(ADungeonGenerator, TorchFlameIntensity) ||
      PropertyName ==
          GET_MEMBER_NAME_CHECKED(ADungeonGenerator, TorchFlameScale) ||
      PropertyName == GET_MEMBER_NAME_CHECKED(ADungeonGenerator, FogDensity) ||
      PropertyName ==
          GET_MEMBER_NAME_CHECKED(ADungeonGenerator, BloomIntensity) ||
      PropertyName ==
          GET_MEMBER_NAME_CHECKED(ADungeonGenerator, BloomThreshold) ||
      PropertyName ==
          GET_MEMBER_NAME_CHECKED(ADungeonGenerator, TorchShadowBias) ||
      PropertyName ==
          GET_MEMBER_NAME_CHECKED(ADungeonGenerator, TorchSourceRadius) ||
      PropertyName ==
          GET_MEMBER_NAME_CHECKED(ADungeonGenerator, bTorchHolderCastShadow) ||
      PropertyName ==
          GET_MEMBER_NAME_CHECKED(ADungeonGenerator,
                                  BossArenaLightIntensity) ||
      PropertyName ==
          GET_MEMBER_NAME_CHECKED(ADungeonGenerator, BossArenaLightRadius) ||
      PropertyName ==
          GET_MEMBER_NAME_CHECKED(ADungeonGenerator, BossTorchIntensity) ||
      PropertyName ==
          GET_MEMBER_NAME_CHECKED(ADungeonGenerator,
                                  BossTorchAttenuationRadius)) {
    ApplyLightingSettings();
    return;
  }

  GenerateDungeon();

  TArray<AActor *> Attached;
  GetAttachedActors(Attached);
  for (AActor *Act : Attached) {
    if (Act) {
      Act->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
      Act->SetFolderPath(FName(TEXT("Dungeon_Torches")));
    }
  }
}
#endif

void ADungeonGenerator::BeginPlay() {
  Super::BeginPlay();

  // Disable individual actor baking at runtime to preserve 60+ FPS via ISMC instances
  bSpawnAsIndividualActors = false;
  ClearBakedActors();

  // Fresh procedural seed on every playthrough so room placements, corridors, traps, and boss roster are randomized
  Seed = FMath::RandRange(1, 9999999);
  bRandomizeSeed = true;
  RandomStream.Initialize(Seed);

  GenerateDungeon();

  auto TeleportPlayer = [this]() {
    UWorld *World = GetWorld();
    if (!World)
      return;
    APawn *PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
    if (PlayerPawn && !PlayerSpawnLocation.IsZero()) {
      if (ACharacter *PlayerChar = Cast<ACharacter>(PlayerPawn)) {
        if (UCharacterMovementComponent *MoveComp =
                PlayerChar->GetCharacterMovement()) {
          MoveComp->StopMovementImmediately();
          MoveComp->Velocity = FVector::ZeroVector;
        }
      }
      PlayerPawn->TeleportTo(PlayerSpawnLocation,
                             FRotator(0.0f, 90.0f, 0.0f), false, true);
      if (AController *C = PlayerPawn->GetController()) {
        C->SetControlRotation(FRotator(0.0f, 90.0f, 0.0f));
      }
    }
  };

  TeleportPlayer();

  // Also re-verify on next frame in case PlayerController possession finishes after BeginPlay
  if (UWorld *World = GetWorld()) {
    World->GetTimerManager().SetTimerForNextTick([TeleportPlayer]() {
      TeleportPlayer();
    });
  }
}

void ADungeonGenerator::EnsureMeshesLoaded() {
  if (!TorchBlueprintClass) {
    TorchBlueprintClass = StaticLoadClass(
        AActor::StaticClass(), nullptr,
        TEXT("/Game/MedievalDungeon/Blueprints/BP_Torch.BP_Torch_C"));
  }

  auto AssignMesh = [](UInstancedStaticMeshComponent *Comp,
                       TSoftObjectPtr<UStaticMesh> &SoftMesh) {
    if (Comp) {
      if (SoftMesh.IsValid()) {
        UStaticMesh *Loaded = SoftMesh.Get();
        if (!Loaded) {
          Loaded = SoftMesh.LoadSynchronous();
        }
        if (Loaded) {
          Comp->SetStaticMesh(Loaded);
        }
      }
      Comp->SetVisibility(true);
      Comp->SetHiddenInGame(false);
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

void ADungeonGenerator::GenerateDungeon() {
  if (FUObjectThreadContext::Get().IsRoutingPostLoad) {
    return;
  }

  if (bRandomizeSeed) {
    Seed = FMath::RandRange(1, 9999999);
  }
  RandomStream.Initialize(Seed);

  EnsureMeshesLoaded();
  ClearDungeon();

  if (GenerateDungeonLayout()) {
    BuildGeometryInstances();
    PlaceEnvironmentalProps();
    ApplyLightingSettings();
    PlaceProceduralTraps();
    SpawnBossEncounters();
    SelectAndSetPlayerSpawn();
  }

  UE_LOG(LogTemp, Log,
         TEXT("=================================================="));
  UE_LOG(LogTemp, Log,
         TEXT("     3-STAGE CANON EVENT DUNGEON GENERATED        "));
  UE_LOG(LogTemp, Log,
         TEXT("=================================================="));
  UE_LOG(LogTemp, Log, TEXT("Seed:                    %d"), Seed);
  UE_LOG(LogTemp, Log, TEXT("Player Spawn Location:   %s"),
         *PlayerSpawnLocation.ToString());
  UE_LOG(LogTemp, Log, TEXT("Boss 1 Room Location:    %s"),
         *Boss1RoomLocation.ToString());
  UE_LOG(LogTemp, Log, TEXT("  - Boss 1 Spawn:        %s"),
         *Boss1SpawnLocation.ToString());
  UE_LOG(LogTemp, Log, TEXT("  - Boss 1 Entrance:     %s"),
         *Boss1EntranceDoorLocation.ToString());
  UE_LOG(LogTemp, Log, TEXT("  - Boss 1 Exit:         %s"),
         *Boss1ExitDoorLocation.ToString());
  UE_LOG(LogTemp, Log, TEXT("Boss 2 Room Location:    %s"),
         *Boss2RoomLocation.ToString());
  UE_LOG(LogTemp, Log, TEXT("  - Boss 2 Spawn:        %s"),
         *Boss2SpawnLocation.ToString());
  UE_LOG(LogTemp, Log, TEXT("  - Boss 2 Entrance:     %s"),
         *Boss2EntranceDoorLocation.ToString());
  UE_LOG(LogTemp, Log, TEXT("  - Boss 2 Exit:         %s"),
         *Boss2ExitDoorLocation.ToString());
  UE_LOG(LogTemp, Log, TEXT("Boss 3 Room Location:    %s"),
         *Boss3RoomLocation.ToString());
  UE_LOG(LogTemp, Log, TEXT("  - Boss 3 Spawn:        %s"),
         *Boss3SpawnLocation.ToString());
  UE_LOG(LogTemp, Log, TEXT("  - Boss 3 Entrance:     %s"),
         *Boss3EntranceDoorLocation.ToString());
  UE_LOG(LogTemp, Log, TEXT("  - Boss 3 Final Exit:   %s"),
         *Boss3ExitDoorLocation.ToString());
  UE_LOG(LogTemp, Log,
         TEXT("=================================================="));

  if (bSpawnAsIndividualActors) {
    UWorld *World = GetWorld();
    if (World && !World->IsGameWorld()) {
      BakeToStaticMeshActors();
    }
  }

#if WITH_EDITOR
  for (TWeakObjectPtr<AActor> &TorchPtr : SpawnedTorches) {
    if (AActor *Torch = TorchPtr.Get()) {
      if (USceneComponent *Root = Torch->GetRootComponent()) {
        Root->DetachFromComponent(
            FDetachmentTransformRules::KeepWorldTransform);
      }
      Torch->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
      Torch->SetFolderPath(FName(TEXT("Dungeon_Torches")));
    }
  }
#endif
}

void ADungeonGenerator::RegenerateDungeon() { GenerateDungeon(); }

void ADungeonGenerator::RegenerateWithNewSeed() {
  Seed = FMath::RandRange(1000, 999999);
  GenerateDungeon();
}

void ADungeonGenerator::ClearDungeon() {
  if (FloorInstances)
    FloorInstances->ClearInstances();
  if (CeilingInstances)
    CeilingInstances->ClearInstances();
  if (RoofInstances)
    RoofInstances->ClearInstances();
  if (WallInstances)
    WallInstances->ClearInstances();
  if (DecorativeWallInstances)
    DecorativeWallInstances->ClearInstances();
  if (WallTrimInstances)
    WallTrimInstances->ClearInstances();
  if (DoorwayInstances)
    DoorwayInstances->ClearInstances();
  if (PillarInstances)
    PillarInstances->ClearInstances();
  if (TorchInstances)
    TorchInstances->ClearInstances();
  if (AltarInstances)
    AltarInstances->ClearInstances();
  if (CoffinInstances)
    CoffinInstances->ClearInstances();
  if (PotInstances)
    PotInstances->ClearInstances();
  if (StatueInstances)
    StatueInstances->ClearInstances();

  Grid.Empty();
  Rooms.Empty();
  PropBlockedCells.Empty();

  Boss1RoomIndex = INDEX_NONE;
  Boss2RoomIndex = INDEX_NONE;
  Boss3RoomIndex = INDEX_NONE;

  ClearSpawnedTorches();
  ClearSpawnedTraps();
  ClearBossEncounters();
  ClearBakedActors();
}

void ADungeonGenerator::ClearSpawnedTorches() {
  UWorld *World = GetWorld();
  if (!World)
    return;

  TSet<AActor *> TorchesToDestroy;

  for (TWeakObjectPtr<AActor> &TorchPtr : SpawnedTorches) {
    if (TorchPtr.IsValid()) {
      TorchesToDestroy.Add(TorchPtr.Get());
    }
  }
  SpawnedTorches.Empty();

  TArray<AActor *> AttachedActors;
  GetAttachedActors(AttachedActors);
  for (AActor *Child : AttachedActors) {
    if (IsValid(Child) &&
        (Child->ActorHasTag(TEXT("DungeonTorch")) ||
         (TorchBlueprintClass && Child->IsA(TorchBlueprintClass)))) {
      TorchesToDestroy.Add(Child);
    }
  }

  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Actor = *It;
    if (IsValid(Actor) && Actor != this) {
      bool bIsTorch = Actor->ActorHasTag(TEXT("DungeonTorch")) ||
                      (TorchBlueprintClass && Actor->IsA(TorchBlueprintClass));
#if WITH_EDITOR
      if (!bIsTorch) {
        FName Folder = Actor->GetFolderPath();
        FString Label = Actor->GetActorLabel();
        if (Folder == FName(TEXT("Dungeon_Torches")) ||
            Label.StartsWith(TEXT("Torch_")) ||
            Label.StartsWith(TEXT("BP_Torch")) ||
            Label.StartsWith(TEXT("BossArenaLight"))) {
          bIsTorch = true;
        }
      }
#endif
      if (bIsTorch) {
        TorchesToDestroy.Add(Actor);
      }
    }
  }

  for (AActor *Torch : TorchesToDestroy) {
    if (IsValid(Torch)) {
      // Safely detach first if attached to prevent detachment assertion
      if (Torch->GetAttachParentActor()) {
        Torch->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
      }
#if WITH_EDITOR
      if (World && !World->IsGameWorld()) {
        World->EditorDestroyActor(Torch, true);
        continue;
      }
#endif
      Torch->Destroy();
    }
  }
}

void ADungeonGenerator::UpdateLightingNow() { ApplyLightingSettings(); }

void ADungeonGenerator::ApplyLightingSettings() {
  UWorld *World = GetWorld();
  if (!World) {
    return;
  }

  // Auto-upgrade legacy low-illumination values saved from previous session
  if (TorchIntensity < 4000.0f) {
    TorchIntensity = 4500.0f;
  }
  if (TorchAttenuationRadius < 1400.0f) {
    TorchAttenuationRadius = 1500.0f;
  }
  if (TorchFlameIntensity < 0.3f) {
    TorchFlameIntensity = 0.60f;
  }
  if (TorchFlameScale < 0.5f) {
    TorchFlameScale = 0.85f;
  }

  // 1. Scale the flame emissive curve on P_Torch_Fire (Cascade particle system)
  UParticleSystem *FireTemplate = nullptr;
  if (TorchBlueprintClass) {
    if (AActor *CDO = TorchBlueprintClass.GetDefaultObject()) {
      TArray<UParticleSystemComponent *> DefaultPSCs;
      CDO->GetComponents<UParticleSystemComponent>(DefaultPSCs);
      for (UParticleSystemComponent *DefPSC : DefaultPSCs) {
        if (DefPSC && DefPSC->Template) {
          FireTemplate = DefPSC->Template;
          break;
        }
      }
    }
  }
  if (!FireTemplate) {
    FireTemplate = Cast<UParticleSystem>(
        StaticLoadObject(UParticleSystem::StaticClass(), nullptr,
                         TEXT("/Game/MedievalDungeon/Particles/P_Torch_Fire")));
  }

  if (FireTemplate) {
    static TArray<FVector> OriginalColorPoints;
    static TArray<FVector> OriginalInitialColorPoints;

    for (UParticleEmitter *Emitter : FireTemplate->Emitters) {
      if (!Emitter)
        continue;
      for (UParticleLODLevel *LOD : Emitter->LODLevels) {
        if (!LOD)
          continue;
        for (UParticleModule *Mod : LOD->Modules) {
          if (!Mod)
            continue;

          // Disable ParticleModuleLight so it does not spawn redundant point
          // lights
          if (Mod->IsA<UParticleModuleLight>()) {
            Mod->bEnabled = false;
          }

          // Scale ColorOverLife curve
          if (UParticleModuleColorOverLife *ColorMod =
                  Cast<UParticleModuleColorOverLife>(Mod)) {
            if (UDistributionVectorConstantCurve *Curve =
                    Cast<UDistributionVectorConstantCurve>(
                        ColorMod->ColorOverLife.Distribution)) {
              if (OriginalColorPoints.Num() == 0) {
                for (const FInterpCurvePoint<FVector> &Pt :
                     Curve->ConstantCurve.Points) {
                  OriginalColorPoints.Add(Pt.OutVal);
                }
              }
              for (int32 i = 0; i < Curve->ConstantCurve.Points.Num() &&
                                i < OriginalColorPoints.Num();
                   ++i) {
                Curve->ConstantCurve.Points[i].OutVal =
                    OriginalColorPoints[i] * TorchFlameIntensity;
              }
            }
          }

          // Scale Initial Color curve if present
          if (UParticleModuleColor *InitialColorMod =
                  Cast<UParticleModuleColor>(Mod)) {
            if (UDistributionVectorConstantCurve *Curve =
                    Cast<UDistributionVectorConstantCurve>(
                        InitialColorMod->StartColor.Distribution)) {
              if (OriginalInitialColorPoints.Num() == 0) {
                for (const FInterpCurvePoint<FVector> &Pt :
                     Curve->ConstantCurve.Points) {
                  OriginalInitialColorPoints.Add(Pt.OutVal);
                }
              }
              for (int32 i = 0; i < Curve->ConstantCurve.Points.Num() &&
                                i < OriginalInitialColorPoints.Num();
                   ++i) {
                Curve->ConstantCurve.Points[i].OutVal =
                    OriginalInitialColorPoints[i] * TorchFlameIntensity;
              }
            }
          }
        }
      }
    }
  }

  // 2. Update Atmosphere (Fog & Post-Processing)
  SetupCatacombAtmosphere();

  // 3. Find and update ALL torches in the World (both spawned and placed in
  // map)
  TArray<AActor *> AllTorches;
  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Act = *It;
    if (IsValid(Act) && Act != this) {
      bool bIsTorch = Act->ActorHasTag(TEXT("DungeonTorch")) ||
                      (TorchBlueprintClass && Act->IsA(TorchBlueprintClass));
#if WITH_EDITOR
      if (!bIsTorch) {
        FName Folder = Act->GetFolderPath();
        FString Label = Act->GetActorLabel();
        if (Folder == FName(TEXT("Dungeon_Torches")) ||
            Label.StartsWith(TEXT("Torch_")) ||
            Label.StartsWith(TEXT("BP_Torch")) ||
            Label.StartsWith(TEXT("BossArenaLight"))) {
          bIsTorch = true;
        }
      }
#endif
      if (bIsTorch) {
        AllTorches.Add(Act);
      }
    }
  }

  for (AActor *Torch : AllTorches) {
    if (!IsValid(Torch))
      continue;

    // Boss Arena Central Illuminators
    if (Torch->ActorHasTag(TEXT("BossArenaLight"))) {
      TArray<UPointLightComponent *> ArenaLights;
      Torch->GetComponents<UPointLightComponent>(ArenaLights);
      for (UPointLightComponent *LightComp : ArenaLights) {
        if (LightComp) {
          LightComp->SetIntensity(BossArenaLightIntensity);
          LightComp->SetAttenuationRadius(BossArenaLightRadius);
          LightComp->MaxDrawDistance = 5500.0f;
          LightComp->MaxDistanceFadeRange = 500.0f;
          LightComp->MarkRenderStateDirty();
        }
      }
      continue;
    }

    const bool bIsBossRoom =
        Torch->ActorHasTag(TEXT("BossRoomTorch")) ||
        IsInsideBossRoom(Torch->GetActorLocation());
    const float EffectiveIntensity =
        bIsBossRoom ? BossTorchIntensity : TorchIntensity;
    const float EffectiveRadius =
        bIsBossRoom ? BossTorchAttenuationRadius : TorchAttenuationRadius;

    TArray<UPointLightComponent *> PointLights;
    Torch->GetComponents<UPointLightComponent>(PointLights);
    for (UPointLightComponent *LightComp : PointLights) {
      if (LightComp) {
        LightComp->bUseInverseSquaredFalloff = true;
        LightComp->SetIntensity(EffectiveIntensity);
        LightComp->SetAttenuationRadius(EffectiveRadius);
        LightComp->SetSourceRadius(bIsBossRoom ? 35.0f : TorchSourceRadius);
        LightComp->SetSoftSourceRadius(
            (bIsBossRoom ? 35.0f : TorchSourceRadius) * 0.75f);
        LightComp->ShadowBias = TorchShadowBias;
        LightComp->ShadowSlopeBias = TorchShadowBias;
        LightComp->SetLightColor(bIsBossRoom
                                     ? FLinearColor(1.0f, 0.72f, 0.38f, 1.0f)
                                     : FLinearColor(1.0f, 0.65f, 0.28f, 1.0f));
        // Boss room perimeter torches act as fill lights; corridor torches use bCorridorTorchesCastShadow
        LightComp->SetCastShadows(bIsBossRoom ? false : bCorridorTorchesCastShadow);
        LightComp->bCastVolumetricShadow = false;
        LightComp->VolumetricScatteringIntensity = TorchVolumetricScattering;

        // Distance Culling: standard torches render visible up to 4500 cm (9 tiles)
        LightComp->MaxDrawDistance = 4500.0f;
        LightComp->MaxDistanceFadeRange = 500.0f;

        // Offset point light outward into room (+24 cm forward) to dramatically reduce holder shadow footprint
        FVector RelLoc = LightComp->GetRelativeLocation();
        if (RelLoc.X < 24.0f) {
          RelLoc.X = 24.0f;
          LightComp->SetRelativeLocation(RelLoc);
        }

        LightComp->MarkRenderStateDirty();
      }
    }

    TArray<UStaticMeshComponent *> MeshComps;
    Torch->GetComponents<UStaticMeshComponent>(MeshComps);
    for (UStaticMeshComponent *SMC : MeshComps) {
      if (SMC) {
        SMC->SetCastShadow(bTorchHolderCastShadow);
        SMC->bCastContactShadow = false;
        SMC->MarkRenderStateDirty();
      }
    }

    TArray<UParticleSystemComponent *> FireParticles;
    Torch->GetComponents<UParticleSystemComponent>(FireParticles);
    for (UParticleSystemComponent *PSC : FireParticles) {
      if (PSC) {
        const float EffectiveFlameScale =
            bIsBossRoom ? (TorchFlameScale * 1.25f) : TorchFlameScale;
        PSC->SetRelativeScale3D(FVector(EffectiveFlameScale));
        PSC->SetCullDistance(bIsBossRoom ? 6000.0f : 5000.0f);
        PSC->ResetParticles(true);
        PSC->Activate(true);
        PSC->MarkRenderStateDirty();
      }
    }
  }
}

void ADungeonGenerator::SetupCatacombAtmosphere() {
  UWorld *World = GetWorld();
  if (!World) {
    return;
  }

  // 1. Exponential Height Fog & Volumetric Scattering (God-Rays & Dusky
  // Catacomb Haze)
  AExponentialHeightFog *TargetFog = nullptr;
  for (TActorIterator<AExponentialHeightFog> It(World); It; ++It) {
    TargetFog = *It;
    break;
  }

  if (!TargetFog) {
    FActorSpawnParameters FogSpawnParams;
    FogSpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    TargetFog = World->SpawnActor<AExponentialHeightFog>(
        FVector(0.0f, 0.0f, 200.0f), FRotator::ZeroRotator, FogSpawnParams);
  }

  if (TargetFog && TargetFog->GetComponent()) {
    UExponentialHeightFogComponent *FogComp = TargetFog->GetComponent();
    FogComp->SetFogDensity(FogDensity);
    FogComp->SetFogHeightFalloff(0.0022f);
    FogComp->SetFogInscatteringColor(FLinearColor(0.04f, 0.05f, 0.08f, 1.0f));
    FogComp->SetVolumetricFog(true);
    FogComp->SetVolumetricFogScatteringDistribution(0.20f);
    FogComp->SetVolumetricFogExtinctionScale(1.0f);
    FogComp->SetVolumetricFogDistance(4500.0f);
    FogComp->MarkRenderStateDirty();
  }

  // 2. Post Process Tuning for Dark Fantasy Atmosphere & Torch Bloom
  APostProcessVolume *TargetPPV = nullptr;
  for (TActorIterator<APostProcessVolume> It(World); It; ++It) {
    TargetPPV = *It;
    break;
  }

  if (TargetPPV) {
    TargetPPV->bUnbound = true;
    TargetPPV->Settings.bOverride_BloomIntensity = true;
    TargetPPV->Settings.BloomIntensity = BloomIntensity;
    TargetPPV->Settings.bOverride_BloomThreshold = true;
    TargetPPV->Settings.BloomThreshold = BloomThreshold;
    TargetPPV->Settings.bOverride_VignetteIntensity = true;
    TargetPPV->Settings.VignetteIntensity = 0.30f;

    // Enable natural Eye Adaptation so dark corridors are properly visible and atmospheric
    TargetPPV->Settings.bOverride_AutoExposureMethod = true;
    TargetPPV->Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
    TargetPPV->Settings.bOverride_AutoExposureMinBrightness = true;
    TargetPPV->Settings.AutoExposureMinBrightness = 0.03f;
    TargetPPV->Settings.bOverride_AutoExposureMaxBrightness = true;
    TargetPPV->Settings.AutoExposureMaxBrightness = 2.0f;
  }

  // 3. Underground Catacomb: suppress outdoor sunlight and sky atmosphere bleed
  for (TActorIterator<ADirectionalLight> It(World); It; ++It) {
    if (ADirectionalLight *DirLight = *It) {
      DirLight->SetActorHiddenInGame(true);
      if (UDirectionalLightComponent *LightComp =
              DirLight->GetComponentByClass<UDirectionalLightComponent>()) {
        LightComp->SetIntensity(0.0f);
        LightComp->SetVisibility(false);
        LightComp->bAffectsWorld = false;
        LightComp->SetCastShadows(false);
        LightComp->MarkRenderStateDirty();
      }
    }
  }
  for (TActorIterator<ASkyLight> It(World); It; ++It) {
    if (ASkyLight *Sky = *It) {
      Sky->SetActorHiddenInGame(true);
      if (USkyLightComponent *SkyComp =
              Sky->GetComponentByClass<USkyLightComponent>()) {
        SkyComp->SetIntensity(0.0f);
        SkyComp->SetVisibility(false);
        SkyComp->bAffectsWorld = false;
        SkyComp->MarkRenderStateDirty();
      }
    }
  }
  TArray<AActor *> AllLevelActors;
  UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllLevelActors);
  for (AActor *Act : AllLevelActors) {
    if (Act && Act->GetClass()->GetName().Contains(TEXT("SkyAtmosphere"))) {
      Act->SetActorHiddenInGame(true);
      if (USceneComponent *Root = Act->GetRootComponent()) {
        Root->SetVisibility(false, true);
      }
    }
  }
}

void ADungeonGenerator::SpawnTorchActor(const FTransform &Transform) {
  UWorld *World = GetWorld();
  if (!World || !TorchBlueprintClass)
    return;

  FActorSpawnParameters SpawnParams;
  SpawnParams.Owner = nullptr;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  SpawnParams.OverrideLevel = World->GetCurrentLevel();

  AActor *TorchActor =
      World->SpawnActor<AActor>(TorchBlueprintClass, Transform, SpawnParams);
  if (TorchActor) {
    TorchActor->Tags.AddUnique(TEXT("DungeonTorch"));
    const bool bIsBossRoom = IsInsideBossRoom(Transform.GetLocation());
    if (bIsBossRoom) {
      TorchActor->Tags.AddUnique(TEXT("BossRoomTorch"));
    }
    const float EffectiveIntensity =
        bIsBossRoom ? BossTorchIntensity : TorchIntensity;
    const float EffectiveRadius =
        bIsBossRoom ? BossTorchAttenuationRadius : TorchAttenuationRadius;

    if (USceneComponent *Root = TorchActor->GetRootComponent()) {
      Root->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    }
    TorchActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
#if WITH_EDITOR
    TorchActor->SetFolderPath(FName(TEXT("Dungeon_Torches")));
    TorchActor->SetActorLabel(
        FString::Printf(TEXT("Torch_%d"), SpawnedTorches.Num() + 1));
#endif
    TArray<UPointLightComponent *> PointLights;
    TorchActor->GetComponents<UPointLightComponent>(PointLights);
    for (UPointLightComponent *LightComp : PointLights) {
      if (LightComp) {
        LightComp->bUseInverseSquaredFalloff = true;
        LightComp->SetIntensity(EffectiveIntensity);
        LightComp->SetAttenuationRadius(EffectiveRadius);
        LightComp->SetSourceRadius(bIsBossRoom ? 35.0f : TorchSourceRadius);
        LightComp->SetSoftSourceRadius(
            (bIsBossRoom ? 35.0f : TorchSourceRadius) * 0.75f);
        LightComp->ShadowBias = TorchShadowBias;
        LightComp->ShadowSlopeBias = TorchShadowBias;
        LightComp->SetLightColor(bIsBossRoom
                                     ? FLinearColor(1.0f, 0.72f, 0.38f, 1.0f)
                                     : FLinearColor(1.0f, 0.65f, 0.28f, 1.0f));
        // Boss room perimeter torches act as fill lights; corridor torches use bCorridorTorchesCastShadow
        LightComp->SetCastShadows(bIsBossRoom ? false : bCorridorTorchesCastShadow);
        LightComp->bCastVolumetricShadow = false;
        LightComp->VolumetricScatteringIntensity = TorchVolumetricScattering;

        // Distance Culling: standard torches render visible up to 4500 cm (9 tiles)
        LightComp->MaxDrawDistance = 4500.0f;
        LightComp->MaxDistanceFadeRange = 500.0f;
      }
    }

    TArray<UStaticMeshComponent *> MeshComps;
    TorchActor->GetComponents<UStaticMeshComponent>(MeshComps);
    for (UStaticMeshComponent *SMC : MeshComps) {
      if (SMC) {
        SMC->SetCastShadow(bTorchHolderCastShadow);
        SMC->bCastContactShadow = false;
      }
    }

    TArray<UParticleSystemComponent *> FireParticles;
    TorchActor->GetComponents<UParticleSystemComponent>(FireParticles);
    for (UParticleSystemComponent *PSC : FireParticles) {
      if (PSC) {
        const float EffectiveFlameScale =
            bIsBossRoom ? (TorchFlameScale * 1.25f) : TorchFlameScale;
        PSC->SetRelativeScale3D(FVector(EffectiveFlameScale));
        PSC->SetCullDistance(bIsBossRoom ? 6000.0f : 5000.0f);
      }
    }

    SpawnedTorches.Add(TorchActor);
  }
}

void ADungeonGenerator::SpawnBossArenaLight(const FVector &Location,
                                            float Intensity, float Radius,
                                            FLinearColor Color,
                                            float SourceRadius) {
  UWorld *World = GetWorld();
  if (!World)
    return;

  FActorSpawnParameters SpawnParams;
  SpawnParams.Owner = nullptr;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  SpawnParams.OverrideLevel = World->GetCurrentLevel();

  AActor *LightActor = World->SpawnActor<AActor>(
      AActor::StaticClass(), FTransform(FRotator::ZeroRotator, Location),
      SpawnParams);
  if (LightActor) {
    LightActor->Tags.AddUnique(TEXT("DungeonTorch"));
    LightActor->Tags.AddUnique(TEXT("BossArenaLight"));

    USceneComponent *RootComp =
        NewObject<USceneComponent>(LightActor, TEXT("ArenaLightRoot"));
    LightActor->SetRootComponent(RootComp);
    RootComp->RegisterComponent();
    RootComp->SetWorldLocation(Location);

    UPointLightComponent *LightComp =
        NewObject<UPointLightComponent>(LightActor, TEXT("ArenaPointLight"));
    LightComp->SetupAttachment(RootComp);
    LightComp->bUseInverseSquaredFalloff = true;
    LightComp->SetIntensity(Intensity);
    LightComp->SetAttenuationRadius(Radius);
    LightComp->SetSourceRadius(SourceRadius);
    LightComp->SetSoftSourceRadius(SourceRadius * 0.8f);
    LightComp->ShadowBias = 0.05f;
    LightComp->ShadowSlopeBias = 0.05f;
    LightComp->SetLightColor(Color);
    // Only the grand central vault light (Intensity >= 10000.0f) casts dynamic shadows; quadrant lights act as bright fill
    const bool bCastShadow = (Intensity >= 10000.0f);
    LightComp->SetCastShadows(bCastShadow);
    LightComp->bCastVolumetricShadow = bCastShadow;
    LightComp->VolumetricScatteringIntensity = bCastShadow ? 0.03f : 0.0f;
    LightComp->MaxDrawDistance = 5500.0f;
    LightComp->MaxDistanceFadeRange = 500.0f;
    LightComp->RegisterComponent();

    if (USceneComponent *Root = LightActor->GetRootComponent()) {
      Root->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    }
    LightActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

#if WITH_EDITOR
    LightActor->SetFolderPath(FName(TEXT("Dungeon_Torches")));
    LightActor->SetActorLabel(
        FString::Printf(TEXT("BossArenaLight_%d"), SpawnedTorches.Num() + 1));
#endif

    SpawnedTorches.Add(LightActor);
  }
}
bool ADungeonGenerator::GenerateDungeonLayout() {
  GridSize = 31;
  Grid.Init(EDungeonCellType::Solid, GridSize * GridSize);
  Rooms.Empty();

  // 1. Build the 3 Boss Arenas (Unskippable Chokepoints) with procedural positioning
  BuildBossArenas();

  const FDungeonRoom &R1 = Rooms[Boss1RoomIndex];
  const int32 R1_CenterX = R1.X + 3;
  const int32 R1_CenterY = R1.Y + 3;

  const FDungeonRoom &R2 = Rooms[Boss2RoomIndex];
  const int32 R2_CenterX = R2.X + 3;
  const int32 R2_CenterY = R2.Y + 3;

  const FDungeonRoom &R3 = Rooms[Boss3RoomIndex];
  const int32 R3_CenterX = R3.X + 3;
  const int32 R3_CenterY = R3.Y + 3;

  // 2. Carve Sector 1 Maze (West Column: X in [1..9], Y in [1 .. R1.Y - 2])
  FIntPoint S1Start(3, 3);
  FIntPoint S1End(R1_CenterX, R1.Y - 2);
  CarveSectorMaze(1, 9, 1, R1.Y - 2, S1Start, S1End);
  // Dedicated 1-tile entrance approach to Boss 1 (no wide horizontal highway)
  SetCell(R1_CenterX, R1.Y - 2, EDungeonCellType::Corridor);
  SetCell(R1_CenterX, R1.Y - 1, EDungeonCellType::BossDoor_1_Enter);

  // 3. Connect Boss 1 Exit across column 9 and 10 to Sector 2
  SetCell(R1.X + 7, R1_CenterY, EDungeonCellType::BossDoor_1_Exit);
  SetCell(9, R1_CenterY, EDungeonCellType::Corridor);
  SetCell(10, R1_CenterY, EDungeonCellType::Corridor);
  SetCell(11, R1_CenterY, EDungeonCellType::Corridor);

  // 4. Carve Sector 2 Maze (Center Column: X in [11..19], Y in [R2.Y + 8 .. 28])
  FIntPoint S2Start(11, R1_CenterY);
  FIntPoint S2End(R2_CenterX, R2.Y + 8);
  CarveSectorMaze(11, 19, R2.Y + 8, 28, S2Start, S2End);
  // Dedicated 1-tile entrance approach to Boss 2
  SetCell(R2_CenterX, R2.Y + 8, EDungeonCellType::Corridor);
  SetCell(R2_CenterX, R2.Y + 7, EDungeonCellType::BossDoor_2_Enter);

  // 5. Connect Boss 2 Exit across column 19 and 20 to Sector 3
  SetCell(R2.X + 7, R2_CenterY, EDungeonCellType::BossDoor_2_Exit);
  SetCell(19, R2_CenterY, EDungeonCellType::Corridor);
  SetCell(20, R2_CenterY, EDungeonCellType::Corridor);
  SetCell(21, R2_CenterY, EDungeonCellType::Corridor);

  // 6. Carve Sector 3 Maze (East Column: X in [21..29], Y in [1 .. R3.Y - 2])
  FIntPoint S3Start(21, R2_CenterY);
  FIntPoint S3End(R3_CenterX, R3.Y - 2);
  CarveSectorMaze(21, 29, 1, R3.Y - 2, S3Start, S3End);
  // Dedicated 1-tile entrance approach to Boss 3
  SetCell(R3_CenterX, R3.Y - 2, EDungeonCellType::Corridor);
  SetCell(R3_CenterX, R3.Y - 1, EDungeonCellType::BossDoor_3_Enter);

  // 7. North Sanctuary corridor behind Boss Room 3 exit door
  SetCell(R3_CenterX, R3.Y + 7, EDungeonCellType::BossDoor_3_Exit);
  for (int32 Y = R3.Y + 8; Y < GridSize - 1; ++Y) {
    SetCell(R3_CenterX, Y, EDungeonCellType::Corridor);
  }

  // 8. Add controlled dead-end loops
  AddSectorLoops(1, 9, 1, R1.Y - 2, 0.20f);
  AddSectorLoops(11, 19, R2.Y + 8, 28, 0.20f);
  AddSectorLoops(21, 29, 1, R3.Y - 2, 0.20f);

  // Setup player spawn
  PlayerSpawnLocation = GridToWorldCenter(3, 3, 130.0f);

  // Compatibility aliases
  JewelRoomLocation = Boss1RoomLocation;
  JewelLocation = Boss1SpawnLocation;
  BeastRoomLocation = Boss2RoomLocation;
  BeastLocation = Boss2SpawnLocation;
  ExitRoomLocation = Boss3RoomLocation;
  ExitDoorLocation = Boss3ExitDoorLocation;

  return true;
}
void ADungeonGenerator::BuildBossArenas() {
  const float FTile = (float)TileSize;

  // Arena Dimensions: 7x7 tiles = 3500x3500 cm

  // --- BOSS ROOM 1 (Stage 1 Finale, West Column) ---
  FDungeonRoom R1;
  R1.X = 1;
  // Procedural placement in Sector 1 (randomly Y in {17, 19, 21})
  R1.Y = 17 + RandomStream.RandRange(0, 2) * 2;
  R1.Width = 7;
  R1.Height = 7;
  R1.Type = EDungeonCellType::BossRoom_1;
  for (int32 X = R1.X; X < R1.X + R1.Width; ++X) {
    for (int32 Y = R1.Y; Y < R1.Y + R1.Height; ++Y) {
      SetCell(X, Y, EDungeonCellType::BossRoom_1);
    }
  }
  const int32 R1_CenterX = R1.X + 3;
  const int32 R1_CenterY = R1.Y + 3;
  // Entrance (South wall) & Exit (East wall)
  SetCell(R1_CenterX, R1.Y - 1, EDungeonCellType::BossDoor_1_Enter);
  SetCell(R1.X + 7, R1_CenterY, EDungeonCellType::BossDoor_1_Exit);

  Boss1RoomIndex = Rooms.Add(R1);
  Boss1RoomLocation = GridToWorldCenter(R1_CenterX, R1_CenterY, 0.0f);
  Boss1SpawnLocation = Boss1RoomLocation + FVector(0.0f, 0.0f, 110.0f);
  Boss1EntranceDoorLocation = FVector((R1_CenterX + 0.5f) * FTile, R1.Y * FTile, 0.0f);
  Boss1ExitDoorLocation = FVector((R1.X + 7) * FTile, (R1_CenterY + 0.5f) * FTile, 0.0f);

  // --- BOSS ROOM 2 (Stage 2 Finale, Center Column) ---
  FDungeonRoom R2;
  R2.X = 11;
  // Procedural placement in Sector 2 (randomly Y in {3, 5, 7})
  R2.Y = 3 + RandomStream.RandRange(0, 2) * 2;
  R2.Width = 7;
  R2.Height = 7;
  R2.Type = EDungeonCellType::BossRoom_2;
  for (int32 X = R2.X; X < R2.X + R2.Width; ++X) {
    for (int32 Y = R2.Y; Y < R2.Y + R2.Height; ++Y) {
      SetCell(X, Y, EDungeonCellType::BossRoom_2);
    }
  }
  const int32 R2_CenterX = R2.X + 3;
  const int32 R2_CenterY = R2.Y + 3;
  // Entrance (North wall) & Exit (East wall)
  SetCell(R2_CenterX, R2.Y + 7, EDungeonCellType::BossDoor_2_Enter);
  SetCell(R2.X + 7, R2_CenterY, EDungeonCellType::BossDoor_2_Exit);

  Boss2RoomIndex = Rooms.Add(R2);
  Boss2RoomLocation = GridToWorldCenter(R2_CenterX, R2_CenterY, 0.0f);
  Boss2SpawnLocation = Boss2RoomLocation + FVector(0.0f, 0.0f, 120.0f);
  Boss2EntranceDoorLocation = FVector((R2_CenterX + 0.5f) * FTile, (R2.Y + 7) * FTile, 0.0f);
  Boss2ExitDoorLocation = FVector((R2.X + 7) * FTile, (R2_CenterY + 0.5f) * FTile, 0.0f);

  // --- BOSS ROOM 3 (Stage 3 Grand Climax, East Column) ---
  FDungeonRoom R3;
  R3.X = 21;
  // Procedural placement in Sector 3 (randomly Y in {17, 19, 21})
  R3.Y = 17 + RandomStream.RandRange(0, 2) * 2;
  R3.Width = 7;
  R3.Height = 7;
  R3.Type = EDungeonCellType::BossRoom_3;
  for (int32 X = R3.X; X < R3.X + R3.Width; ++X) {
    for (int32 Y = R3.Y; Y < R3.Y + R3.Height; ++Y) {
      SetCell(X, Y, EDungeonCellType::BossRoom_3);
    }
  }
  const int32 R3_CenterX = R3.X + 3;
  const int32 R3_CenterY = R3.Y + 3;
  // Entrance (South wall) & Final Exit Door (North wall, 3 boss relics required)
  SetCell(R3_CenterX, R3.Y - 1, EDungeonCellType::BossDoor_3_Enter);
  SetCell(R3_CenterX, R3.Y + 7, EDungeonCellType::BossDoor_3_Exit);

  Boss3RoomIndex = Rooms.Add(R3);
  Boss3RoomLocation = GridToWorldCenter(R3_CenterX, R3_CenterY, 0.0f);
  Boss3SpawnLocation = Boss3RoomLocation + FVector(0.0f, 0.0f, 115.0f);
  Boss3EntranceDoorLocation = FVector((R3_CenterX + 0.5f) * FTile, R3.Y * FTile, 0.0f);
  Boss3ExitDoorLocation = FVector((R3_CenterX + 0.5f) * FTile, (R3.Y + 7) * FTile, 0.0f);
}

void ADungeonGenerator::CarveCryptChamber(int32 OriginX, int32 OriginY, int32 W,
                                          int32 H) {
  for (int32 X = OriginX; X < OriginX + W; ++X) {
    for (int32 Y = OriginY; Y < OriginY + H; ++Y) {
      if (X >= 0 && X < GridSize && Y >= 0 && Y < GridSize) {
        SetCell(X, Y, EDungeonCellType::Corridor);
      }
    }
  }
}

void ADungeonGenerator::CarveSectorMaze(int32 MinX, int32 MaxX, int32 MinY,
                                        int32 MaxY, FIntPoint StartCell,
                                        FIntPoint EndCell) {
  int32 SX = StartCell.X;
  int32 SY = StartCell.Y;
  if (SX % 2 == 0)
    SX = FMath::Min(MaxX - 1, SX + 1);
  if (SY % 2 == 0)
    SY = FMath::Min(MaxY - 1, SY + 1);

  TArray<FIntPoint> Stack;
  if (GetCell(SX, SY) == EDungeonCellType::Solid) {
    SetCell(SX, SY, EDungeonCellType::Corridor);
  }
  Stack.Push(FIntPoint(SX, SY));

  const FIntPoint Dirs[4] = {FIntPoint(0, 2), FIntPoint(0, -2), FIntPoint(2, 0),
                             FIntPoint(-2, 0)};
  int32 LastDir = -1;
  int32 StraightSteps = 0;

  while (Stack.Num() > 0) {
    FIntPoint Current = Stack.Top();
    TArray<int32> ValidDirs;

    for (int32 d = 0; d < 4; ++d) {
      int32 NX = Current.X + Dirs[d].X;
      int32 NY = Current.Y + Dirs[d].Y;

      if (NX >= MinX && NX <= MaxX && NY >= MinY && NY <= MaxY) {
        if (GetCell(NX, NY) == EDungeonCellType::Solid) {
          // Avoid running straight for more than 1 step
          if (d == LastDir && StraightSteps >= 1) {
            continue;
          }
          ValidDirs.Add(d);
        }
      }
    }

    // Fallback if no direction passed the straight limiter
    if (ValidDirs.Num() == 0) {
      for (int32 d = 0; d < 4; ++d) {
        int32 NX = Current.X + Dirs[d].X;
        int32 NY = Current.Y + Dirs[d].Y;
        if (NX >= MinX && NX <= MaxX && NY >= MinY && NY <= MaxY &&
            GetCell(NX, NY) == EDungeonCellType::Solid) {
          ValidDirs.Add(d);
        }
      }
    }

    if (ValidDirs.Num() > 0) {
      int32 Chosen = -1;
      TArray<int32> TurnDirs;
      for (int32 d : ValidDirs) {
        if (d != LastDir)
          TurnDirs.Add(d);
      }

      // 85% chance to TURN, forcing winding S-bends, zigzags, and corners
      if (TurnDirs.Num() > 0 &&
          (LastDir == -1 || RandomStream.FRand() < 0.85f)) {
        Chosen = TurnDirs[RandomStream.RandRange(0, TurnDirs.Num() - 1)];
      } else {
        Chosen = ValidDirs[RandomStream.RandRange(0, ValidDirs.Num() - 1)];
      }

      if (Chosen == LastDir) {
        StraightSteps++;
      } else {
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
    } else {
      Stack.Pop();
      LastDir = -1;
      StraightSteps = 0;
    }
  }

  ConnectPointToNearestCorridor(StartCell, MinX, MaxX, MinY, MaxY);
  ConnectPointToNearestCorridor(EndCell, MinX, MaxX, MinY, MaxY);
}

void ADungeonGenerator::ConnectPointToNearestCorridor(FIntPoint Pt, int32 MinX,
                                                      int32 MaxX, int32 MinY,
                                                      int32 MaxY) {
  if (GetCell(Pt.X, Pt.Y) == EDungeonCellType::Corridor) {
    return;
  }

  // BFS from Pt to find the nearest Corridor within [MinX..MaxX, MinY..MaxY]
  TQueue<FIntPoint> Queue;
  TMap<FIntPoint, FIntPoint> ParentMap;
  TSet<FIntPoint> Visited;

  Queue.Enqueue(Pt);
  Visited.Add(Pt);

  const FIntPoint Dirs[4] = {FIntPoint(0, 1), FIntPoint(0, -1), FIntPoint(1, 0),
                             FIntPoint(-1, 0)};
  TOptional<FIntPoint> Target;

  while (!Queue.IsEmpty()) {
    FIntPoint Curr;
    Queue.Dequeue(Curr);

    if (GetCell(Curr.X, Curr.Y) == EDungeonCellType::Corridor && Curr != Pt) {
      Target = Curr;
      break;
    }

    for (int32 d = 0; d < 4; ++d) {
      FIntPoint Next = Curr + Dirs[d];
      if (Next.X >= MinX && Next.X <= MaxX && Next.Y >= MinY &&
          Next.Y <= MaxY && !Visited.Contains(Next)) {
        Visited.Add(Next);
        ParentMap.Add(Next, Curr);
        Queue.Enqueue(Next);
      }
    }
  }

  if (Target.IsSet()) {
    FIntPoint Curr = Target.GetValue();
    while (ParentMap.Contains(Curr)) {
      if (GetCell(Curr.X, Curr.Y) == EDungeonCellType::Solid) {
        SetCell(Curr.X, Curr.Y, EDungeonCellType::Corridor);
      }
      Curr = ParentMap[Curr];
    }
    if (GetCell(Pt.X, Pt.Y) == EDungeonCellType::Solid) {
      SetCell(Pt.X, Pt.Y, EDungeonCellType::Corridor);
    }
  }
}

void ADungeonGenerator::AddSectorLoops(int32 MinX, int32 MaxX, int32 MinY,
                                       int32 MaxY, float LoopProb) {
  const FIntPoint Dirs[4] = {FIntPoint(0, 1), FIntPoint(0, -1), FIntPoint(1, 0),
                             FIntPoint(-1, 0)};

  for (int32 X = MinX + 1; X < MaxX; ++X) {
    for (int32 Y = MinY + 1; Y < MaxY; ++Y) {
      if (GetCell(X, Y) == EDungeonCellType::Solid) {
        int32 CorrNeighbors = 0;
        for (int32 d = 0; d < 4; ++d) {
          int32 NX = X + Dirs[d].X;
          int32 NY = Y + Dirs[d].Y;
          if (NX >= MinX && NX <= MaxX && NY >= MinY && NY <= MaxY &&
              GetCell(NX, NY) == EDungeonCellType::Corridor) {
            CorrNeighbors++;
          }
        }

        if (CorrNeighbors == 2 && RandomStream.FRand() < LoopProb) {
          // Ensure we don't merge walls into a 2x2 open room
          bool bCreates2x2 = false;
          for (int32 ox = -1; ox <= 0; ++ox) {
            for (int32 oy = -1; oy <= 0; ++oy) {
              int32 OpenCount = 0;
              for (int32 cx = 0; cx <= 1; ++cx) {
                for (int32 cy = 0; cy <= 1; ++cy) {
                  if (GetCell(X + ox + cx, Y + oy + cy) !=
                          EDungeonCellType::Solid ||
                      (ox + cx == 0 && oy + cy == 0)) {
                    OpenCount++;
                  }
                }
              }
              if (OpenCount == 4) {
                bCreates2x2 = true;
                break;
              }
            }
            if (bCreates2x2)
              break;
          }

          if (!bCreates2x2) {
            SetCell(X, Y, EDungeonCellType::Corridor);
          }
        }
      }
    }
  }
}
void ADungeonGenerator::BuildGeometryInstances() {
  const float FTile = (float)TileSize;

  // 1. Continuous Stone Floor (-1..GridSize)
  if (FloorInstances) {
    for (int32 X = -1; X <= GridSize; ++X) {
      for (int32 Y = -1; Y <= GridSize; ++Y) {
        FVector FloorPos((X + 1) * FTile, (Y + 1) * FTile, 0.0f);
        FloorInstances->AddInstance(FTransform(FRotator::ZeroRotator, FloorPos,
                                               FVector(1.02f, 1.02f, 1.0f)));
      }
    }
  }

  // 2. Continuous Stone Ceiling & Roof (-1..GridSize)
  for (int32 X = -1; X <= GridSize; ++X) {
    for (int32 Y = -1; Y <= GridSize; ++Y) {
      FVector TilePos((X + 1) * FTile, (Y + 1) * FTile, WallHeight);
      if (CeilingInstances) {
        CeilingInstances->AddInstance(FTransform(FRotator::ZeroRotator, TilePos,
                                                 FVector(1.02f, 1.02f, 1.0f)));
      }
      if (RoofInstances) {
        RoofInstances->AddInstance(FTransform(
            FRotator::ZeroRotator, TilePos + FVector(0.0f, 0.0f, 5.0f),
            FVector(1.02f, 1.02f, 1.0f)));
      }
    }
  }

  // 3. Spawn Walls
  for (int32 X = 0; X < GridSize; ++X) {
    for (int32 Y = 0; Y < GridSize; ++Y) {
      if (IsWalkable(X, Y)) {
        auto PlaceWall = [this, X, Y](FVector Loc, FRotator Rot, FVector DecLoc,
                                      FRotator DecRot, bool bCanBeDecorative) {
          // Never make a wall decorative (cupboard) if near doorways or altars
          if (IsNearDoorway(Loc, 400.0f) || IsNearPropOrAltar(Loc, 400.0f)) {
            bCanBeDecorative = false;
          }

          // Corridors keep clean plain stone walls
          if (GetCell(X, Y) == EDungeonCellType::Corridor) {
            bCanBeDecorative = false;
          }

          // Wall intervals in boss rooms where torches mount must be plain flat
          // stone
          for (const FDungeonRoom &Room : Rooms) {
            if (X >= Room.X && X < Room.X + Room.Width && Y >= Room.Y &&
                Y < Room.Y + Room.Height) {
              if ((X - Room.X) % 2 == 1 || (Y - Room.Y) % 2 == 1) {
                bCanBeDecorative = false;
              }
              break;
            }
          }

          bool bDecorative = bCanBeDecorative &&
                             (RandomStream.FRand() < DecorativeWallRatio) &&
                             DecorativeWallInstances;
          if (bDecorative) {
            FTransform DecTransform(DecRot, DecLoc + FVector(0.0f, 0.0f, -4.0f), FVector(1.01f, 1.0f, 1.02f));
            DecorativeWallInstances->AddInstance(DecTransform);
          } else if (WallInstances) {
            FTransform WallTransform(Rot, Loc + FVector(0.0f, 0.0f, -4.0f), FVector(1.01f, 1.0f, 1.02f));
            WallInstances->AddInstance(WallTransform);
          }

          if (WallTrimInstances) {
            FTransform TrimTransform(Rot, Loc, FVector(1.01f, 1.0f, 1.0f));
            WallTrimInstances->AddInstance(TrimTransform);
          }
        };

        // South Edge
        if (!IsWalkable(X, Y - 1)) {
          bool bStraight =
              (!IsWalkable(X - 1, Y - 1) && IsWalkable(X - 1, Y)) &&
              (!IsWalkable(X + 1, Y - 1) && IsWalkable(X + 1, Y));
          PlaceWall(FVector(X * FTile, Y * FTile, 0.0f),
                    FRotator(0.0f, 180.0f, 0.0f),
                    FVector((X + 1) * FTile, Y * FTile, 0.0f),
                    FRotator(0.0f, 0.0f, 0.0f), bStraight);
        }

        // North Edge
        if (!IsWalkable(X, Y + 1)) {
          bool bStraight =
              (!IsWalkable(X - 1, Y + 1) && IsWalkable(X - 1, Y)) &&
              (!IsWalkable(X + 1, Y + 1) && IsWalkable(X + 1, Y));
          PlaceWall(FVector((X + 1) * FTile, (Y + 1) * FTile, 0.0f),
                    FRotator(0.0f, 0.0f, 0.0f),
                    FVector(X * FTile, (Y + 1) * FTile, 0.0f),
                    FRotator(0.0f, 180.0f, 0.0f), bStraight);
        }

        // West Edge
        if (!IsWalkable(X - 1, Y)) {
          bool bStraight =
              (!IsWalkable(X - 1, Y - 1) && IsWalkable(X, Y - 1)) &&
              (!IsWalkable(X - 1, Y + 1) && IsWalkable(X, Y + 1));
          PlaceWall(FVector(X * FTile, (Y + 1) * FTile, 0.0f),
                    FRotator(0.0f, 90.0f, 0.0f),
                    FVector(X * FTile, Y * FTile, 0.0f),
                    FRotator(0.0f, -90.0f, 0.0f), bStraight);
        }

        // East Edge
        if (!IsWalkable(X + 1, Y)) {
          bool bStraight =
              (!IsWalkable(X + 1, Y - 1) && IsWalkable(X, Y - 1)) &&
              (!IsWalkable(X + 1, Y + 1) && IsWalkable(X, Y + 1));
          PlaceWall(FVector((X + 1) * FTile, Y * FTile, 0.0f),
                    FRotator(0.0f, -90.0f, 0.0f),
                    FVector((X + 1) * FTile, (Y + 1) * FTile, 0.0f),
                    FRotator(0.0f, 90.0f, 0.0f), bStraight);
        }
      }
    }
  }

  PlaceWallPillars();
}

void ADungeonGenerator::PlaceWallPillars() {
  const float FTile = (float)TileSize;
  if (!PillarInstances)
    return;

  // SM_Crypt_Pillar native mesh height is 330.84cm. WallHeight is 450cm.
  // Scale Z so the pillar extends all the way from Z=0 to the roof/ceiling at
  // Z=WallHeight + 2cm.
  const float PillarScaleZ =
      (WallHeight + 5.0f) / 330.84f; // 1.3753f (flush with ceiling)

  for (int32 X = 0; X <= GridSize; ++X) {
    for (int32 Y = 0; Y <= GridSize; ++Y) {
      bool bTL = IsWalkable(X - 1, Y);
      bool bTR = IsWalkable(X, Y);
      bool bBL = IsWalkable(X - 1, Y - 1);
      bool bBR = IsWalkable(X, Y - 1);

      int32 WalkableCornerCount =
          (bTL ? 1 : 0) + (bTR ? 1 : 0) + (bBL ? 1 : 0) + (bBR ? 1 : 0);
      if (WalkableCornerCount >= 1 && WalkableCornerCount <= 3) {
        FVector PillarPos(X * FTile, Y * FTile, 0.0f);
        PillarInstances->AddInstance(
            FTransform(FRotator::ZeroRotator, PillarPos,
                       FVector(1.0f, 1.0f, PillarScaleZ)));
      }
    }
  }
}

void ADungeonGenerator::PlaceEnvironmentalProps() {
  const float FTile = (float)TileSize;

  // 1. Fully illuminate all 3 Boss Rooms on ALL 4 SIDES with torches
  for (const FDungeonRoom &Room : Rooms) {
    PlaceBossRoomTorches(Room);
  }

  // 2. Fully illuminate the maze corridors with wall-mounted torches
  PlaceMazeCorridorTorches();

  // 2. Doorway frames and double doors are spawned dynamically by
  // ABossArenaGate in SpawnBossEncounters()!

  // 3. Boss Room 3 Final Altar / Relic Pedestals (for the 3 collected boss
  // items)
  if (AltarInstances) {
    FVector Altar1Pos = Boss3RoomLocation + FVector(-300.0f, 800.0f, 0.0f);
    FVector Altar2Pos = Boss3RoomLocation + FVector(0.0f, 800.0f, 0.0f);
    FVector Altar3Pos = Boss3RoomLocation + FVector(300.0f, 800.0f, 0.0f);

    AltarInstances->AddInstance(FTransform(FRotator::ZeroRotator, Altar1Pos,
                                           FVector(0.8f, 0.8f, 0.8f)));
    AltarInstances->AddInstance(FTransform(FRotator::ZeroRotator, Altar2Pos,
                                           FVector(1.0f, 1.0f, 1.0f)));
    AltarInstances->AddInstance(FTransform(FRotator::ZeroRotator, Altar3Pos,
                                           FVector(0.8f, 0.8f, 0.8f)));
  }
}

void ADungeonGenerator::PlaceBossRoomTorches(const FDungeonRoom &Room) {
  const float FTile = (float)TileSize;

  // 1. South Wall (facing North into room, Yaw = 0.0f) - Every wall tile
  for (int32 X = Room.X + 1; X < Room.X + Room.Width - 1; ++X) {
    if (GetCell(X, Room.Y - 1) != EDungeonCellType::Solid)
      continue;
    FVector Pos((X + 0.5f) * FTile, Room.Y * FTile + 6.5f, 220.0f);
    if (IsNearDoorway(Pos, 450.0f) || IsNearPropOrAltar(Pos, 350.0f))
      continue;
    SpawnTorchActor(
        FTransform(FRotator(0.0f, 90.0f, 0.0f), Pos, FVector::OneVector));
  }

  // 2. North Wall (facing South into room, Yaw = -90.0f) - Every wall tile
  for (int32 X = Room.X + 1; X < Room.X + Room.Width - 1; ++X) {
    if (GetCell(X, Room.Y + Room.Height) != EDungeonCellType::Solid)
      continue;
    FVector Pos((X + 0.5f) * FTile, (Room.Y + Room.Height) * FTile - 6.5f,
                220.0f);
    if (IsNearDoorway(Pos, 450.0f) || IsNearPropOrAltar(Pos, 350.0f))
      continue;
    SpawnTorchActor(
        FTransform(FRotator(0.0f, -90.0f, 0.0f), Pos, FVector::OneVector));
  }

  // 3. West Wall (facing East into room, Yaw = 0.0f) - Every wall tile
  for (int32 Y = Room.Y + 1; Y < Room.Y + Room.Height - 1; ++Y) {
    if (GetCell(Room.X - 1, Y) != EDungeonCellType::Solid)
      continue;
    FVector Pos(Room.X * FTile + 6.5f, (Y + 0.5f) * FTile, 220.0f);
    if (IsNearDoorway(Pos, 450.0f) || IsNearPropOrAltar(Pos, 350.0f))
      continue;
    SpawnTorchActor(
        FTransform(FRotator(0.0f, 0.0f, 0.0f), Pos, FVector::OneVector));
  }

  // 4. East Wall (facing West into room, Yaw = 180.0f) - Every wall tile
  for (int32 Y = Room.Y + 1; Y < Room.Y + Room.Height - 1; ++Y) {
    if (GetCell(Room.X + Room.Width, Y) != EDungeonCellType::Solid)
      continue;
    FVector Pos((Room.X + Room.Width) * FTile - 6.5f, (Y + 0.5f) * FTile,
                220.0f);
    if (IsNearDoorway(Pos, 450.0f) || IsNearPropOrAltar(Pos, 350.0f))
      continue;
    SpawnTorchActor(
        FTransform(FRotator(0.0f, 180.0f, 0.0f), Pos, FVector::OneVector));
  }

  // 5. Boss Room Central & Quadrant Arena Illumination (Fully lights the entire arena combat area)
  FVector RoomCenter =
      GridToWorldCenter(Room.X + Room.Width / 2, Room.Y + Room.Height / 2, 0.0f);

  // Grand Center Vault Illuminator (Height 380 cm)
  SpawnBossArenaLight(FVector(RoomCenter.X, RoomCenter.Y, 380.0f),
                      BossArenaLightIntensity, BossArenaLightRadius,
                      FLinearColor(1.0f, 0.85f, 0.58f, 1.0f), 100.0f);

  // Four Quadrant Arena Lights (Offset +-850 cm, Height 280 cm)
  const float QOffset = 850.0f;
  SpawnBossArenaLight(
      FVector(RoomCenter.X - QOffset, RoomCenter.Y - QOffset, 280.0f),
      BossArenaLightIntensity * 0.45f, BossArenaLightRadius * 0.75f,
      FLinearColor(1.0f, 0.75f, 0.42f, 1.0f), 60.0f);
  SpawnBossArenaLight(
      FVector(RoomCenter.X + QOffset, RoomCenter.Y - QOffset, 280.0f),
      BossArenaLightIntensity * 0.45f, BossArenaLightRadius * 0.75f,
      FLinearColor(1.0f, 0.75f, 0.42f, 1.0f), 60.0f);
  SpawnBossArenaLight(
      FVector(RoomCenter.X - QOffset, RoomCenter.Y + QOffset, 280.0f),
      BossArenaLightIntensity * 0.45f, BossArenaLightRadius * 0.75f,
      FLinearColor(1.0f, 0.75f, 0.42f, 1.0f), 60.0f);
  SpawnBossArenaLight(
      FVector(RoomCenter.X + QOffset, RoomCenter.Y + QOffset, 280.0f),
      BossArenaLightIntensity * 0.45f, BossArenaLightRadius * 0.75f,
      FLinearColor(1.0f, 0.75f, 0.42f, 1.0f), 60.0f);
}

void ADungeonGenerator::PlaceMazeCorridorTorches() {
  const float FTile = (float)TileSize;
  const float MinSpacingSq =
      FMath::Square(380.0f); // Prevents tight corner overlap in same cell while allowing alternating every other wall across cells

  TArray<FVector> PlacedTorchLocations;
  for (const TWeakObjectPtr<AActor> &Ptr : SpawnedTorches) {
    if (AActor *Act = Ptr.Get()) {
      if (!IsInsideBossRoom(Act->GetActorLocation())) {
        PlacedTorchLocations.Add(Act->GetActorLocation());
      }
    }
  }

  auto TryPlaceTorch = [this, &PlacedTorchLocations,
                        MinSpacingSq](FVector Pos, FRotator Rot) -> bool {
    if (IsNearDoorway(Pos, 400.0f) || IsNearPropOrAltar(Pos, 350.0f)) {
      return false;
    }
    if (IsInsideBossRoom(Pos)) {
      return false;
    }
    for (const FVector &Existing : PlacedTorchLocations) {
      if (FVector::DistSquared2D(Pos, Existing) < MinSpacingSq) {
        return false;
      }
    }
    SpawnTorchActor(FTransform(Rot, Pos, FVector::OneVector));
    PlacedTorchLocations.Add(Pos);
    return true;
  };

  // Pass 1: Alternating torches every other wall segment along corridors
  for (int32 Y = 1; Y < GridSize - 1; ++Y) {
    for (int32 X = 1; X < GridSize - 1; ++X) {
      if (GetCell(X, Y) != EDungeonCellType::Corridor ||
          !IsInsidePlayableMaze(X, Y)) {
        continue;
      }

      // 1. South Wall (facing North into corridor, Yaw = 90.0f): alternating (even X)
      if (GetCell(X, Y - 1) == EDungeonCellType::Solid && (X % 2 == 0)) {
        FVector Pos((X + 0.5f) * FTile, Y * FTile + 6.5f, 220.0f);
        TryPlaceTorch(Pos, FRotator(0.0f, 90.0f, 0.0f));
      }

      // 2. North Wall (facing South into corridor, Yaw = -90.0f): alternating (odd X)
      if (GetCell(X, Y + 1) == EDungeonCellType::Solid && (X % 2 == 1)) {
        FVector Pos((X + 0.5f) * FTile, (Y + 1) * FTile - 6.5f, 220.0f);
        TryPlaceTorch(Pos, FRotator(0.0f, -90.0f, 0.0f));
      }

      // 3. West Wall (facing East into corridor, Yaw = 0.0f): alternating (even Y)
      if (GetCell(X - 1, Y) == EDungeonCellType::Solid && (Y % 2 == 0)) {
        FVector Pos(X * FTile + 6.5f, (Y + 0.5f) * FTile, 220.0f);
        TryPlaceTorch(Pos, FRotator(0.0f, 0.0f, 0.0f));
      }

      // 4. East Wall (facing West into corridor, Yaw = 180.0f): alternating (odd Y)
      if (GetCell(X + 1, Y) == EDungeonCellType::Solid && (Y % 2 == 1)) {
        FVector Pos((X + 1) * FTile - 6.5f, (Y + 0.5f) * FTile, 220.0f);
        TryPlaceTorch(Pos, FRotator(0.0f, 180.0f, 0.0f));
      }
    }
  }

  // Pass 2: Dark Zone Elimination - ensure every playable corridor tile has warm torch light within 900 cm
  for (int32 Y = 1; Y < GridSize - 1; ++Y) {
    for (int32 X = 1; X < GridSize - 1; ++X) {
      if (GetCell(X, Y) != EDungeonCellType::Corridor ||
          !IsInsidePlayableMaze(X, Y)) {
        continue;
      }
      FVector CellCenter = GridToWorldCenter(X, Y, 220.0f);
      bool bHasTorchNearby = false;
      for (const FVector &Existing : PlacedTorchLocations) {
        if (FVector::DistSquared2D(CellCenter, Existing) < FMath::Square(900.0f)) {
          bHasTorchNearby = true;
          break;
        }
      }
      if (!bHasTorchNearby) {
        if (GetCell(X, Y - 1) == EDungeonCellType::Solid) {
          TryPlaceTorch(FVector((X + 0.5f) * FTile, Y * FTile + 6.5f, 220.0f), FRotator(0.0f, 90.0f, 0.0f));
        } else if (GetCell(X, Y + 1) == EDungeonCellType::Solid) {
          TryPlaceTorch(FVector((X + 0.5f) * FTile, (Y + 1) * FTile - 6.5f, 220.0f), FRotator(0.0f, -90.0f, 0.0f));
        } else if (GetCell(X - 1, Y) == EDungeonCellType::Solid) {
          TryPlaceTorch(FVector(X * FTile + 6.5f, (Y + 0.5f) * FTile, 220.0f), FRotator(0.0f, 0.0f, 0.0f));
        } else if (GetCell(X + 1, Y) == EDungeonCellType::Solid) {
          TryPlaceTorch(FVector((X + 1) * FTile - 6.5f, (Y + 0.5f) * FTile, 220.0f), FRotator(0.0f, 180.0f, 0.0f));
        }
      }
    }
  }
}

bool ADungeonGenerator::SelectAndSetPlayerSpawn() {
  // Sector 1 always begins at (3, 3)
  if (IsValidCell(3, 3) && GetCell(3, 3) == EDungeonCellType::Corridor) {
    PlayerSpawnLocation = GridToWorldCenter(3, 3, 130.0f);
  } else {
    // Fallback: locate the first valid walkable corridor cell in Sector 1
    for (int32 Y = 1; Y < GridSize - 1; ++Y) {
      for (int32 X = 1; X < 9; ++X) {
        if (GetCell(X, Y) == EDungeonCellType::Corridor) {
          PlayerSpawnLocation = GridToWorldCenter(X, Y, 130.0f);
          break;
        }
      }
      if (!PlayerSpawnLocation.IsZero())
        break;
    }
  }

  if (UWorld *World = GetWorld()) {
    if (AActor *PlayerStartActor =
            UGameplayStatics::GetActorOfClass(World, APlayerStart::StaticClass())) {
      if (USceneComponent *Root = PlayerStartActor->GetRootComponent()) {
        Root->SetMobility(EComponentMobility::Movable);
      }
      PlayerStartActor->SetActorLocation(PlayerSpawnLocation);
      PlayerStartActor->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
    }
  }
  return true;
}

bool ADungeonGenerator::IsNearDoorway(const FVector &Location,
                                      float Threshold) const {
  const float ThreshSq = Threshold * Threshold;
  if (FVector::DistSquared2D(Location, Boss1EntranceDoorLocation) < ThreshSq)
    return true;
  if (FVector::DistSquared2D(Location, Boss1ExitDoorLocation) < ThreshSq)
    return true;
  if (FVector::DistSquared2D(Location, Boss2EntranceDoorLocation) < ThreshSq)
    return true;
  if (FVector::DistSquared2D(Location, Boss2ExitDoorLocation) < ThreshSq)
    return true;
  if (FVector::DistSquared2D(Location, Boss3EntranceDoorLocation) < ThreshSq)
    return true;
  if (FVector::DistSquared2D(Location, Boss3ExitDoorLocation) < ThreshSq)
    return true;
  return false;
}

bool ADungeonGenerator::IsNearPropOrAltar(const FVector &Location,
                                          float Threshold) const {
  const float ThreshSq = Threshold * Threshold;
  FVector Altar1Pos = Boss3RoomLocation + FVector(-300.0f, 800.0f, 0.0f);
  FVector Altar2Pos = Boss3RoomLocation + FVector(0.0f, 800.0f, 0.0f);
  FVector Altar3Pos = Boss3RoomLocation + FVector(300.0f, 800.0f, 0.0f);

  if (FVector::DistSquared2D(Location, Altar1Pos) < ThreshSq)
    return true;
  if (FVector::DistSquared2D(Location, Altar2Pos) < ThreshSq)
    return true;
  if (FVector::DistSquared2D(Location, Altar3Pos) < ThreshSq)
    return true;
  return false;
}

bool ADungeonGenerator::IsInsidePlayableMaze(int32 X, int32 Y) const {
  return IsValidCell(X, Y) && GetCell(X, Y) == EDungeonCellType::Corridor;
}

void ADungeonGenerator::BakeToStaticMeshActors() {
  UWorld *World = GetWorld();
  if (!World)
    return;

  ClearBakedActors();

  auto SpawnActorsFromISM = [this, World](UInstancedStaticMeshComponent *ISM,
                                          TSoftObjectPtr<UStaticMesh> &SoftMesh,
                                          const FName &FolderName,
                                          const FString &LabelPrefix) {
    if (!ISM)
      return;
    UStaticMesh *Mesh = SoftMesh.Get();
    if (!Mesh) {
      Mesh = SoftMesh.LoadSynchronous();
    }
    if (!Mesh)
      return;

    const int32 Count = ISM->GetInstanceCount();
    for (int32 i = 0; i < Count; ++i) {
      FTransform InstanceTransform;
      ISM->GetInstanceTransform(i, InstanceTransform, true);

      FActorSpawnParameters SpawnParams;
      SpawnParams.Owner = nullptr;
      SpawnParams.SpawnCollisionHandlingOverride =
          ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

      AStaticMeshActor *SMActor = World->SpawnActor<AStaticMeshActor>(
          AStaticMeshActor::StaticClass(), InstanceTransform, SpawnParams);
      if (SMActor) {
        SMActor->Tags.AddUnique(TEXT("DungeonBakedActor"));
        UStaticMeshComponent *SMC = SMActor->GetStaticMeshComponent();
        if (SMC) {
          SMC->SetMobility(EComponentMobility::Movable);
          SMC->SetStaticMesh(Mesh);
          SMC->SetMobility(EComponentMobility::Static);
          SMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
          SMC->SetCollisionObjectType(ECC_WorldStatic);
          SMC->SetCollisionResponseToAllChannels(ECR_Block);
          SMC->SetCastShadow(true);
          SMC->bCastDynamicShadow = true;
          SMC->bCastShadowAsTwoSided = true;
          SMC->bAffectDistanceFieldLighting = true;
        }
#if WITH_EDITOR
        SMActor->SetFolderPath(FolderName);
        SMActor->SetActorLabel(
            FString::Printf(TEXT("%s_%d"), *LabelPrefix, i + 1));
#endif
        BakedActors.Add(SMActor);
      }
    }
    ISM->ClearInstances();
  };

  // Pillars
  SpawnActorsFromISM(PillarInstances, PillarMesh,
                     FName(TEXT("Dungeon_Pillars")), TEXT("Pillar"));

  // Walls
  SpawnActorsFromISM(WallInstances, WallMesh, FName(TEXT("Dungeon_Walls")),
                     TEXT("Wall"));
  SpawnActorsFromISM(DecorativeWallInstances, DecorativeWallMesh,
                     FName(TEXT("Dungeon_Walls")), TEXT("Wall_Decorative"));

  // Doorways
  SpawnActorsFromISM(DoorwayInstances, DoorwayMesh,
                     FName(TEXT("Dungeon_Doors")), TEXT("Doorway"));

  // Props
  SpawnActorsFromISM(AltarInstances, AltarMesh, FName(TEXT("Dungeon_Props")),
                     TEXT("Altar"));
  SpawnActorsFromISM(CoffinInstances, CoffinMesh, FName(TEXT("Dungeon_Props")),
                     TEXT("Coffin"));
  SpawnActorsFromISM(PotInstances, PotMesh, FName(TEXT("Dungeon_Props")),
                     TEXT("Pot"));
  SpawnActorsFromISM(StatueInstances, StatueMesh, FName(TEXT("Dungeon_Props")),
                     TEXT("Statue"));

  UE_LOG(LogTemp, Log,
         TEXT("DungeonGenerator: Successfully baked %d static mesh actors into "
              "individual objects!"),
         BakedActors.Num());
}

void ADungeonGenerator::ClearBakedActors() {
  for (TWeakObjectPtr<AActor> &ActorPtr : BakedActors) {
    if (ActorPtr.IsValid()) {
      ActorPtr->Destroy();
    }
  }
  BakedActors.Empty();

  UWorld *World = GetWorld();
  if (World) {
    TArray<AActor *> AllActors;
    UGameplayStatics::GetAllActorsOfClass(
        World, AStaticMeshActor::StaticClass(), AllActors);
    for (AActor *Actor : AllActors) {
      if (IsValid(Actor) && Actor != this) {
        bool bIsDungeonActor = false;
        if (Actor->ActorHasTag(TEXT("DungeonBakedActor"))) {
          bIsDungeonActor = true;
        }
#if WITH_EDITOR
        if (!bIsDungeonActor) {
          FName Folder = Actor->GetFolderPath();
          FString Label = Actor->GetActorLabel();
          if (Folder == FName(TEXT("Dungeon_Pillars")) ||
              Folder == FName(TEXT("Dungeon_Walls")) ||
              Folder == FName(TEXT("Dungeon_Doors")) ||
              Folder == FName(TEXT("Dungeon_Floors")) ||
              Folder == FName(TEXT("Dungeon_Ceilings")) ||
              Folder == FName(TEXT("Dungeon_Props")) ||
              Label.StartsWith(TEXT("Wall_")) ||
              Label.StartsWith(TEXT("Pillar_")) ||
              Label.StartsWith(TEXT("Doorway_")) ||
              Label.StartsWith(TEXT("Altar_")) ||
              Label.StartsWith(TEXT("Coffin_")) ||
              Label.StartsWith(TEXT("Pot_")) ||
              Label.StartsWith(TEXT("Statue_"))) {
            bIsDungeonActor = true;
          }
        }
#endif
        if (!bIsDungeonActor) {
          if (AStaticMeshActor *SMA = Cast<AStaticMeshActor>(Actor)) {
            if (UStaticMeshComponent *SMC = SMA->GetStaticMeshComponent()) {
              if (UStaticMesh *Mesh = SMC->GetStaticMesh()) {
                FString MeshName = Mesh->GetName();
                if (MeshName.Contains(TEXT("Crypt")) ||
                    MeshName.Contains(TEXT("SM_Alter")) ||
                    MeshName.Contains(TEXT("SM_Coffin")) ||
                    MeshName.Contains(TEXT("SM_Pot")) ||
                    MeshName.Contains(TEXT("SM_Gargoyle"))) {
                  bIsDungeonActor = true;
                }
              }
            }
          }
        }

        if (bIsDungeonActor) {
#if WITH_EDITOR
          if (!World->IsGameWorld()) {
            World->EditorDestroyActor(Actor, true);
            continue;
          }
#endif
          Actor->Destroy();
        }
      }
    }
  }
}

void ADungeonGenerator::SetBossDefeated(int32 BossIndex, bool bDefeated) {
  switch (BossIndex) {
  case 1:
    bBoss1Defeated = bDefeated;
    break;
  case 2:
    bBoss2Defeated = bDefeated;
    break;
  case 3:
    bBoss3Defeated = bDefeated;
    break;
  default:
    break;
  }

  if (bDefeated) {
    SpawnBossGemDrop(BossIndex);

    // If final boss is defeated, spawn the stone altar in front of the exit
    // door
    if (BossIndex == 3) {
      SpawnExitAltar();
    }
  }
}

void ADungeonGenerator::SpawnBossGemDrop(int32 BossIndex) {
  UWorld *World = GetWorld();
  if (!World)
    return;

  FVector GemLoc;
  const TCHAR *GemMeshPath = TEXT(
      "/Game/_GAME/Assets/AncientTreasures/Meshes/SM_Gems_01a.SM_Gems_01a");
  FLinearColor GlowColor;
  FName GemTag;

  FString RelicName;
  switch (BossIndex) {
  case 1: // Room 1: Sun Gem (Ruby / Crimson-Gold)
    GemLoc = Boss1RoomLocation + FVector(0.0f, 0.0f, 65.0f);
    GemMeshPath = TEXT(
        "/Game/_GAME/Assets/AncientTreasures/Meshes/SM_Gems_01a.SM_Gems_01a");
    GlowColor = FLinearColor(1.0f, 0.25f, 0.1f);
    GemTag = FName(TEXT("Gem_1"));
    RelicName = TEXT("Sun Relic");
    break;
  case 2: // Room 2: Wind Gem (Emerald Green)
    GemLoc = Boss2RoomLocation + FVector(0.0f, 0.0f, 65.0f);
    GemMeshPath = TEXT(
        "/Game/_GAME/Assets/AncientTreasures/Meshes/SM_Gems_01b.SM_Gems_01b");
    GlowColor = FLinearColor(0.1f, 1.0f, 0.35f);
    GemTag = FName(TEXT("Gem_2"));
    RelicName = TEXT("Wind Relic");
    break;
  case 3: // Room 3: Void Gem (Sapphire / Royal Blue)
    GemLoc = Boss3RoomLocation + FVector(0.0f, 0.0f, 65.0f);
    GemMeshPath = TEXT(
        "/Game/_GAME/Assets/AncientTreasures/Meshes/SM_Gems_01c.SM_Gems_01c");
    GlowColor = FLinearColor(0.2f, 0.5f, 1.0f);
    GemTag = FName(TEXT("Gem_3"));
    RelicName = TEXT("Void Relic");
    break;
  default:
    return;
  }

  UStaticMesh *GemMesh = LoadObject<UStaticMesh>(nullptr, GemMeshPath);
  if (!GemMesh)
    return;

  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  ABossGemPickup *GemPickup = World->SpawnActor<ABossGemPickup>(
      ABossGemPickup::StaticClass(), FTransform(FRotator::ZeroRotator, GemLoc),
      SpawnParams);
  if (GemPickup) {
    GemPickup->SetupGem(BossIndex, RelicName, GemMesh, GlowColor);
#if WITH_EDITOR
    GemPickup->SetActorLabel(FString::Printf(TEXT("BossGem_%d"), BossIndex));
    GemPickup->SetFolderPath(FName(TEXT("Dungeon_Gems")));
#endif
    SpawnedGemPickups.Add(GemPickup);
    UE_LOG(LogTemp, Log, TEXT("SpawnBossGemDrop: Spawned interactive ABossGemPickup for %s in Room %d"),
           *RelicName, BossIndex);
  }
}

void ADungeonGenerator::SpawnExitAltar() {
  if (SpawnedAltarActor.IsValid())
    return;
  UWorld *World = GetWorld();
  if (!World)
    return;

  // In front of North exit door in Room 3
  const FVector AltarLoc = Boss3ExitDoorLocation + FVector(0.0f, -350.0f, 0.0f);
  UStaticMesh *LoadedAltarMesh = LoadObject<UStaticMesh>(
      nullptr, TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Alter.SM_Alter"));
  if (!LoadedAltarMesh)
    return;

  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  AActor *AltarActor = World->SpawnActor<AActor>(
      AActor::StaticClass(), FTransform(FRotator(0.0f, 180.0f, 0.0f), AltarLoc),
      SpawnParams);
  if (AltarActor) {
    USceneComponent *RootComp =
        NewObject<USceneComponent>(AltarActor, TEXT("AltarRoot"));
    AltarActor->SetRootComponent(RootComp);
    RootComp->RegisterComponent();
    RootComp->SetWorldLocation(AltarLoc);

    UStaticMeshComponent *MeshComp =
        NewObject<UStaticMeshComponent>(AltarActor, TEXT("AltarMesh"));
    MeshComp->SetupAttachment(RootComp);
    MeshComp->RegisterComponent();
    MeshComp->SetStaticMesh(LoadedAltarMesh);
    MeshComp->SetRelativeScale3D(FVector(1.15f, 1.15f, 1.15f));
    MeshComp->SetCollisionProfileName(TEXT("BlockAll"));

    UPointLightComponent *LightComp =
        NewObject<UPointLightComponent>(AltarActor, TEXT("AltarLight"));
    LightComp->SetupAttachment(RootComp);
    LightComp->RegisterComponent();
    LightComp->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
    LightComp->SetLightColor(FLinearColor(0.85f, 0.92f, 1.0f));
    LightComp->SetIntensity(2500.0f);
    LightComp->SetAttenuationRadius(550.0f);

    AltarActor->Tags.Add(FName(TEXT("ExitAltar")));

#if WITH_EDITOR
    AltarActor->SetActorLabel(TEXT("FinalExitAltar"));
    AltarActor->SetFolderPath(FName(TEXT("Dungeon_Bosses")));
#endif
    SpawnedAltarActor = AltarActor;
  }
}

void ADungeonGenerator::ActivateExitAltar() {
  if (FDungeonGameState::IsAltarActivated())
    return;
  FDungeonGameState::SetAltarActivated(true);

  AActor *AltarActor = SpawnedAltarActor.Get();
  if (!AltarActor && GetWorld()) {
    TArray<AActor *> FoundAltars;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("ExitAltar")),
                                          FoundAltars);
    if (FoundAltars.Num() > 0)
      AltarActor = FoundAltars[0];
  }

  if (AltarActor) {
    // Place 3 gem static meshes onto the top surface of the altar in 3 distinct
    // slot positions
    const FVector SlotOffsets[3] = {FVector(-45.0f, 0.0f, 95.0f),
                                    FVector(0.0f, 0.0f, 95.0f),
                                    FVector(45.0f, 0.0f, 95.0f)};
    const TCHAR *GemMeshPaths[3] = {TEXT("/Game/_GAME/Assets/AncientTreasures/"
                                         "Meshes/SM_Gems_01a.SM_Gems_01a"),
                                    TEXT("/Game/_GAME/Assets/AncientTreasures/"
                                         "Meshes/SM_Gems_01b.SM_Gems_01b"),
                                    TEXT("/Game/_GAME/Assets/AncientTreasures/"
                                         "Meshes/SM_Gems_01c.SM_Gems_01c")};
    const FLinearColor GemColors[3] = {FLinearColor(1.0f, 0.25f, 0.1f),
                                       FLinearColor(0.1f, 1.0f, 0.35f),
                                       FLinearColor(0.2f, 0.5f, 1.0f)};

    for (int32 i = 0; i < 3; ++i) {
      UStaticMesh *GemMesh = LoadObject<UStaticMesh>(nullptr, GemMeshPaths[i]);
      if (GemMesh) {
        UStaticMeshComponent *SlotGem = NewObject<UStaticMeshComponent>(
            AltarActor, *FString::Printf(TEXT("SlotGem_%d"), i + 1));
        SlotGem->SetupAttachment(AltarActor->GetRootComponent());
        SlotGem->RegisterComponent();
        SlotGem->SetStaticMesh(GemMesh);
        SlotGem->SetRelativeLocation(SlotOffsets[i]);
        SlotGem->SetRelativeScale3D(FVector(2.4f, 2.4f, 2.4f));
        SlotGem->SetCollisionProfileName(TEXT("NoCollision"));

        UPointLightComponent *SlotLight = NewObject<UPointLightComponent>(
            AltarActor, *FString::Printf(TEXT("SlotLight_%d"), i + 1));
        SlotLight->SetupAttachment(SlotGem);
        SlotLight->RegisterComponent();
        SlotLight->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));
        SlotLight->SetLightColor(GemColors[i]);
        SlotLight->SetIntensity(2200.0f);
        SlotLight->SetAttenuationRadius(250.0f);
      }
    }

    // Play magical chime / activation sound
    USoundBase *ChimeSound = LoadObject<USoundBase>(
        nullptr, TEXT("/Game/Audio/SoulPickup.SoulPickup"));
    if (ChimeSound && GetWorld()) {
      UGameplayStatics::PlaySoundAtLocation(GetWorld(), ChimeSound,
                                            AltarActor->GetActorLocation());
    }
  }

  // Open final exit door
  if (GetWorld()) {
    TArray<AActor *> FoundGates;
    UGameplayStatics::GetAllActorsWithTag(
        GetWorld(), FName(TEXT("BossArenaGate")), FoundGates);
    for (AActor *GateActor : FoundGates) {
      if (ABossArenaGate *Gate = Cast<ABossArenaGate>(GateActor)) {
        if (!Gate->bIsEntranceGate &&
            FVector::Dist2D(Gate->GetActorLocation(), Boss3ExitDoorLocation) <
                600.0f) {
          Gate->OpenGate();
        }
      }
    }
  }

  // Spawn Victory Portal
  SpawnVictoryPortal();
}

void ADungeonGenerator::SpawnVictoryPortal() {
  if (SpawnedVictoryPortal.IsValid())
    return;
  UWorld *World = GetWorld();
  if (!World)
    return;

  // In exit doorway of Room 3
  const FVector PortalLoc = Boss3ExitDoorLocation + FVector(0.0f, 0.0f, 20.0f);
  UStaticMesh *PortalMesh = LoadObject<UStaticMesh>(
      nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  AActor *PortalActor = World->SpawnActor<AActor>(
      AActor::StaticClass(), FTransform(FRotator(90.0f, 0.0f, 0.0f), PortalLoc),
      SpawnParams);
  if (PortalActor) {
    USceneComponent *RootComp =
        NewObject<USceneComponent>(PortalActor, TEXT("PortalRoot"));
    PortalActor->SetRootComponent(RootComp);
    RootComp->RegisterComponent();
    RootComp->SetWorldLocation(PortalLoc);

    if (PortalMesh) {
      UStaticMeshComponent *MeshComp =
          NewObject<UStaticMeshComponent>(PortalActor, TEXT("PortalMesh"));
      MeshComp->SetupAttachment(RootComp);
      MeshComp->RegisterComponent();
      MeshComp->SetStaticMesh(PortalMesh);
      MeshComp->SetRelativeScale3D(FVector(2.0f, 2.0f, 2.5f));
      MeshComp->SetCollisionProfileName(TEXT("NoCollision"));

      URotatingMovementComponent *RotComp =
          NewObject<URotatingMovementComponent>(PortalActor,
                                                TEXT("PortalRotate"));
      RotComp->RegisterComponent();
      RotComp->RotationRate = FRotator(0.0f, 0.0f, 75.0f);
    }

    UPointLightComponent *PortalLight =
        NewObject<UPointLightComponent>(PortalActor, TEXT("PortalLight"));
    PortalLight->SetupAttachment(RootComp);
    PortalLight->RegisterComponent();
    PortalLight->SetLightColor(FLinearColor(0.25f, 0.85f, 1.0f));
    PortalLight->SetIntensity(7000.0f);
    PortalLight->SetAttenuationRadius(700.0f);

    PortalActor->Tags.Add(FName(TEXT("VictoryPortal")));

#if WITH_EDITOR
    PortalActor->SetActorLabel(TEXT("VictoryPortal"));
    PortalActor->SetFolderPath(FName(TEXT("Dungeon_Bosses")));
#endif
    SpawnedVictoryPortal = PortalActor;
  }
}

bool ADungeonGenerator::IsBossDefeated(int32 BossIndex) const {
  switch (BossIndex) {
  case 1:
    return bBoss1Defeated;
  case 2:
    return bBoss2Defeated;
  case 3:
    return bBoss3Defeated;
  default:
    return false;
  }
}

int32 ADungeonGenerator::GetBossRoomAtLocation(
    const FVector &WorldLocation) const {
  if (TileSize <= 0) {
    return 0;
  }

  // Transform world location to local dungeon grid space
  const FVector LocalPos =
      GetActorTransform().InverseTransformPosition(WorldLocation);
  const float FTile = (float)TileSize;

  const int32 TileX = FMath::FloorToInt(LocalPos.X / FTile);
  const int32 TileY = FMath::FloorToInt(LocalPos.Y / FTile);

  // Dynamically check Room 1, Room 2, and Room 3 bounds
  if (Rooms.IsValidIndex(Boss1RoomIndex)) {
    const FDungeonRoom &R1 = Rooms[Boss1RoomIndex];
    if (TileX >= R1.X && TileX < R1.X + R1.Width && TileY >= R1.Y &&
        TileY < R1.Y + R1.Height) {
      return 1;
    }
  }

  if (Rooms.IsValidIndex(Boss2RoomIndex)) {
    const FDungeonRoom &R2 = Rooms[Boss2RoomIndex];
    if (TileX >= R2.X && TileX < R2.X + R2.Width && TileY >= R2.Y &&
        TileY < R2.Y + R2.Height) {
      return 2;
    }
  }

  if (Rooms.IsValidIndex(Boss3RoomIndex)) {
    const FDungeonRoom &R3 = Rooms[Boss3RoomIndex];
    if (TileX >= R3.X && TileX < R3.X + R3.Width && TileY >= R3.Y &&
        TileY < R3.Y + R3.Height) {
      return 3;
    }
  }

  return 0;
}

bool ADungeonGenerator::IsInsideBossRoom(const FVector &WorldLocation) const {
  return GetBossRoomAtLocation(WorldLocation) > 0;
}

void ADungeonGenerator::ClearSpawnedTraps() {
  UWorld *World = GetWorld();
  if (!World)
    return;

  TSet<AActor *> TrapsToDestroy;

  for (TWeakObjectPtr<AActor> &TrapPtr : SpawnedTraps) {
    if (TrapPtr.IsValid()) {
      TrapsToDestroy.Add(TrapPtr.Get());
    }
  }
  SpawnedTraps.Empty();

  TArray<AActor *> AttachedActors;
  GetAttachedActors(AttachedActors);
  for (AActor *Child : AttachedActors) {
    if (IsValid(Child) &&
        (Child->ActorHasTag(TEXT("DungeonTrap")) ||
         (SpikeTrapClass && Child->IsA(SpikeTrapClass)) ||
         Child->IsA(ASpikeTrap::StaticClass()) ||
         (WallDartTrapClass && Child->IsA(WallDartTrapClass)) ||
         Child->IsA(AWallDartTrap::StaticClass()))) {
      TrapsToDestroy.Add(Child);
    }
  }

  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Actor = *It;
    if (IsValid(Actor) && Actor != this) {
      bool bIsTrap = Actor->ActorHasTag(TEXT("DungeonTrap")) ||
                     (SpikeTrapClass && Actor->IsA(SpikeTrapClass)) ||
                     Actor->IsA(ASpikeTrap::StaticClass()) ||
                     (WallDartTrapClass && Actor->IsA(WallDartTrapClass)) ||
                     Actor->IsA(AWallDartTrap::StaticClass());
#if WITH_EDITOR
      if (!bIsTrap) {
        FName Folder = Actor->GetFolderPath();
        FString Label = Actor->GetActorLabel();
        if (Folder == FName(TEXT("Dungeon_Traps")) ||
            Label.StartsWith(TEXT("Trap_")) ||
            Label.StartsWith(TEXT("SpikeTrap")) ||
            Label.StartsWith(TEXT("WallDartTrap"))) {
          bIsTrap = true;
        }
      }
#endif
      if (bIsTrap) {
        TrapsToDestroy.Add(Actor);
      }
    }
  }

  for (AActor *Trap : TrapsToDestroy) {
    if (IsValid(Trap)) {
      if (Trap->GetAttachParentActor()) {
        Trap->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
      }
#if WITH_EDITOR
      if (World && !World->IsGameWorld()) {
        World->EditorDestroyActor(Trap, true);
        continue;
      }
#endif
      Trap->Destroy();
    }
  }
}

void ADungeonGenerator::PlaceProceduralTraps() {
  UWorld *World = GetWorld();
  if (!World)
    return;

  ClearSpawnedTraps();

  TSet<FIntPoint> PlacedTrapTiles;

  // Key safety exclusion zones (Manhattan distance checks)
  const FIntPoint SpawnTile(3, 3);
  TArray<FIntPoint> BossDoorTiles;
  auto WorldToTile = [this](const FVector &WorldLoc) -> FIntPoint {
    const FVector LocalPos =
        GetActorTransform().InverseTransformPosition(WorldLoc);
    return FIntPoint(FMath::FloorToInt(LocalPos.X / (float)TileSize),
                     FMath::FloorToInt(LocalPos.Y / (float)TileSize));
  };
  BossDoorTiles.Add(WorldToTile(Boss1EntranceDoorLocation));
  BossDoorTiles.Add(WorldToTile(Boss1ExitDoorLocation));
  BossDoorTiles.Add(WorldToTile(Boss2EntranceDoorLocation));
  BossDoorTiles.Add(WorldToTile(Boss2ExitDoorLocation));
  BossDoorTiles.Add(WorldToTile(Boss3EntranceDoorLocation));
  BossDoorTiles.Add(WorldToTile(Boss3ExitDoorLocation));

  const float FTile = (float)TileSize;
  int32 DartTrapStreak = 0; // Tracks consecutive dart traps to prevent long runs without pressure plates

  for (int32 X = 1; X < GridSize - 1; ++X) {
    for (int32 Y = 1; Y < GridSize - 1; ++Y) {
      const int32 CellIdx = GetGridIndex(X, Y);
      if (!Grid.IsValidIndex(CellIdx) ||
          Grid[CellIdx] != EDungeonCellType::Corridor) {
        continue;
      }

      // 1. Safety check: Never within 4 tiles of spawn
      const int32 DistToSpawn =
          FMath::Abs(X - SpawnTile.X) + FMath::Abs(Y - SpawnTile.Y);
      if (DistToSpawn <= 4) {
        continue;
      }

      // 2. Safety check: Never within 2 tiles of any boss doorway
      bool bNearDoor = false;
      for (const FIntPoint &Door : BossDoorTiles) {
        if (FMath::Abs(X - Door.X) + FMath::Abs(Y - Door.Y) <= 2) {
          bNearDoor = true;
          break;
        }
      }
      if (bNearDoor) {
        continue;
      }

      // 3. Spacing check: Never place adjacent to an existing trap
      if (PlacedTrapTiles.Contains(FIntPoint(X - 1, Y)) ||
          PlacedTrapTiles.Contains(FIntPoint(X + 1, Y)) ||
          PlacedTrapTiles.Contains(FIntPoint(X, Y - 1)) ||
          PlacedTrapTiles.Contains(FIntPoint(X, Y + 1))) {
        continue;
      }

      // 4. Sector density threshold
      float TargetDensity = TrapDensitySector1;
      if (X >= 20 || Y >= 20) {
        TargetDensity = TrapDensitySector3;
      } else if (X >= 10 || Y >= 10) {
        TargetDensity = TrapDensitySector2;
      }

      // Pseudo-random roll using dungeon Seed
      const float Roll = FMath::FRand();
      if (Roll > TargetDensity) {
        continue;
      }

      // 5. Determine alley orientation for wall-mounted traps
      const bool bSolidWest = !IsWalkable(X - 1, Y);
      const bool bSolidEast = !IsWalkable(X + 1, Y);
      const bool bSolidNorth = !IsWalkable(X, Y + 1);
      const bool bSolidSouth = !IsWalkable(X, Y - 1);

      const bool bNorthSouthAlley = bSolidWest && bSolidEast;
      const bool bEastWestAlley = bSolidNorth && bSolidSouth;

      FActorSpawnParameters SpawnParams;
      SpawnParams.SpawnCollisionHandlingOverride =
          ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
      SpawnParams.OverrideLevel = World->GetCurrentLevel();

      AActor *NewTrap = nullptr;

      // Determine trap type: heavily favors floor pressure plates (ASpikeTrap) across the dungeon
      const bool bCanBeDartTrap = (bNorthSouthAlley || bEastWestAlley);
      bool bSpawnDartTrap = false;

      if (bCanBeDartTrap) {
        if (DartTrapStreak >= 2) {
          bSpawnDartTrap = false; // Cap consecutive tripwires at 2 to guarantee frequent pressure plates
        } else {
          // Roll against WallDartTrapRatio (default 0.35 = 35% dart / 65% pressure plate)
          bSpawnDartTrap = (FMath::FRand() < WallDartTrapRatio);
        }
      }

      if (bSpawnDartTrap) {
        TSubclassOf<AWallDartTrap> DartTrapClass =
            WallDartTrapClass
                ? WallDartTrapClass
                : TSubclassOf<AWallDartTrap>(AWallDartTrap::StaticClass());

        // Align dispenser and receiver flush against the solid walls
        FRotator TrapRot = bNorthSouthAlley ? FRotator(0.0f, 0.0f, 0.0f)
                                            : FRotator(0.0f, 90.0f, 0.0f);
        FVector TrapWorldLoc = GridToWorldCenter(X, Y, 0.0f);

        // Small longitudinal variation along corridor axis (+/- 35 cm) so
        // tripwires aren't strictly grid-centered
        if (bNorthSouthAlley) {
          TrapWorldLoc.Y += FMath::FRandRange(-35.0f, 35.0f);
        } else {
          TrapWorldLoc.X += FMath::FRandRange(-35.0f, 35.0f);
        }

        FTransform TrapTransform(TrapRot, TrapWorldLoc,
                                 FVector(1.0f, 1.0f, 1.0f));

        AWallDartTrap *DartTrap = World->SpawnActor<AWallDartTrap>(
            DartTrapClass, TrapTransform, SpawnParams);
        if (DartTrap) {
          NewTrap = DartTrap;
          DartTrapStreak++;
#if WITH_EDITOR
          DartTrap->SetActorLabel(
              FString::Printf(TEXT("WallDartTrap_%d_%d"), X, Y));
#endif
        }
      } else {
        // Floor pressure plate spike trap embedded flush in stone floor (1 for every 10 tripwires)
        TSubclassOf<ASpikeTrap> TrapClassToSpawn =
            SpikeTrapClass ? SpikeTrapClass
                           : TSubclassOf<ASpikeTrap>(ASpikeTrap::StaticClass());

        FVector TrapWorldLoc = GridToWorldCenter(X, Y, 0.0f);
        FRotator TrapRot = FRotator::ZeroRotator;
        FTransform TrapTransform(TrapRot, TrapWorldLoc,
                                 FVector(1.0f, 1.0f, 1.0f));

        ASpikeTrap *SpikeTrap = World->SpawnActor<ASpikeTrap>(
            TrapClassToSpawn, TrapTransform, SpawnParams);
        if (SpikeTrap) {
          SpikeTrap->bIsPeriodic = false; // Pure pressure-plate
          NewTrap = SpikeTrap;
          DartTrapStreak = 0; // Reset streak after pressure plate is placed
#if WITH_EDITOR
          SpikeTrap->SetActorLabel(
              FString::Printf(TEXT("SpikeTrap_%d_%d"), X, Y));
#endif
        }
      }

      if (NewTrap) {
        NewTrap->Tags.AddUnique(TEXT("DungeonTrap"));

        // Crucial: Detach so Unreal puts it into the top-level Dungeon_Traps
        // Outliner folder!
        if (USceneComponent *Root = NewTrap->GetRootComponent()) {
          Root->DetachFromComponent(
              FDetachmentTransformRules::KeepWorldTransform);
        }
        NewTrap->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

#if WITH_EDITOR
        NewTrap->SetFolderPath(FName(TEXT("Dungeon_Traps")));
#endif
        SpawnedTraps.Add(NewTrap);
        PlacedTrapTiles.Add(FIntPoint(X, Y));
      }
    }
  }

  UE_LOG(LogTemp, Log,
         TEXT("Procedural Traps Spawned: %d traps across the labyrinth."),
         PlacedTrapTiles.Num());
}

void ADungeonGenerator::RandomizeBosses() {
  SpawnBossEncounters();
}

void ADungeonGenerator::SpawnBossEncounters() {
  UWorld *World = GetWorld();
  if (!World)
    return;

  if (FUObjectThreadContext::Get().IsRoutingPostLoad) {
    return;
  }

  ClearBossEncounters();

  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  SpawnParams.OverrideLevel = World->GetCurrentLevel();

  TSubclassOf<ABossArenaGate> GateClass =
      BossArenaGateClass
          ? BossArenaGateClass
          : TSubclassOf<ABossArenaGate>(ABossArenaGate::StaticClass());
  TSubclassOf<ABossShadowKnight> ShadowKnightClass =
      BossShadowKnightClass
          ? BossShadowKnightClass
          : TSubclassOf<ABossShadowKnight>(ABossShadowKnight::StaticClass());
  TSubclassOf<ABossGrux> GruxClass =
      BossGruxClass ? BossGruxClass
                    : TSubclassOf<ABossGrux>(ABossGrux::StaticClass());
  TSubclassOf<ABossMinotaur> MinotaurClass =
      BossMinotaurClass
          ? BossMinotaurClass
          : TSubclassOf<ABossMinotaur>(ABossMinotaur::StaticClass());

  // -------------------------------------------------------------------------
  // 1. Prepare and randomize the boss roster for this match/run
  // -------------------------------------------------------------------------
  enum class EDungeonBossType : uint8 { ShadowKnight, Grux, Minotaur };

  TArray<EDungeonBossType> BossPool;
  BossPool.Add(EDungeonBossType::Minotaur); // Aurelius the Iron Bastion
  BossPool.Add(EDungeonBossType::Grux);     // Valakor the Twin-Blade Berserker
  BossPool.Add(EDungeonBossType::ShadowKnight); // Malikor the Shadow Knight

  if (bRandomizeBossRoster) {
    if (bAllowDuplicateBosses) {
      const EDungeonBossType AllTypes[3] = {
          EDungeonBossType::Minotaur, EDungeonBossType::Grux,
          EDungeonBossType::ShadowKnight};
      for (int32 i = 0; i < 3; ++i) {
        BossPool[i] = AllTypes[FMath::RandRange(0, 2)];
      }
    } else {
      // Fisher-Yates shuffle ensuring all 3 unique bosses appear in randomized order
      for (int32 i = BossPool.Num() - 1; i > 0; --i) {
        const int32 j = FMath::RandRange(0, i);
        BossPool.Swap(i, j);
      }
    }
  }

  // -------------------------------------------------------------------------
  // 2. Define the room parameters for all 3 Boss Arenas
  // -------------------------------------------------------------------------
  struct FRoomEncounterDef {
    int32 RoomNumber;
    FVector RoomCenter;
    FVector EntranceDoorLoc;
    FRotator EntranceDoorRot;
    FVector ExitDoorLoc;
    FRotator ExitDoorRot;
    FRotator BossFacingRot;
  };

  const FRoomEncounterDef RoomDefs[3] = {
      // Room 1 (Sector 1 Arena): Entrance at South, Exit at East, Boss faces South toward entrance
      {1, Boss1RoomLocation, Boss1EntranceDoorLocation,
       FRotator(0.0f, 0.0f, 0.0f), Boss1ExitDoorLocation,
       FRotator(0.0f, -90.0f, 0.0f), FRotator(0.0f, -90.0f, 0.0f)},
      // Room 2 (Sector 2 Arena): Entrance at North, Exit at East, Boss faces North toward entrance
      {2, Boss2RoomLocation, Boss2EntranceDoorLocation,
       FRotator(0.0f, 180.0f, 0.0f), Boss2ExitDoorLocation,
       FRotator(0.0f, -90.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f)},
      // Room 3 (Sector 3 Arena): Entrance at South, Exit at North, Boss faces South toward entrance
      {3, Boss3RoomLocation, Boss3EntranceDoorLocation,
       FRotator(0.0f, 0.0f, 0.0f), Boss3ExitDoorLocation,
       FRotator(0.0f, 0.0f, 0.0f), FRotator(0.0f, -90.0f, 0.0f)}};

  // -------------------------------------------------------------------------
  // 3. Helper lambda to spawn gates and boss for a given room configuration
  // -------------------------------------------------------------------------
  auto SpawnRoomEncounter = [&](const FRoomEncounterDef &Def,
                                EDungeonBossType BossType,
                                TWeakObjectPtr<AActor> &OutSpawnedBossPtr,
                                FVector &OutSpawnLoc) {
    // A. Entrance Gate
    ABossArenaGate *EnterGate = World->SpawnActor<ABossArenaGate>(
        GateClass, FTransform(Def.EntranceDoorRot, Def.EntranceDoorLoc),
        SpawnParams);
    if (EnterGate) {
      EnterGate->bIsEntranceGate = true;
      if (USceneComponent *Root = EnterGate->GetRootComponent()) {
        Root->DetachFromComponent(
            FDetachmentTransformRules::KeepWorldTransform);
      }
      EnterGate->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
#if WITH_EDITOR
      EnterGate->SetActorLabel(
          FString::Printf(TEXT("Boss%d_EntranceGate"), Def.RoomNumber));
      EnterGate->SetFolderPath(FName(TEXT("Dungeon_Bosses")));
#endif
      SpawnedArenaGates.Add(EnterGate);
    }

    // B. Exit Gate
    ABossArenaGate *ExitGate = World->SpawnActor<ABossArenaGate>(
        GateClass, FTransform(Def.ExitDoorRot, Def.ExitDoorLoc), SpawnParams);
    if (ExitGate) {
      ExitGate->bIsEntranceGate = false;
      if (USceneComponent *Root = ExitGate->GetRootComponent()) {
        Root->DetachFromComponent(
            FDetachmentTransformRules::KeepWorldTransform);
      }
      ExitGate->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
#if WITH_EDITOR
      ExitGate->SetActorLabel(
          FString::Printf(TEXT("Boss%d_ExitGate"), Def.RoomNumber));
      ExitGate->SetFolderPath(FName(TEXT("Dungeon_Bosses")));
#endif
      SpawnedArenaGates.Add(ExitGate);
    }

    // C. Spawn Randomized Boss Actor
    AActor *SpawnedBossActor = nullptr;
    switch (BossType) {
    case EDungeonBossType::ShadowKnight: {
      const float CapsuleHalfZ = 110.0f;
      OutSpawnLoc = FVector(Def.RoomCenter.X, Def.RoomCenter.Y, CapsuleHalfZ);
      ABossShadowKnight *ShadowKnight = World->SpawnActor<ABossShadowKnight>(
          ShadowKnightClass, FTransform(Def.BossFacingRot, OutSpawnLoc),
          SpawnParams);
      if (ShadowKnight) {
        ShadowKnight->EntranceGate = EnterGate;
        ShadowKnight->ExitGate = ExitGate;
        if (EnterGate) {
          EnterGate->LinkedBossShadowKnight = ShadowKnight;
        }
        if (USceneComponent *Root = ShadowKnight->GetRootComponent()) {
          Root->DetachFromComponent(
              FDetachmentTransformRules::KeepWorldTransform);
        }
        ShadowKnight->DetachFromActor(
            FDetachmentTransformRules::KeepWorldTransform);
#if WITH_EDITOR
        ShadowKnight->SetActorLabel(FString::Printf(
            TEXT("Boss%d_Malikor_ShadowKnight"), Def.RoomNumber));
        ShadowKnight->SetFolderPath(FName(TEXT("Dungeon_Bosses")));
#endif
        SpawnedBossActor = ShadowKnight;
      }
      break;
    }

    case EDungeonBossType::Grux: {
      const float CapsuleHalfZ = 120.0f;
      OutSpawnLoc = FVector(Def.RoomCenter.X, Def.RoomCenter.Y, CapsuleHalfZ);
      ABossGrux *Grux = World->SpawnActor<ABossGrux>(
          GruxClass, FTransform(Def.BossFacingRot, OutSpawnLoc), SpawnParams);
      if (Grux) {
        Grux->EntranceGate = EnterGate;
        Grux->ExitGate = ExitGate;
        if (EnterGate) {
          EnterGate->LinkedBossGrux = Grux;
        }
        if (USceneComponent *Root = Grux->GetRootComponent()) {
          Root->DetachFromComponent(
              FDetachmentTransformRules::KeepWorldTransform);
        }
        Grux->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
#if WITH_EDITOR
        Grux->SetActorLabel(
            FString::Printf(TEXT("Boss%d_Valakor_TwinBlade"), Def.RoomNumber));
        Grux->SetFolderPath(FName(TEXT("Dungeon_Bosses")));
#endif
        SpawnedBossActor = Grux;
      }
      break;
    }

    case EDungeonBossType::Minotaur: {
      const float CapsuleHalfZ = 115.0f;
      OutSpawnLoc = FVector(Def.RoomCenter.X, Def.RoomCenter.Y, CapsuleHalfZ);
      ABossMinotaur *Minotaur = World->SpawnActor<ABossMinotaur>(
          MinotaurClass, FTransform(Def.BossFacingRot, OutSpawnLoc),
          SpawnParams);
      if (Minotaur) {
        Minotaur->EntranceGate = EnterGate;
        Minotaur->ExitGate = ExitGate;
        if (EnterGate) {
          EnterGate->LinkedBoss = Minotaur;
        }
        if (USceneComponent *Root = Minotaur->GetRootComponent()) {
          Root->DetachFromComponent(
              FDetachmentTransformRules::KeepWorldTransform);
        }
        Minotaur->DetachFromActor(
            FDetachmentTransformRules::KeepWorldTransform);
#if WITH_EDITOR
        Minotaur->SetActorLabel(FString::Printf(
            TEXT("Boss%d_Aurelius_IronBastion"), Def.RoomNumber));
        Minotaur->SetFolderPath(FName(TEXT("Dungeon_Bosses")));
#endif
        SpawnedBossActor = Minotaur;
      }
      break;
    }
    }

    OutSpawnedBossPtr = SpawnedBossActor;
  };

  // -------------------------------------------------------------------------
  // 4. Spawn each room with its randomly assigned boss
  // -------------------------------------------------------------------------
  SpawnRoomEncounter(RoomDefs[0], BossPool[0], SpawnedBoss1,
                     Boss1SpawnLocation);
  SpawnRoomEncounter(RoomDefs[1], BossPool[1], SpawnedBoss2,
                     Boss2SpawnLocation);
  SpawnRoomEncounter(RoomDefs[2], BossPool[2], SpawnedBoss3,
                     Boss3SpawnLocation);

  auto GetBossName = [](EDungeonBossType Type) -> const TCHAR * {
    switch (Type) {
    case EDungeonBossType::ShadowKnight:
      return TEXT("Malikor the Shadow Knight");
    case EDungeonBossType::Grux:
      return TEXT("Valakor the Twin-Blade Berserker");
    case EDungeonBossType::Minotaur:
      return TEXT("Aurelius the Iron Bastion");
    default:
      return TEXT("Unknown Boss");
    }
  };

  UE_LOG(LogTemp, Log,
         TEXT("Randomized Boss Encounters Spawned: Room 1 = %s, Room 2 = %s, "
              "Room 3 = %s"),
         GetBossName(BossPool[0]), GetBossName(BossPool[1]),
         GetBossName(BossPool[2]));
}

void ADungeonGenerator::ClearBossEncounters() {
  UWorld *World = GetWorld();
  if (!World)
    return;

  // Purge all pre-existing or orphaned boss actors in the level
  TArray<AActor *> OrphanedBosses;
  UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("Boss")),
                                        OrphanedBosses);
  for (AActor *Boss : OrphanedBosses) {
    if (Boss) {
#if WITH_EDITOR
      if (World && !World->IsGameWorld()) {
        World->EditorDestroyActor(Boss, true);
      } else
#endif
      {
        Boss->Destroy();
      }
    }
  }

  SpawnedBoss1.Reset();
  SpawnedBoss2.Reset();
  SpawnedBoss3.Reset();
  bBoss1Defeated = false;
  bBoss2Defeated = false;
  bBoss3Defeated = false;

  // Purge all pre-existing or orphaned arena gates in the level
  TArray<AActor *> OrphanedGates;
  UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("BossArenaGate")),
                                        OrphanedGates);
  for (AActor *Gate : OrphanedGates) {
    if (Gate) {
#if WITH_EDITOR
      if (World && !World->IsGameWorld()) {
        World->EditorDestroyActor(Gate, true);
      } else
#endif
      {
        Gate->Destroy();
      }
    }
  }
  SpawnedArenaGates.Empty();

  // Clean up spawned gem pickups, altar, and portal
  for (TWeakObjectPtr<AActor> &GemPtr : SpawnedGemPickups) {
    if (GemPtr.IsValid()) {
      GemPtr.Get()->Destroy();
    }
  }
  SpawnedGemPickups.Empty();

  if (SpawnedAltarActor.IsValid()) {
    SpawnedAltarActor.Get()->Destroy();
    SpawnedAltarActor.Reset();
  }

  if (SpawnedVictoryPortal.IsValid()) {
    SpawnedVictoryPortal.Get()->Destroy();
    SpawnedVictoryPortal.Reset();
  }

  FDungeonGameState::Reset();
}
