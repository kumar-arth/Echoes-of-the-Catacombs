// Copyright Epic Games, Inc. All Rights Reserved.

#include "EchoesOfTheCatacombCharacter.h"
#include "DungeonHUDWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/DamageEvents.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "EchoesOfTheCatacomb.h"

AEchoesOfTheCatacombCharacter::AEchoesOfTheCatacombCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 1000.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Attack sound
	static ConstructorHelpers::FObjectFinder<USoundBase> WhooshSoundFinder(
		TEXT("/Game/ParagonGrux/Characters/Heroes/Grux/Sounds/SoundCues/Grux_Effort_Swing.Grux_Effort_Swing"));
	if (WhooshSoundFinder.Succeeded())
	{
		AttackSound = WhooshSoundFinder.Object;
	}

	CurrentHealth = MaxHealth;
}

void AEchoesOfTheCatacombCharacter::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = 1000.f;
	}

	// Create and attach Screen-Space Top-Left Health Bar HUD
	if (IsLocallyControlled() && GEngine && GEngine->GameViewport)
	{
		DungeonHUDWidget = SNew(SDungeonHUDWidget).Character(this);
		GEngine->GameViewport->AddViewportWidgetContent(DungeonHUDWidget.ToSharedRef(), 100);
	}
}

void AEchoesOfTheCatacombCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up Enhanced Input action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AEchoesOfTheCatacombCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AEchoesOfTheCatacombCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AEchoesOfTheCatacombCharacter::Look);
	}

	// Direct combat keys: Left Click to attack, Shift or Alt to dodge
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::PerformAttack);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::PerformDodge);
	PlayerInputComponent->BindKey(EKeys::LeftAlt, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::PerformDodge);
}

void AEchoesOfTheCatacombCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void AEchoesOfTheCatacombCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AEchoesOfTheCatacombCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AEchoesOfTheCatacombCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AEchoesOfTheCatacombCharacter::DoJumpStart()
{
	Jump();
}

void AEchoesOfTheCatacombCharacter::DoJumpEnd()
{
	StopJumping();
}

void AEchoesOfTheCatacombCharacter::PerformAttack()
{
	if (bIsAttacking || bIsDodging) return;

	bIsAttacking = true;
	GetWorldTimerManager().SetTimer(AttackCooldownHandle, this, &AEchoesOfTheCatacombCharacter::ResetAttackState, 0.35f, false);

	if (AttackSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, AttackSound, GetActorLocation());
	}

	// Sphere sweep in front of player
	const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 15.0f);
	const FVector End = Start + (GetActorForwardVector() * AttackRange);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	const bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult, Start, End, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(55.0f), QueryParams);

	if (bHit && HitResult.GetActor())
	{
		AActor* HitActor = HitResult.GetActor();
		UGameplayStatics::ApplyDamage(HitActor, AttackDamage, GetController(), this, UDamageType::StaticClass());

		// Extra visual debug or knock impulse
		if (ACharacter* EnemyChar = Cast<ACharacter>(HitActor))
		{
			EnemyChar->LaunchCharacter(GetActorForwardVector() * 250.0f, false, false);
		}
	}
}

void AEchoesOfTheCatacombCharacter::PerformDodge()
{
	if (bIsDodging) return;

	bIsDodging = true;
	GetWorldTimerManager().SetTimer(DodgeDurationHandle, this, &AEchoesOfTheCatacombCharacter::ResetDodgeState, 0.45f, false);

	const FVector MoveInput = GetLastMovementInputVector();
	const FVector DodgeDir = MoveInput.IsNearlyZero() ? GetActorForwardVector() : MoveInput.GetSafeNormal();

	LaunchCharacter(DodgeDir * 920.0f + FVector(0.0f, 0.0f, 50.0f), true, true);
}

void AEchoesOfTheCatacombCharacter::ResetAttackState()
{
	bIsAttacking = false;
}

void AEchoesOfTheCatacombCharacter::ResetDodgeState()
{
	bIsDodging = false;
}

void AEchoesOfTheCatacombCharacter::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	FDamageEvent DmgEvent;
	TakeDamage(Damage, DmgEvent, DamageCauser ? DamageCauser->GetInstigatorController() : nullptr, DamageCauser);
}

float AEchoesOfTheCatacombCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
                                              AController* EventInstigator, AActor* DamageCauser)
{
	// Invulnerability frames while dodge rolling!
	if (bIsDodging)
	{
		return 0.0f;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		HandleDeath();
	}

	return DamageAmount;
}

void AEchoesOfTheCatacombCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

	if (DungeonHUDWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(DungeonHUDWidget.ToSharedRef());
		DungeonHUDWidget.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void AEchoesOfTheCatacombCharacter::HandleDeath()
{
	// Disable player input and movement
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// On-screen notification so the player knows they took fatal damage
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("YOU DIED - Slain in the Catacombs"));
	}

	// 1.5s delay before reloading at spawn point
	GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AEchoesOfTheCatacombCharacter::ReloadCurrentLevel, 1.5f, false);
}

void AEchoesOfTheCatacombCharacter::ReloadCurrentLevel()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::OpenLevel(this, FName(*World->GetName()), false);
	}
}

void AEchoesOfTheCatacombCharacter::ApplyHealing(float Healing, AActor* Healer)
{
	CurrentHealth = FMath::Clamp(CurrentHealth + Healing, 0.0f, MaxHealth);
}
