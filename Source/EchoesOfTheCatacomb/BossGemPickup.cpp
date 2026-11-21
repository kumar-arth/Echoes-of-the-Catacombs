// Copyright Epic Games, Inc. All Rights Reserved.

#include "BossGemPickup.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Character/MyCharacter.h"
#include "DungeonGenerator.h"

ABossGemPickup::ABossGemPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(SceneRoot);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetRelativeScale3D(FVector(3.2f, 3.2f, 3.2f));

	LightComp = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightComp"));
	LightComp->SetupAttachment(SceneRoot);
	LightComp->SetIntensity(5000.0f);
	LightComp->SetAttenuationRadius(600.0f);
	LightComp->SetCastShadows(false);

	RotComp = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotComp"));
	RotComp->RotationRate = FRotator(0.0f, 65.0f, 0.0f);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->InitSphereRadius(220.0f);
	InteractionSphere->SetCollisionProfileName(TEXT("Trigger"));
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Tags.Add(TEXT("BossGem"));
	Tags.Add(TEXT("Interactable"));
}

void ABossGemPickup::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionSphere)
	{
		InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABossGemPickup::OnSphereOverlap);
		InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ABossGemPickup::OnSphereEndOverlap);
	}
}

void ABossGemPickup::SetupGem(int32 InGemIndex, const FString& InGemName, UStaticMesh* InMesh, const FLinearColor& InColor)
{
	GemIndex = InGemIndex;
	GemName = InGemName;

	if (MeshComp && InMesh)
	{
		MeshComp->SetStaticMesh(InMesh);
	}

	if (LightComp)
	{
		LightComp->SetLightColor(InColor);
	}

	Tags.AddUnique(FName(*FString::Printf(TEXT("Gem_%d"), InGemIndex)));
}

bool ABossGemPickup::CanInteract_Implementation(AActor* Interactor) const
{
	return true;
}

FText ABossGemPickup::GetInteractionPrompt_Implementation() const
{
	return FText::FromString(FString::Printf(TEXT("[E] Pick up %s"), *GemName));
}

int32 ABossGemPickup::GetInteractionPriority_Implementation() const
{
	return 100;
}

void ABossGemPickup::Interact_Implementation(AActor* Interactor)
{
	FDungeonGameState::CollectGem(GemIndex);

	USoundBase* PickupCue = Cast<USoundBase>(
		StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/SoulPickup.SoulPickup")));
	if (PickupCue)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupCue, GetActorLocation(), 1.0f, 1.0f);
	}

	if (AMyCharacter* MyChar = Cast<AMyCharacter>(Interactor))
	{
		MyChar->UnregisterInteractable(this);
	}

	FDungeonGameState::SetPromptText(TEXT(""));

	UE_LOG(LogTemp, Log, TEXT("BossGemPickup: Player collected %s (Gem %d). Total gems: %d/3"),
		*GemName, GemIndex, FDungeonGameState::GetGemCount());

	Destroy();
}

void ABossGemPickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                     UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                     bool bFromSweep, const FHitResult& SweepResult)
{
	if (AMyCharacter* MyChar = Cast<AMyCharacter>(OtherActor))
	{
		MyChar->RegisterInteractable(this);
		FDungeonGameState::SetPromptText(FString::Printf(TEXT("[E] Pick up %s"), *GemName));
	}
}

void ABossGemPickup::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                       UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (AMyCharacter* MyChar = Cast<AMyCharacter>(OtherActor))
	{
		MyChar->UnregisterInteractable(this);
		FDungeonGameState::SetPromptText(TEXT(""));
	}
}
