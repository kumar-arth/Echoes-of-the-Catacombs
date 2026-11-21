// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "EchoesOfTheCatacombCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AEchoesOfTheCatacombCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Companion class to spawn with this character */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Companion", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class ACompanionGuide> CompanionClass;

	/** Active companion instance following this character */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Companion", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class ACompanionGuide> CompanionInstance;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:

	/** Constructor */
	AEchoesOfTheCatacombCharacter();

protected:
	virtual void BeginPlay() override;	

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** Returns CompanionInstance subobject **/
	UFUNCTION(BlueprintPure, Category="Companion")
	class ACompanionGuide* GetCompanionInstance() const { return CompanionInstance; }

	/** Starts capturing voice for companion command (Push-to-Talk pressed) */
	UFUNCTION(BlueprintCallable, Category="Companion|Voice")
	void StartCompanionListening();

	/** Stops capturing voice and sends to companion LLM (Push-to-Talk released) */
	UFUNCTION(BlueprintCallable, Category="Companion|Voice")
	void StopCompanionListening();

	/** Toggles voice listening on / off */
	UFUNCTION(BlueprintCallable, Category="Companion|Voice")
	void ToggleCompanionListening();

	/** Console command for offline / text testing: "SayCompanion <command>" */
	UFUNCTION(Exec)
	void SayCompanion(const FString& Command);

	/** Console command alias for offline / text testing: "Companion.Say <command>" */
	UFUNCTION(Exec)
	void CompanionSay(const FString& Command);

	/** Runs 10-attempt voice diagnostics table: "Companion.VoiceTest" or "CompanionVoiceTest" */
	UFUNCTION(Exec)
	void CompanionVoiceTest();

	/** Sets the Gemini API key from the in-game console: "SetGeminiKey <key>" */
	UFUNCTION(Exec)
	void SetGeminiKey(const FString& InApiKey);

	/** Sets the Gemini model name from in-game console: "SetGeminiModel <model>" */
	UFUNCTION(Exec)
	void SetGeminiModel(const FString& InModel);

	/** Opens Windows microphone privacy settings from in-game console: "OpenMicSettings" */
	UFUNCTION(Exec)
	void OpenMicSettings();

	/** Runs a quick in-game microphone diagnostics test: "TestMic" */
	UFUNCTION(Exec)
	void TestMic();

	/** Handles pressing 'F' to trigger companion echolocation pulse */
	UFUNCTION(BlueprintCallable, Category = "Companion|Echo")
	void TriggerCompanionEchoPulse();
};

