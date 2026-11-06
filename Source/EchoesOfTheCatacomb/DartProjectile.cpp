// Copyright Epic Games, Inc. All Rights Reserved.

#include "DartProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "WallDartTrap.h"

ADartProjectile::ADartProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. Sphere collision
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(10.0f);
	CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionComp->SetNotifyRigidBodyCollision(true);
	CollisionComp->OnComponentHit.AddDynamic(this, &ADartProjectile::OnHit);
	RootComponent = CollisionComp;

	// 2. Menacing Iron Dart / Spear Mesh
	DartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DartMesh"));
	DartMesh->SetupAttachment(RootComponent);
	DartMesh->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.9f));
	DartMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	DartMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		DartMesh->SetStaticMesh(CylinderFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> IronMatFinder(
		TEXT("/Game/MedievalDungeon/Materials/M_Metal_Decor_Inst.M_Metal_Decor_Inst"));
	if (IronMatFinder.Succeeded())
	{
		DartMesh->SetMaterial(0, IronMatFinder.Object);
	}

	// 3. Glowing Fiery Amber Tracer Light
	DartLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("DartLight"));
	DartLight->SetupAttachment(RootComponent);
	DartLight->SetLightColor(FLinearColor(1.0f, 0.45f, 0.08f));
	DartLight->SetIntensity(3000.0f);
	DartLight->SetAttenuationRadius(250.0f);
	DartLight->SetCastShadows(false);

	// 4. Projectile movement (fast deadly velocity)
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 2600.0f;
	ProjectileMovement->MaxSpeed = 2600.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bShouldBounce = false;

	InitialLifeSpan = 1.5f;
	Tags.AddUnique(TEXT("DungeonTrapDart"));
}

void ADartProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
                           UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Strictly ignore self, owner trap, or any wall dart trap components
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner() || OtherActor->IsA(AWallDartTrap::StaticClass()))
	{
		return;
	}

	// Hit player or character
	if (ACharacter* Char = Cast<ACharacter>(OtherActor))
	{
		UGameplayStatics::ApplyDamage(Char, DamageAmount, nullptr, this, UDamageType::StaticClass());

		// Knock the player back in the direction of the dart flight
		const FVector HitDir = ProjectileMovement ? ProjectileMovement->Velocity.GetSafeNormal() : GetActorForwardVector();
		Char->LaunchCharacter(HitDir * 550.0f + FVector(0.0f, 0.0f, 150.0f), true, true);
	}

	Destroy();
}
