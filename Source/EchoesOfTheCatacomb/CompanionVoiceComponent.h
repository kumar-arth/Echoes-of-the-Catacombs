// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "Generators/AudioGenerator.h"
#include "CompanionVoiceTypes.h"
#include "CompanionLocalRecognizerInterface.h"
#include "CompanionVoiceComponent.generated.h"

class UAudioCapture;
class ACompanionGuide;
class FJsonObject;
class IVoiceCapture;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoiceListeningStateChanged, bool, bIsListening);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMicVolumeUpdated, float, NormalizedVolume);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLLMCommandExecuted, FName, ActionName, const FString&, ResponseMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLLMTranscriptReceived, const FString&, TranscriptText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoiceCommandRecognized, const FCompanionVoiceResult&, Result);

/**
 * High-performance Voice Command Component with pre-roll/post-roll buffering,
 * offline live mic speech recognition, strict JSON Gemini structured output,
 * WAV debug logging, and connection warm-up.
 */
UCLASS(ClassGroup = (Echoes), meta = (BlueprintSpawnableComponent))
class ECHOESOFTHECATACOMB_API UCompanionVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCompanionVoiceComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// -------------------------------------------------------------------------
	// Configuration
	// -------------------------------------------------------------------------

	/** Google Gemini API key. Loaded from Config/VoiceApiConfig.ini or GEMINI_API_KEY environment variable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|LLM", meta = (DisplayName = "Gemini API Key"))
	FString GeminiApiKey;

	/** Gemini Model name (e.g. gemini-flash-lite-latest, gemini-3.5-flash, gemini-3.8-flash) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|LLM")
	FString GeminiModel = TEXT("gemini-flash-lite-latest");

	/** Custom API Endpoint URL (empty uses Google official API) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|LLM")
	FString CustomEndpointUrl;

	/** Pre-roll duration captured continuously before Push-to-Talk is pressed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Audio", meta = (ClampMin = "0.05", ClampMax = "1.0", DisplayName = "Pre-Roll Duration (Seconds)"))
	float PreRollDuration = 0.30f;

	/** Post-roll tail captured after Push-to-Talk is released to prevent clipping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Audio", meta = (ClampMin = "0.05", ClampMax = "1.0", DisplayName = "Post-Roll Duration (Seconds)"))
	float PostRollDuration = 0.15f;

	/** Volume threshold below which audio is treated as ambient room noise */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Audio", meta = (ClampMin = "0.001", ClampMax = "0.5"))
	float NoiseGateThreshold = 0.015f;

	/** Maximum recording duration before auto-transmitting in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Audio", meta = (ClampMin = "1.0", ClampMax = "15.0"))
	float MaxRecordingDuration = 7.0f;

	// -------------------------------------------------------------------------
	// Audio Feedback (Cues)
	// -------------------------------------------------------------------------

	/** Sound played instantly when Push-to-Talk is released (Listening / Processing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Audio|Cues")
	TObjectPtr<USoundBase> ProcessingSoundCue;

	/** Sound played when a valid command is recognized and executed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Audio|Cues")
	TObjectPtr<USoundBase> CommandAcceptedSoundCue;

	/** Sound played when speech was unclear or returned UNKNOWN */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Audio|Cues")
	TObjectPtr<USoundBase> CommandUnknownSoundCue;

	// -------------------------------------------------------------------------
	// State
	// -------------------------------------------------------------------------

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Companion|State")
	bool bIsListening = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Companion|State")
	float CurrentMicLevel = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Companion|State")
	bool bIsProcessingLLM = false;

	// -------------------------------------------------------------------------
	// Delegates
	// -------------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Companion|Events")
	FOnVoiceListeningStateChanged OnListeningStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Events")
	FOnMicVolumeUpdated OnMicVolumeUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Events")
	FOnLLMCommandExecuted OnCommandExecuted;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Events")
	FOnLLMTranscriptReceived OnTranscriptReceived;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Events")
	FOnVoiceCommandRecognized OnCommandRecognized;

	// -------------------------------------------------------------------------
	// Public API
	// -------------------------------------------------------------------------

	/** Initializes audio capture device handle */
	UFUNCTION(BlueprintCallable, Category = "Companion|Voice")
	bool InitializeMicrophone();

	/** Starts capturing microphone audio (Push-to-Talk pressed) */
	UFUNCTION(BlueprintCallable, Category = "Companion|Voice")
	void StartListening();

	/** Stops capturing and transmits audio to recognizer/Gemini (Push-to-Talk released) */
	UFUNCTION(BlueprintCallable, Category = "Companion|Voice")
	void StopListening();

	/** Toggles voice listening on / off */
	UFUNCTION(BlueprintCallable, Category = "Companion|Voice")
	void ToggleListening();

	/** Fallback or chat-command input (executes verbal intents via text) */
	UFUNCTION(BlueprintCallable, Category = "Companion|Voice")
	void ProcessTextCommand(const FString& TextCommand);

	/** Sets the Gemini API key at runtime */
	UFUNCTION(BlueprintCallable, Category = "Companion|LLM")
	void SetGeminiApiKey(const FString& NewKey);

	/** Sets a custom offline local recognizer (pluggable architecture) */
	void SetLocalRecognizer(TScriptInterface<ICompanionLocalRecognizerInterface> InRecognizer);

	/** Opens Windows OS Microphone Privacy settings page */
	UFUNCTION(BlueprintCallable, Category = "Companion|Voice")
	void OpenWindowsMicrophoneSettings();

	/** Dispatches a parsed command enum directly to the companion orb (guaranteed on Game Thread) */
	void DispatchCommand(ECompanionVoiceCommand Command, float Confidence, const FString& RawTranscript, bool bLocalPath, float LatencyMs);

	/** Fires a lightweight HTTP request to warm up TCP/TLS sockets to Google's server */
	void WarmUpGeminiConnection();

	/** Runs the Companion.VoiceTest debug benchmark and prints table of attempts */
	void RunVoiceTest(bool bRunAutomatedSequence = true);

	/** Retrieves the last 10 recorded voice attempts */
	const TArray<FCompanionVoiceAttemptRecord>& GetAttemptHistory() const { return AttemptHistory; }

	/** Static helper to get the active voice component */
	static UCompanionVoiceComponent* GetActiveVoiceComponent();

