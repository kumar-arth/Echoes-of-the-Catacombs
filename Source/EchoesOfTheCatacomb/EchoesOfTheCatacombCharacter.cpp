// Copyright Epic Games, Inc. All Rights Reserved.

#include "EchoesOfTheCatacombCharacter.h"
#include "CompanionGuide.h"
#include "CompanionVoiceComponent.h"
#include "EchoPulseComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
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

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 1000.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Default companion class
	CompanionClass = ACompanionGuide::StaticClass();

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AEchoesOfTheCatacombCharacter::BeginPlay()
{
	Super::BeginPlay();

	// First check if a companion guide is already present in the level
	if (!CompanionInstance && GetWorld())
	{
		CompanionInstance = Cast<ACompanionGuide>(UGameplayStatics::GetActorOfClass(GetWorld(), ACompanionGuide::StaticClass()));
		if (CompanionInstance)
		{
			CompanionInstance->SetFollowTarget(this);
		}
	}

	// Spawn companion guide if configured and not already found
	if (CompanionClass && !CompanionInstance && GetWorld())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector SpawnLoc = GetActorLocation() + GetActorRotation().RotateVector(FVector(-30.0f, 55.0f, 75.0f));
		CompanionInstance = GetWorld()->SpawnActor<ACompanionGuide>(CompanionClass, SpawnLoc, GetActorRotation(), SpawnParams);
		if (CompanionInstance)
		{
			CompanionInstance->SetFollowTarget(this);
		}
	}

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = 1000.f;
	}
}
void AEchoesOfTheCatacombCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AEchoesOfTheCatacombCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AEchoesOfTheCatacombCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AEchoesOfTheCatacombCharacter::Look);
	}
	else
	{
		UE_LOG(LogEchoesOfTheCatacomb, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}

	// Push-to-Talk for Companion Guide Voice (Hold 'V' to talk, release to send)
	PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::StartCompanionListening);
	PlayerInputComponent->BindKey(EKeys::V, IE_Released, this, &AEchoesOfTheCatacombCharacter::StopCompanionListening);

	// Echolocation Pulse from Companion (Press 'F')
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::TriggerCompanionEchoPulse);
}

void AEchoesOfTheCatacombCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AEchoesOfTheCatacombCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AEchoesOfTheCatacombCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AEchoesOfTheCatacombCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AEchoesOfTheCatacombCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AEchoesOfTheCatacombCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}


void AEchoesOfTheCatacombCharacter::StartCompanionListening()
{
	if (CompanionInstance && CompanionInstance->GetVoiceComponent())
	{
		CompanionInstance->GetVoiceComponent()->StartListening();
	}
}

void AEchoesOfTheCatacombCharacter::StopCompanionListening()
{
	if (CompanionInstance && CompanionInstance->GetVoiceComponent())
	{
		CompanionInstance->GetVoiceComponent()->StopListening();
	}
}

void AEchoesOfTheCatacombCharacter::ToggleCompanionListening()
{
	if (CompanionInstance && CompanionInstance->GetVoiceComponent())
	{
		CompanionInstance->GetVoiceComponent()->ToggleListening();
	}
}

