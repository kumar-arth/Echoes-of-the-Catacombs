// Copyright Epic Games, Inc. All Rights Reserved.

#include "EchoesOfTheCatacombGameMode.h"
#include "DungeonGenerator.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AEchoesOfTheCatacombGameMode::AEchoesOfTheCatacombGameMode()
{
	// Set migrated Dark Knight character as the default player pawn
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/_GAME/BP/Characters/Player/DarkKnight/BP_DarkKnight"));
	if (PlayerPawnBPClass.Class != nullptr)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	// Set migrated character controller with Enhanced Input mappings
	static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerBPClass(TEXT("/Game/_GAME/BP/Input/BP_CharacterController"));
	if (PlayerControllerBPClass.Class != nullptr)
	{
		PlayerControllerClass = PlayerControllerBPClass.Class;
	}
}

AActor* AEchoesOfTheCatacombGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	if (UWorld* World = GetWorld())
	{
		ADungeonGenerator* Gen = Cast<ADungeonGenerator>(UGameplayStatics::GetActorOfClass(World, ADungeonGenerator::StaticClass()));
		AActor* PStart = UGameplayStatics::GetActorOfClass(World, APlayerStart::StaticClass());
		if (Gen && PStart)
		{
			if (Gen->PlayerSpawnLocation.IsZero())
			{
				Gen->PlayerSpawnLocation = Gen->GridToWorldCenter(3, 3, 130.0f);
			}
			if (USceneComponent* Root = PStart->GetRootComponent())
			{
				Root->SetMobility(EComponentMobility::Movable);
			}
			PStart->SetActorLocation(Gen->PlayerSpawnLocation);
			PStart->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
			return PStart;
		}
	}
	return Super::FindPlayerStart_Implementation(Player, IncomingName);
}