private:
	static TWeakObjectPtr<UCompanionVoiceComponent> ActiveVoiceComponent;

	TSharedPtr<IVoiceCapture> VoiceCapture;

	UPROPERTY()
	TObjectPtr<UAudioCapture> AudioCapture;

	FAudioGeneratorHandle AudioCaptureHandle;

	FCriticalSection AudioBufferMutex;
	TArray<int16> RecordedPcmSamples;
	TArray<int16> PreRollRingBuffer;
	int32 PreRollCapacitySamples = 4800;

	int32 CaptureSampleRate = 16000;
	int32 CaptureChannels = 1;
	float RecordingTimer = 0.0f;
	bool bPostRollActive = false;
	float PostRollTimer = 0.0f;

	TArray<uint8> CachedLastWavBytes;
	int32 RetryAttemptCount = 0;
	double VoiceStartTimeSeconds = 0.0;

	UPROPERTY()
	TScriptInterface<ICompanionLocalRecognizerInterface> LocalRecognizer;

	TArray<FCompanionVoiceAttemptRecord> AttemptHistory;

	void DrainMicrophoneBuffer();
	TArray<uint8> BuildWavFile(const TArray<int16>& InPcm, int32 InSampleRate, int32 InChannels);
	TSharedPtr<FJsonObject> BuildGeminiPayload(const TArray<TSharedPtr<FJsonValue>>& InParts) const;
	void SendAudioToGemini(const TArray<uint8>& WavBytes);
	void OnGeminiResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
	void SaveDebugWavFile(const TArray<uint8>& WavBytes, const FString& Label);
	void RecordAttempt(const FString& Transcript, ECompanionVoiceCommand Command, bool bLocalFastPath, float LatencyMs, float Confidence);
	void PrintAttemptHistoryTable() const;

	ACompanionGuide* GetCompanionGuide() const;
};
