// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CompanionLocalRecognizerInterface.h"
#include "CompanionKeywordRecognizer.generated.h"

/**
 * High-performance, offline speech and keyword recognizer.
 * Supports Windows SAPI speech recognition on live microphone PCM audio
 * and strict tokenized whole-word keyword matching with negation word filtering.
 */
UCLASS()
class ECHOESOFTHECATACOMB_API UCompanionKeywordRecognizer : public UObject, public ICompanionLocalRecognizerInterface
{
	GENERATED_BODY()

public:
	virtual bool RecognizeAudio(const TArray<int16>& InPcmSamples, int32 InSampleRate, int32 InChannels, FCompanionVoiceResult& OutResult) override;
	virtual bool RecognizeCommand(const FString& InText, FCompanionVoiceResult& OutResult) override;

	/** Checks if utterance contains negation words that must force routing to Gemini */
	static bool ContainsNegationWord(const FString& InText);

	/** Checks if any of the target words/phrases match as whole words in the text */
	static bool MatchesWholeWordOrPhrase(const FString& InNormalizedText, const TArray<FString>& InTokens, const TArray<FString>& TargetKeywords);

private:
	/** Attempts speech recognition on Windows using SAPI in-memory stream */
	bool RunWindowsSapiSpeechRecognition(const TArray<int16>& InPcmSamples, int32 InSampleRate, int32 InChannels, FString& OutRecognizedText);
};
