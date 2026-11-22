// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CompanionVoiceTypes.h"
#include "CompanionLocalRecognizerInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UCompanionLocalRecognizerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for swappable offline / local keyword and grammar voice recognizers
 * (e.g. Windows Speech Recognition with restricted grammar, Vosk, whisper.cpp tiny).
 */
class ECHOESOFTHECATACOMB_API ICompanionLocalRecognizerInterface
{
	GENERATED_BODY()

public:
	/**
	 * Recognizes commands from live recorded PCM audio samples offline.
	 * Returns true if recognized with high confidence, false to fall back to Gemini.
	 */
	virtual bool RecognizeAudio(const TArray<int16>& InPcmSamples, int32 InSampleRate, int32 InChannels, FCompanionVoiceResult& OutResult) = 0;

	/**
	 * Recognizes commands from text transcripts offline.
	 * Returns true if recognized with high confidence, false to fall back to Gemini.
	 */
	virtual bool RecognizeCommand(const FString& InText, FCompanionVoiceResult& OutResult) = 0;
};
