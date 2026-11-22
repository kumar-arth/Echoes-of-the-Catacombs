// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CompanionVoiceTypes.generated.h"

/**
 * Standardized companion verbal commands recognized by local and LLM pipelines.
 * UNKNOWN is explicitly 0 so any uninitialized/unparsed result never defaults to FOLLOW or any action.
 */
UENUM(BlueprintType)
enum class ECompanionVoiceCommand : uint8
{
	UNKNOWN = 0      UMETA(DisplayName = "Unknown / Unclear"),
	FOLLOW           UMETA(DisplayName = "Follow Player"),
	STAY             UMETA(DisplayName = "Stay / Anchor Position"),
	RETREAT          UMETA(DisplayName = "Retreat / Run Safe Distance"),
	ATTACK           UMETA(DisplayName = "Attack / Concuss Target"),
	PULSE            UMETA(DisplayName = "Echolocation Pulse"),
	EXIT             UMETA(DisplayName = "Guide to Exit"),
	SCOUT            UMETA(DisplayName = "Scout Corridor Ahead"),
	TRAPS            UMETA(DisplayName = "Scan Traps"),
	DANGER           UMETA(DisplayName = "Sense Danger"),
	BOSS             UMETA(DisplayName = "Query Boss Health"),
	UPGRADE          UMETA(DisplayName = "Open Upgrade Forge"),
	DISTRACT         UMETA(DisplayName = "Distract Monsters")
};

/**
 * Result of local or cloud voice parsing
 */
USTRUCT(BlueprintType)
struct FCompanionVoiceResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Voice")
	ECompanionVoiceCommand Command = ECompanionVoiceCommand::UNKNOWN;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Voice")
	float Confidence = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Voice")
	FString RawTranscript;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Voice")
	bool bIsLocalFastPath = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Voice")
	float LatencyMs = 0.0f;
};

/**
 * Historical record of a voice attempt for diagnostics and regression testing
 */
USTRUCT(BlueprintType)
struct FCompanionVoiceAttemptRecord
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Voice")
	int32 AttemptIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Voice")
	FString Transcript;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Voice")
	ECompanionVoiceCommand Command = ECompanionVoiceCommand::UNKNOWN;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Voice")
	bool bIsLocalFastPath = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Voice")
	float LatencyMs = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Voice")
	float Confidence = 0.0f;
};