void AEchoesOfTheCatacombCharacter::SayCompanion(const FString& Command)
{
	FString CleanCommand = Command.TrimStartAndEnd();

	if (!CompanionInstance && GetWorld())
	{
		CompanionInstance = Cast<ACompanionGuide>(UGameplayStatics::GetActorOfClass(GetWorld(), ACompanionGuide::StaticClass()));
		if (CompanionInstance)
		{
			CompanionInstance->SetFollowTarget(this);
		}
	}

	if (!CompanionInstance)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[SayCompanion]: [ERROR] No CompanionGuide found in level!"));
		}
		UE_LOG(LogEchoesOfTheCatacomb, Error, TEXT("[SayCompanion]: No CompanionGuide found in world."));
		return;
	}

	if (CleanCommand.IsEmpty() || CleanCommand.Equals(TEXT("help"), ESearchCase::IgnoreCase))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 9.0f, FColor::Cyan, TEXT("=== Companion Verbal & Console Commands ==="));
			GEngine->AddOnScreenDebugMessage(-1, 9.0f, FColor::White, TEXT("  SayCompanion stay     -> Hold position & stay anchored in place"));
			GEngine->AddOnScreenDebugMessage(-1, 9.0f, FColor::White, TEXT("  SayCompanion follow   -> Follow close at right shoulder"));
			GEngine->AddOnScreenDebugMessage(-1, 9.0f, FColor::White, TEXT("  SayCompanion run      -> Fall back to safe standoff distance (5m)"));
			GEngine->AddOnScreenDebugMessage(-1, 9.0f, FColor::White, TEXT("  SayCompanion scout    -> Scout hallway ahead (14m)"));
			GEngine->AddOnScreenDebugMessage(-1, 9.0f, FColor::White, TEXT("  SayCompanion concuss  -> Stun/dash closest enemy"));
			GEngine->AddOnScreenDebugMessage(-1, 9.0f, FColor::White, TEXT("  SayCompanion distract -> Strobe light bait monsters"));
			GEngine->AddOnScreenDebugMessage(-1, 9.0f, FColor::White, TEXT("  SayCompanion pulse    -> Echolocation pulse (or press 'F')"));
			GEngine->AddOnScreenDebugMessage(-1, 9.0f, FColor::White, TEXT("  SayCompanion traps    -> Scan for traps"));
			GEngine->AddOnScreenDebugMessage(-1, 9.0f, FColor::White, TEXT("  SayCompanion danger   -> Sense danger / count hostiles"));
			GEngine->AddOnScreenDebugMessage(-1, 9.0f, FColor::White, TEXT("  SayCompanion boss     -> Query boss health"));
			GEngine->AddOnScreenDebugMessage(-1, 9.0f, FColor::White, TEXT("  SayCompanion exit     -> Guide to dungeon exit"));
			GEngine->AddOnScreenDebugMessage(-1, 9.0f, FColor::White, TEXT("  Companion.Say <cmd>   -> Alias for SayCompanion"));
		}
		return;
	}

	if (CompanionInstance->GetVoiceComponent())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, FString::Printf(TEXT("[SayCompanion]: '%s'"), *CleanCommand));
		}
		CompanionInstance->GetVoiceComponent()->ProcessTextCommand(CleanCommand);
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[SayCompanion]: [ERROR] VoiceComponent not found on Companion."));
		}
		UE_LOG(LogEchoesOfTheCatacomb, Warning, TEXT("[SayCompanion]: VoiceComponent not available."));
	}
}

void AEchoesOfTheCatacombCharacter::CompanionSay(const FString& Command)
{
	SayCompanion(Command);
}

void AEchoesOfTheCatacombCharacter::CompanionVoiceTest()
{
	if (UCompanionVoiceComponent* Comp = UCompanionVoiceComponent::GetActiveVoiceComponent())
	{
		Comp->RunVoiceTest(true);
	}
	else if (CompanionInstance && CompanionInstance->GetVoiceComponent())
	{
		CompanionInstance->GetVoiceComponent()->RunVoiceTest(true);
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[Companion.VoiceTest]: [ERROR] No CompanionVoiceComponent found in scene."));
		}
	}
}

void AEchoesOfTheCatacombCharacter::TriggerCompanionEchoPulse()
{
	if (CompanionInstance)
	{
		CompanionInstance->TriggerEcholocationPulse();
	}
	else if (UEchoPulseComponent* PulseComp = FindComponentByClass<UEchoPulseComponent>())
	{
		PulseComp->TriggerPulseFromOwner();
	}
}

void AEchoesOfTheCatacombCharacter::SetGeminiKey(const FString& InApiKey)
{
	if (CompanionInstance && CompanionInstance->GetVoiceComponent())
	{
		CompanionInstance->GetVoiceComponent()->SetGeminiApiKey(InApiKey);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(505, 5.0f, FColor::Green, TEXT("[Companion]: [OK] GEMINI_API_KEY successfully configured!"));
		}
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(505, 5.0f, FColor::Red, TEXT("[Companion]: [ERROR] CompanionInstance or VoiceComponent not found."));
		}
	}
}

void AEchoesOfTheCatacombCharacter::SetGeminiModel(const FString& InModel)
{
	if (CompanionInstance && CompanionInstance->GetVoiceComponent())
	{
		CompanionInstance->GetVoiceComponent()->GeminiModel = InModel;
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(506, 5.0f, FColor::Green, FString::Printf(TEXT("[Companion]: [OK] Gemini Model set to: %s"), *InModel));
		}
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(506, 5.0f, FColor::Red, TEXT("[Companion]: [ERROR] CompanionInstance or VoiceComponent not found."));
		}
	}
}

void AEchoesOfTheCatacombCharacter::OpenMicSettings()
{
	if (CompanionInstance && CompanionInstance->GetVoiceComponent())
	{
		CompanionInstance->GetVoiceComponent()->OpenWindowsMicrophoneSettings();
	}
	else
	{
		FPlatformProcess::LaunchURL(TEXT("ms-settings:privacy-microphone"), nullptr, nullptr);
	}
}

void AEchoesOfTheCatacombCharacter::TestMic()
{
	if (CompanionInstance && CompanionInstance->GetVoiceComponent())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(509, 5.0f, FColor::Cyan, TEXT("[Companion]: [TEST] Testing microphone capture... Hold 'V' and speak, then release."));
		}
		CompanionInstance->GetVoiceComponent()->StartListening();
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(509, 5.0f, FColor::Red, TEXT("[Companion]: [ERROR] CompanionInstance not found."));
		}
	}
}
