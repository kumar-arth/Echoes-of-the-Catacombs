// Copyright Epic Games, Inc. All Rights Reserved.

#include "CompanionVoiceComponent.h"
#include "CompanionGuide.h"
#include "CompanionKeywordRecognizer.h"
#include "AudioCapture.h"
#include "Voice.h"
#include "Interfaces/VoiceCapture.h"
#include "Misc/ConfigCacheIni.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/IConsoleManager.h"
#include "Async/Async.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/Engine.h"

TWeakObjectPtr<UCompanionVoiceComponent> UCompanionVoiceComponent::ActiveVoiceComponent = nullptr;

// Console variable to toggle voice debug logging and WAV dumps to Saved/VoiceDebug/
static TAutoConsoleVariable<int32> CVarCompanionVoiceDebug(
	TEXT("Companion.VoiceDebug"),
	1,
	TEXT("Controls companion voice diagnostics, latency metrics, and debug WAV dumps.\n")
	TEXT(" 0: Disabled\n")
	TEXT(" 1: Enabled (writes debug logs and Saves/VoiceDebug/*.wav files)"),
	ECVF_Cheat
);

// Console command: Companion.VoiceTest
static FAutoConsoleCommand GCompanionVoiceTestCmd(
	TEXT("Companion.VoiceTest"),
	TEXT("Executes a 10-attempt voice diagnostics test and prints a formatted table of transcript, command, path, and latency."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (UCompanionVoiceComponent* Comp = UCompanionVoiceComponent::GetActiveVoiceComponent())
		{
			const bool bRunAutomated = Args.Num() > 0 ? (Args[0].Equals(TEXT("run"), ESearchCase::IgnoreCase) || Args[0].Equals(TEXT("test"), ESearchCase::IgnoreCase)) : true;
			Comp->RunVoiceTest(bRunAutomated);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Companion.VoiceTest] No active UCompanionVoiceComponent found in scene."));
		}
	})
);

UCompanionVoiceComponent::UCompanionVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	PreRollDuration = 0.30f;
	PostRollDuration = 0.15f; // 150 ms tail
}

void UCompanionVoiceComponent::BeginPlay()
{
	Super::BeginPlay();

	ActiveVoiceComponent = this;

	// Load API key and Model from gitignored VoiceApiConfig.ini
	const FString ConfigPath = FPaths::ProjectConfigDir() / TEXT("VoiceApiConfig.ini");
	if (FPaths::FileExists(ConfigPath))
	{
		GConfig->GetString(TEXT("GeminiAPI"), TEXT("ApiKey"), GeminiApiKey, ConfigPath);
		GConfig->GetString(TEXT("GeminiAPI"), TEXT("Model"), GeminiModel, ConfigPath);
	}

	if (GeminiApiKey.IsEmpty() || GeminiApiKey.TrimStartAndEnd().IsEmpty())
	{
		GeminiApiKey = FPlatformMisc::GetEnvironmentVariable(TEXT("GEMINI_API_KEY"));
	}

	if (GeminiModel.IsEmpty() || GeminiModel.Contains(TEXT("gemini-2.0")) || GeminiModel.Contains(TEXT("gemini-1.5")) || GeminiModel.Contains(TEXT("gemini-2.5")))
	{
		GeminiModel = TEXT("gemini-flash-lite-latest");
	}

	// Initialize default local fast-path keyword/speech recognizer
	UCompanionKeywordRecognizer* DefaultRecognizer = NewObject<UCompanionKeywordRecognizer>(this);
	LocalRecognizer.SetObject(DefaultRecognizer);
	LocalRecognizer.SetInterface(Cast<ICompanionLocalRecognizerInterface>(DefaultRecognizer));

	InitializeMicrophone();

	// Warm up TCP/TLS connection to Gemini in background
	WarmUpGeminiConnection();
}

void UCompanionVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ActiveVoiceComponent.Get() == this)
	{
		ActiveVoiceComponent.Reset();
	}

	if (VoiceCapture.IsValid())
	{
		if (VoiceCapture->IsCapturing())
		{
			VoiceCapture->Stop();
		}
		VoiceCapture->Shutdown();
		VoiceCapture.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

UCompanionVoiceComponent* UCompanionVoiceComponent::GetActiveVoiceComponent()
{
	return ActiveVoiceComponent.Get();
}

void UCompanionVoiceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Continually drain mic buffer into rolling pre-roll buffer
	DrainMicrophoneBuffer();

	if (bIsListening)
	{
		RecordingTimer += DeltaTime;
		if (RecordingTimer >= MaxRecordingDuration)
		{
			StopListening();
		}
	}
	else if (bPostRollActive)
	{
		PostRollTimer += DeltaTime;
		if (PostRollTimer >= PostRollDuration)
		{
			bPostRollActive = false;

			// Finished capturing post-roll tail (150ms), now process audio
			TArray<int16> SamplesToProcess;
			{
				FScopeLock Lock(&AudioBufferMutex);
				SamplesToProcess = MoveTemp(RecordedPcmSamples);
			}

			const int32 MinSamplesNeeded = FMath::Max(1, (int32)(CaptureSampleRate * CaptureChannels * 0.10f));
			if (SamplesToProcess.Num() >= MinSamplesNeeded)
			{
				// 1. Try local offline speech recognizer on live PCM audio first!
				if (LocalRecognizer.GetInterface())
				{
					FCompanionVoiceResult LocalResult;
					if (LocalRecognizer->RecognizeAudio(SamplesToProcess, CaptureSampleRate, CaptureChannels, LocalResult))
					{
						const float LocalLatency = (float)((FPlatformTime::Seconds() - VoiceStartTimeSeconds) * 1000.0);
						DispatchCommand(LocalResult.Command, LocalResult.Confidence, LocalResult.RawTranscript, true, LocalLatency);
						return;
					}
				}

				// 2. Fall back to Gemini Cloud for complex / conversational / negated utterances
				const float AudioDurationSec = (float)SamplesToProcess.Num() / (float)(CaptureSampleRate * CaptureChannels);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(500, 2.0f, FColor::Cyan, FString::Printf(TEXT("[Companion]: [PROCESSING] Routing to Gemini Cloud (%.2fs)..."), AudioDurationSec));
				}

				TArray<uint8> WavData = BuildWavFile(SamplesToProcess, CaptureSampleRate, CaptureChannels);
				if (CVarCompanionVoiceDebug.GetValueOnGameThread() > 0)
				{
					SaveDebugWavFile(WavData, TEXT("Captured"));
				}

				SendAudioToGemini(WavData);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[CompanionVoice] Recording too brief (%d samples)."), SamplesToProcess.Num());
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(500, 3.0f, FColor::Yellow, TEXT("[Companion]: [WARN] Voice too brief or mic quiet. Hold 'V' while speaking."));
				}
				DispatchCommand(ECompanionVoiceCommand::UNKNOWN, 0.0f, TEXT("Too short"), false, 0.0f);
			}
		}
	}
}

void UCompanionVoiceComponent::DrainMicrophoneBuffer()
{
	if (!VoiceCapture.IsValid() || !VoiceCapture->IsCapturing())
	{
		return;
	}

	uint32 AvailableVoiceData = 0;
	VoiceCapture->GetCaptureState(AvailableVoiceData);

	if (AvailableVoiceData > 0)
	{
		TArray<uint8> VoiceBuffer;
		VoiceBuffer.SetNumUninitialized(AvailableVoiceData);
		uint32 ReadBytes = 0;
		VoiceCapture->GetVoiceData(VoiceBuffer.GetData(), AvailableVoiceData, ReadBytes);

		if (ReadBytes > 0)
		{
			const int16* PcmPtr = reinterpret_cast<const int16*>(VoiceBuffer.GetData());
			const int32 NumSamples = ReadBytes / sizeof(int16);

			// Calculate RMS audio energy
			float SumSq = 0.0f;
			for (int32 i = 0; i < NumSamples; ++i)
			{
				const float Norm = (float)PcmPtr[i] / 32768.0f;
				SumSq += Norm * Norm;
			}
			const float RMS = FMath::Sqrt(SumSq / (float)NumSamples);
			CurrentMicLevel = FMath::Clamp(RMS * 8.0f, 0.0f, 1.0f);
			OnMicVolumeUpdated.Broadcast(CurrentMicLevel);

			FScopeLock Lock(&AudioBufferMutex);

			if (bIsListening || bPostRollActive)
			{
				RecordedPcmSamples.Append(PcmPtr, NumSamples);
			}
			else
			{
				// Maintain rolling pre-roll buffer
				PreRollRingBuffer.Append(PcmPtr, NumSamples);
				if (PreRollRingBuffer.Num() > PreRollCapacitySamples)
				{
					const int32 Excess = PreRollRingBuffer.Num() - PreRollCapacitySamples;
					PreRollRingBuffer.RemoveAt(0, Excess, EAllowShrinking::No);
				}
			}
		}
	}
}

bool UCompanionVoiceComponent::InitializeMicrophone()
{
	if (VoiceCapture.IsValid())
	{
		return true;
	}

	// 1. Ensure Voice module config is enabled in Engine ini
	bool bVoiceEnabled = false;
	if (!GConfig->GetBool(TEXT("Voice"), TEXT("bEnabled"), bVoiceEnabled, GEngineIni) || !bVoiceEnabled)
	{
		GConfig->SetBool(TEXT("Voice"), TEXT("bEnabled"), true, GEngineIni);
		GConfig->Flush(false, GEngineIni);
	}

	if (FVoiceModule::IsAvailable() && !FVoiceModule::Get().IsVoiceEnabled())
	{
		IModuleInterface& VoiceMod = static_cast<IModuleInterface&>(FVoiceModule::Get());
		VoiceMod.ShutdownModule();
		VoiceMod.StartupModule();
	}

	// Format A: 16000 Hz, 1 channel (optimal for speech AI / Gemini)
	CaptureSampleRate = 16000;
	CaptureChannels = 1;
	PreRollCapacitySamples = FMath::Max(1, (int32)(CaptureSampleRate * CaptureChannels * PreRollDuration));
	VoiceCapture = FVoiceModule::Get().CreateVoiceCapture(TEXT(""), CaptureSampleRate, CaptureChannels);
	if (VoiceCapture.IsValid())
	{
		VoiceCapture->Start(); // Keep continuous capture running for pre-roll buffer
		UE_LOG(LogTemp, Log, TEXT("[CompanionVoice] Initialized IVoiceCapture mic (%d Hz, %d ch, PreRoll: %d samples)"), CaptureSampleRate, CaptureChannels, PreRollCapacitySamples);
		return true;
	}

	// Format B: 48000 Hz, 2 channels (hardware fallback)
	CaptureSampleRate = 48000;
	CaptureChannels = 2;
	PreRollCapacitySamples = FMath::Max(1, (int32)(CaptureSampleRate * CaptureChannels * PreRollDuration));
	VoiceCapture = FVoiceModule::Get().CreateVoiceCapture(TEXT(""), CaptureSampleRate, CaptureChannels);
	if (VoiceCapture.IsValid())
	{
		VoiceCapture->Start();
		UE_LOG(LogTemp, Log, TEXT("[CompanionVoice] Initialized IVoiceCapture mic with native hardware format (%d Hz, %d ch)"), CaptureSampleRate, CaptureChannels);
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[CompanionVoice] Failed to create IVoiceCapture instance."));
	return false;
}

void UCompanionVoiceComponent::StartListening()
{
	if (bIsListening)
	{
		return;
	}

	RetryAttemptCount = 0;
	VoiceStartTimeSeconds = FPlatformTime::Seconds();

	if (!VoiceCapture.IsValid())
	{
		InitializeMicrophone();
	}

	if (!VoiceCapture.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[CompanionVoice] Cannot start listening: No microphone capture interface available."));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(500, 6.0f, FColor::Red, TEXT("[Companion]: [ERROR] Mic device unavailable. Opening Windows Settings..."));
		}
		OpenWindowsMicrophoneSettings();
		return;
	}

	// Seed recording buffer with the pre-roll samples
	{
		FScopeLock Lock(&AudioBufferMutex);
		RecordedPcmSamples.Reset();
		if (PreRollRingBuffer.Num() > 0)
		{
			RecordedPcmSamples.Append(PreRollRingBuffer);
			PreRollRingBuffer.Reset();
		}
	}

	RecordingTimer = 0.0f;
	bIsListening = true;
	bPostRollActive = false;

	if (!VoiceCapture->IsCapturing())
	{
		VoiceCapture->Start();
	}

	OnListeningStateChanged.Broadcast(true);

	if (ACompanionGuide* Guide = GetCompanionGuide())
	{
		Guide->SetTemporaryLightColor(FLinearColor(0.9f, 0.4f, 1.0f), 999.0f); // Magenta listening indicator
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(500, 4.0f, FLinearColor(0.9f, 0.4f, 1.0f).ToFColor(true), TEXT("[Companion]: [MIC] Listening... (Hold 'V' to speak, release to send)"));
	}
}

void UCompanionVoiceComponent::StopListening()
{
	if (!bIsListening)
	{
		return;
	}

	bIsListening = false;
	OnListeningStateChanged.Broadcast(false);

	// Instant user feedback on release: play processing cue and flicker light
	if (ProcessingSoundCue)
	{
		UGameplayStatics::PlaySound2D(this, ProcessingSoundCue);
	}

	if (ACompanionGuide* Guide = GetCompanionGuide())
	{
		Guide->SetTemporaryLightColor(FLinearColor(0.2f, 0.8f, 1.0f), 0.35f); // Cyan flicker
	}

	// Begin post-roll tail capture (150ms) before finalizing buffer
	bPostRollActive = true;
	PostRollTimer = 0.0f;
}

void UCompanionVoiceComponent::ToggleListening()
{
	if (bIsListening)
	{
		StopListening();
	}
	else
	{
		StartListening();
	}
}

TArray<uint8> UCompanionVoiceComponent::BuildWavFile(const TArray<int16>& InPcm, int32 InSampleRate, int32 InChannels)
{
	// Convert stereo to mono
	TArray<int16> MonoPcm;
	const int16* FinalPcmPtr = InPcm.GetData();
	int32 FinalSampleCount = InPcm.Num();
	int32 FinalChannels = InChannels;

	if (InChannels == 2 && InPcm.Num() >= 2)
	{
		const int32 NumFrames = InPcm.Num() / 2;
		MonoPcm.SetNumUninitialized(NumFrames);
		for (int32 i = 0; i < NumFrames; ++i)
		{
			const int32 Sum = (int32)InPcm[i * 2] + (int32)InPcm[i * 2 + 1];
			MonoPcm[i] = (int16)(Sum / 2);
		}
		FinalPcmPtr = MonoPcm.GetData();
		FinalSampleCount = NumFrames;
		FinalChannels = 1;
	}

	// Downsample 48000 Hz down to 16000 Hz (3:1 average)
	TArray<int16> Resampled16k;
	int32 OutSampleRate = InSampleRate;
	if (FinalChannels == 1 && InSampleRate == 48000 && FinalSampleCount >= 3)
	{
		const int32 Num16k = FinalSampleCount / 3;
		Resampled16k.SetNumUninitialized(Num16k);
		for (int32 i = 0; i < Num16k; ++i)
		{
			const int32 Sum = (int32)FinalPcmPtr[i * 3] + (int32)FinalPcmPtr[i * 3 + 1] + (int32)FinalPcmPtr[i * 3 + 2];
			Resampled16k[i] = (int16)(Sum / 3);
		}
		FinalPcmPtr = Resampled16k.GetData();
		FinalSampleCount = Num16k;
		OutSampleRate = 16000;
	}

	TArray<uint8> WavBytes;
	const int32 DataSize = FinalSampleCount * sizeof(int16);
	const int32 ChunkSize = 36 + DataSize;
	const int32 ByteRate = OutSampleRate * FinalChannels * sizeof(int16);
	const int16 BlockAlign = FinalChannels * sizeof(int16);
	const int16 BitsPerSample = 16;

	WavBytes.SetNumUninitialized(44 + DataSize);
	uint8* Ptr = WavBytes.GetData();

	// RIFF header
	FMemory::Memcpy(Ptr + 0, "RIFF", 4);
	FMemory::Memcpy(Ptr + 4, &ChunkSize, 4);
	FMemory::Memcpy(Ptr + 8, "WAVE", 4);

	// fmt chunk
	FMemory::Memcpy(Ptr + 12, "fmt ", 4);
	const int32 Subchunk1Size = 16;
	const int16 AudioFormat = 1; // PCM
	FMemory::Memcpy(Ptr + 16, &Subchunk1Size, 4);
	FMemory::Memcpy(Ptr + 20, &AudioFormat, 2);
	const int16 Channels16 = (int16)FinalChannels;
	FMemory::Memcpy(Ptr + 22, &Channels16, 2);
	FMemory::Memcpy(Ptr + 24, &OutSampleRate, 4);
	FMemory::Memcpy(Ptr + 28, &ByteRate, 4);
	FMemory::Memcpy(Ptr + 32, &BlockAlign, 2);
	FMemory::Memcpy(Ptr + 34, &BitsPerSample, 2);

	// data chunk
	FMemory::Memcpy(Ptr + 36, "data", 4);
	FMemory::Memcpy(Ptr + 40, &DataSize, 4);
	FMemory::Memcpy(Ptr + 44, FinalPcmPtr, DataSize);

	return WavBytes;
}

void UCompanionVoiceComponent::SaveDebugWavFile(const TArray<uint8>& WavBytes, const FString& Label)
{
	const FString DebugDir = FPaths::ProjectSavedDir() / TEXT("VoiceDebug");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*DebugDir))
	{
		PlatformFile.CreateDirectoryTree(*DebugDir);
	}

	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString FilePath = DebugDir / FString::Printf(TEXT("Voice_%s_%s.wav"), *Label, *Timestamp);
	FFileHelper::SaveArrayToFile(WavBytes, *FilePath);
	UE_LOG(LogTemp, Log, TEXT("[CompanionVoice] Saved debug WAV to: %s (%d bytes)"), *FilePath, WavBytes.Num());
}

TSharedPtr<FJsonObject> UCompanionVoiceComponent::BuildGeminiPayload(const TArray<TSharedPtr<FJsonValue>>& InParts) const
{
	TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();

	// Strict system prompt with response examples and UNKNOWN handling
	TSharedPtr<FJsonObject> SystemInstruction = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> SysParts;
	TSharedPtr<FJsonObject> SysTextPart = MakeShared<FJsonObject>();
	SysTextPart->SetStringField(TEXT("text"),
		TEXT("You are a companion voice command classifier in 'Echoes of the Catacombs'. ")
		TEXT("Output ONLY strict JSON with format: {\"command\": \"...\", \"confidence\": float}. ")
		TEXT("Allowed commands: UNKNOWN, FOLLOW, STAY, RETREAT, ATTACK, PULSE, EXIT, SCOUT, TRAPS, DANGER, BOSS, UPGRADE, DISTRACT. ")
		TEXT("If speech is unclear, conversational, or not a game command, return command UNKNOWN and confidence 0.0.\n")
		TEXT("Examples:\n")
		TEXT("- 'stay', 'stay here', 'hold', 'stop', 'wait' -> {\"command\": \"STAY\", \"confidence\": 1.0}\n")
		TEXT("- 'follow', 'to me', 'heel', 'come with me', 'let's go' -> {\"command\": \"FOLLOW\", \"confidence\": 1.0}\n")
		TEXT("- 'run', 'retreat', 'fall back', 'safe distance' -> {\"command\": \"RETREAT\", \"confidence\": 1.0}\n")
		TEXT("- 'attack', 'concuss', 'stun', 'blast' -> {\"command\": \"ATTACK\", \"confidence\": 1.0}\n")
		TEXT("- 'pulse', 'echo', 'ping', 'sonar' -> {\"command\": \"PULSE\", \"confidence\": 1.0}\n")
		TEXT("- 'exit', 'way out', 'escape' -> {\"command\": \"EXIT\", \"confidence\": 1.0}\n")
		TEXT("- 'traps', 'spikes', 'scan traps' -> {\"command\": \"TRAPS\", \"confidence\": 1.0}\n")
		TEXT("- 'danger', 'threat', 'is it safe' -> {\"command\": \"DANGER\", \"confidence\": 1.0}\n")
		TEXT("- 'boss', 'boss hp', 'check boss' -> {\"command\": \"BOSS\", \"confidence\": 1.0}\n")
		TEXT("- 'upgrade', 'forge', 'enhance' -> {\"command\": \"UPGRADE\", \"confidence\": 1.0}\n")
		TEXT("- 'distract', 'decoy', 'lure' -> {\"command\": \"DISTRACT\", \"confidence\": 1.0}"));
	SysParts.Add(MakeShared<FJsonValueObject>(SysTextPart));
	SystemInstruction->SetArrayField(TEXT("parts"), SysParts);
	RootObject->SetObjectField(TEXT("system_instruction"), SystemInstruction);

	// Contents
	TArray<TSharedPtr<FJsonValue>> ContentsArray;
	TSharedPtr<FJsonObject> UserContent = MakeShared<FJsonObject>();
	UserContent->SetStringField(TEXT("role"), TEXT("user"));
	UserContent->SetArrayField(TEXT("parts"), InParts);
	ContentsArray.Add(MakeShared<FJsonValueObject>(UserContent));
	RootObject->SetArrayField(TEXT("contents"), ContentsArray);

	// Fast generation config (temperature 0, structured JSON output, low token limit)
	TSharedPtr<FJsonObject> GenConfig = MakeShared<FJsonObject>();
	GenConfig->SetNumberField(TEXT("temperature"), 0.0);
	GenConfig->SetStringField(TEXT("response_mime_type"), TEXT("application/json"));
	GenConfig->SetNumberField(TEXT("maxOutputTokens"), 64);
	RootObject->SetObjectField(TEXT("generationConfig"), GenConfig);

	return RootObject;
}

void UCompanionVoiceComponent::SendAudioToGemini(const TArray<uint8>& WavBytes)
{
	if (WavBytes.Num() == 0)
	{
		return;
	}

	bIsProcessingLLM = true;
	CachedLastWavBytes = WavBytes;

	FString ActiveKey = GeminiApiKey;
	if (ActiveKey.IsEmpty())
	{
		ActiveKey = FPlatformMisc::GetEnvironmentVariable(TEXT("GEMINI_API_KEY"));
	}

	if (ActiveKey.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CompanionVoice] No Gemini API key configured in VoiceApiConfig.ini or GEMINI_API_KEY env var."));
		DispatchCommand(ECompanionVoiceCommand::UNKNOWN, 0.0f, TEXT("Missing API Key"), false, 0.0f);
		return;
	}

	const FString Base64Audio = FBase64::Encode(WavBytes);

	TArray<TSharedPtr<FJsonValue>> ContentParts;
	TSharedPtr<FJsonObject> AudioPart = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> InlineData = MakeShared<FJsonObject>();
	InlineData->SetStringField(TEXT("mime_type"), TEXT("audio/wav"));
	InlineData->SetStringField(TEXT("data"), Base64Audio);
	AudioPart->SetObjectField(TEXT("inline_data"), InlineData);
	ContentParts.Add(MakeShared<FJsonValueObject>(AudioPart));

	TSharedPtr<FJsonObject> RootObject = BuildGeminiPayload(ContentParts);

	FString RequestBodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	FString Url = CustomEndpointUrl;
	if (Url.IsEmpty())
	{
		Url = FString::Printf(TEXT("https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s"),
			*GeminiModel, *ActiveKey);
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBodyString);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UCompanionVoiceComponent::OnGeminiResponseReceived);
	HttpRequest->ProcessRequest();
}

void UCompanionVoiceComponent::OnGeminiResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	bIsProcessingLLM = false;
	const float LatencyMs = (float)((FPlatformTime::Seconds() - VoiceStartTimeSeconds) * 1000.0);

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CompanionVoice] HTTP request to Gemini failed. Latency: %.1f ms"), LatencyMs);
		AsyncTask(ENamedThreads::GameThread, [this, LatencyMs]()
		{
			DispatchCommand(ECompanionVoiceCommand::UNKNOWN, 0.0f, TEXT("Connection failed"), false, LatencyMs);
		});
		return;
	}

	const int32 StatusCode = Response->GetResponseCode();
	const FString ResponseString = Response->GetContentAsString();

	if (StatusCode != 200)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CompanionVoice] Gemini returned status %d."), StatusCode);

		// Handle transient server load spikes (503 / 429)
		if ((StatusCode == 503 || StatusCode == 429) && RetryAttemptCount < 2 && CachedLastWavBytes.Num() > 0)
		{
			RetryAttemptCount++;
			if (GeminiModel == TEXT("gemini-flash-lite-latest"))
			{
				GeminiModel = TEXT("gemini-3.5-flash");
			}
			else if (GeminiModel == TEXT("gemini-3.5-flash"))
			{
				GeminiModel = TEXT("gemini-3.8-flash");
			}
			else
			{
				GeminiModel = TEXT("gemini-flash-lite-latest");
			}

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(501, 3.0f, FColor::Yellow, FString::Printf(TEXT("[Companion]: [RETRY] Load spike (%d). Retrying with %s..."), StatusCode, *GeminiModel));
			}
			UE_LOG(LogTemp, Log, TEXT("[CompanionVoice] Retrying with model %s (Attempt %d)."), *GeminiModel, RetryAttemptCount);
			SendAudioToGemini(CachedLastWavBytes);
			return;
		}

		AsyncTask(ENamedThreads::GameThread, [this, StatusCode, LatencyMs]()
		{
			DispatchCommand(ECompanionVoiceCommand::UNKNOWN, 0.0f, FString::Printf(TEXT("HTTP %d"), StatusCode), false, LatencyMs);
		});
		return;
	}

	RetryAttemptCount = 0;

	// Parse Response JSON
	TSharedPtr<FJsonObject> JsonResponse;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);
	if (!FJsonSerializer::Deserialize(Reader, JsonResponse) || !JsonResponse.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CompanionVoice] Failed to deserialize Gemini outer JSON."));
		AsyncTask(ENamedThreads::GameThread, [this, LatencyMs]()
		{
			DispatchCommand(ECompanionVoiceCommand::UNKNOWN, 0.0f, TEXT("Parse error"), false, LatencyMs);
		});
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* CandidatesArray;
	if (!JsonResponse->TryGetArrayField(TEXT("candidates"), CandidatesArray) || CandidatesArray->Num() == 0)
	{
		AsyncTask(ENamedThreads::GameThread, [this, LatencyMs]()
		{
			DispatchCommand(ECompanionVoiceCommand::UNKNOWN, 0.0f, TEXT("No candidate"), false, LatencyMs);
		});
		return;
	}

	TSharedPtr<FJsonObject> FirstCandidate = (*CandidatesArray)[0]->AsObject();
	TSharedPtr<FJsonObject> ContentObj = FirstCandidate->GetObjectField(TEXT("content"));
	const TArray<TSharedPtr<FJsonValue>>* PartsArray;
	if (!ContentObj->TryGetArrayField(TEXT("parts"), PartsArray))
	{
		AsyncTask(ENamedThreads::GameThread, [this, LatencyMs]()
		{
			DispatchCommand(ECompanionVoiceCommand::UNKNOWN, 0.0f, TEXT("No parts"), false, LatencyMs);
		});
		return;
	}

	FString JsonRawText;
	for (const TSharedPtr<FJsonValue>& PartVal : *PartsArray)
	{
		TSharedPtr<FJsonObject> PartObj = PartVal->AsObject();
		if (PartObj->HasField(TEXT("text")))
		{
			JsonRawText += PartObj->GetStringField(TEXT("text"));
		}
	}

	// Parse inner strict JSON { "command": "...", "confidence": 1.0 }
	TSharedPtr<FJsonObject> InnerParsed;
	TSharedRef<TJsonReader<>> InnerReader = TJsonReaderFactory<>::Create(JsonRawText);
	FString CommandStr;
	float Confidence = 0.0f;

	if (FJsonSerializer::Deserialize(InnerReader, InnerParsed) && InnerParsed.IsValid())
	{
		CommandStr = InnerParsed->GetStringField(TEXT("command")).ToUpper();
		if (InnerParsed->HasField(TEXT("confidence")))
		{
			Confidence = (float)InnerParsed->GetNumberField(TEXT("confidence"));
		}
	}

	ECompanionVoiceCommand RecognizedCommand = ECompanionVoiceCommand::UNKNOWN;
	if (CommandStr.Equals(TEXT("STAY")))         RecognizedCommand = ECompanionVoiceCommand::STAY;
	else if (CommandStr.Equals(TEXT("FOLLOW")))   RecognizedCommand = ECompanionVoiceCommand::FOLLOW;
	else if (CommandStr.Equals(TEXT("RETREAT")))  RecognizedCommand = ECompanionVoiceCommand::RETREAT;
	else if (CommandStr.Equals(TEXT("ATTACK")))   RecognizedCommand = ECompanionVoiceCommand::ATTACK;
	else if (CommandStr.Equals(TEXT("PULSE")))    RecognizedCommand = ECompanionVoiceCommand::PULSE;
	else if (CommandStr.Equals(TEXT("EXIT")))     RecognizedCommand = ECompanionVoiceCommand::EXIT;
	else if (CommandStr.Equals(TEXT("SCOUT")))    RecognizedCommand = ECompanionVoiceCommand::SCOUT;
	else if (CommandStr.Equals(TEXT("TRAPS")))    RecognizedCommand = ECompanionVoiceCommand::TRAPS;
	else if (CommandStr.Equals(TEXT("DANGER")))   RecognizedCommand = ECompanionVoiceCommand::DANGER;
	else if (CommandStr.Equals(TEXT("BOSS")))     RecognizedCommand = ECompanionVoiceCommand::BOSS;
	else if (CommandStr.Equals(TEXT("UPGRADE")))  RecognizedCommand = ECompanionVoiceCommand::UPGRADE;
	else if (CommandStr.Equals(TEXT("DISTRACT"))) RecognizedCommand = ECompanionVoiceCommand::DISTRACT;

	// Item 3: Require higher confidence (>= 0.85f) for ATTACK and RETREAT than for FOLLOW and STAY (>= 0.70f)
	const float MinThreshold = (RecognizedCommand == ECompanionVoiceCommand::ATTACK || RecognizedCommand == ECompanionVoiceCommand::RETREAT) ? 0.85f : 0.70f;
	if (Confidence < MinThreshold)
	{
		RecognizedCommand = ECompanionVoiceCommand::UNKNOWN;
	}

	// Dispatch result strictly on the Game Thread
	AsyncTask(ENamedThreads::GameThread, [this, RecognizedCommand, Confidence, JsonRawText, LatencyMs]()
	{
		DispatchCommand(RecognizedCommand, Confidence, JsonRawText.TrimStartAndEnd(), false, LatencyMs);
	});
}

void UCompanionVoiceComponent::ProcessTextCommand(const FString& TextCommand)
{
	const double StartTime = FPlatformTime::Seconds();
	OnTranscriptReceived.Broadcast(TextCommand);

	// Try local fast-path keyword recognizer first for instant <1ms offline execution
	if (LocalRecognizer.GetInterface())
	{
		FCompanionVoiceResult LocalResult;
		if (LocalRecognizer->RecognizeCommand(TextCommand, LocalResult))
		{
			const float LocalLatency = (float)((FPlatformTime::Seconds() - StartTime) * 1000.0);
			DispatchCommand(LocalResult.Command, LocalResult.Confidence, TextCommand, true, LocalLatency);
			return;
		}
	}

	// Ambiguous text or negation present: route to Gemini JSON classifier
	VoiceStartTimeSeconds = StartTime;
	FString ActiveKey = GeminiApiKey;
	if (ActiveKey.IsEmpty())
	{
		ActiveKey = FPlatformMisc::GetEnvironmentVariable(TEXT("GEMINI_API_KEY"));
	}

	if (ActiveKey.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CompanionVoice] No Gemini API key configured."));
		DispatchCommand(ECompanionVoiceCommand::UNKNOWN, 0.0f, TextCommand, false, 0.0f);
		return;
	}

	TArray<TSharedPtr<FJsonValue>> ContentParts;
	TSharedPtr<FJsonObject> TextPart = MakeShared<FJsonObject>();
	TextPart->SetStringField(TEXT("text"), TextCommand);
	ContentParts.Add(MakeShared<FJsonValueObject>(TextPart));

	TSharedPtr<FJsonObject> RootObject = BuildGeminiPayload(ContentParts);

	FString RequestBodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	const FString Url = FString::Printf(TEXT("https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s"),
		*GeminiModel, *ActiveKey);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBodyString);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UCompanionVoiceComponent::OnGeminiResponseReceived);
	HttpRequest->ProcessRequest();
}

void UCompanionVoiceComponent::RecordAttempt(const FString& Transcript, ECompanionVoiceCommand Command, bool bLocalFastPath, float LatencyMs, float Confidence)
{
	FCompanionVoiceAttemptRecord Record;
	Record.AttemptIndex = AttemptHistory.Num() + 1;
	Record.Transcript = Transcript;
	Record.Command = Command;
	Record.bIsLocalFastPath = bLocalFastPath;
	Record.LatencyMs = LatencyMs;
	Record.Confidence = Confidence;

	AttemptHistory.Add(Record);
	if (AttemptHistory.Num() > 10)
	{
		AttemptHistory.RemoveAt(0, AttemptHistory.Num() - 10, EAllowShrinking::No);
	}
}

void UCompanionVoiceComponent::PrintAttemptHistoryTable() const
{
	UE_LOG(LogTemp, Display, TEXT("========================================================================================="));
	UE_LOG(LogTemp, Display, TEXT("| Att | Transcript           | Command    | Path            | Latency (ms) | Conf   |"));
	UE_LOG(LogTemp, Display, TEXT("+-----+----------------------+------------+-----------------+--------------+--------+"));

	for (int32 i = 0; i < AttemptHistory.Num(); ++i)
	{
		const FCompanionVoiceAttemptRecord& Rec = AttemptHistory[i];
		const TCHAR* CmdName = TEXT("UNKNOWN");
		switch (Rec.Command)
		{
		case ECompanionVoiceCommand::FOLLOW:   CmdName = TEXT("FOLLOW"); break;
		case ECompanionVoiceCommand::STAY:     CmdName = TEXT("STAY"); break;
		case ECompanionVoiceCommand::RETREAT:  CmdName = TEXT("RETREAT"); break;
		case ECompanionVoiceCommand::ATTACK:   CmdName = TEXT("ATTACK"); break;
		case ECompanionVoiceCommand::PULSE:    CmdName = TEXT("PULSE"); break;
		case ECompanionVoiceCommand::EXIT:     CmdName = TEXT("EXIT"); break;
		case ECompanionVoiceCommand::SCOUT:    CmdName = TEXT("SCOUT"); break;
		case ECompanionVoiceCommand::TRAPS:    CmdName = TEXT("TRAPS"); break;
		case ECompanionVoiceCommand::DANGER:   CmdName = TEXT("DANGER"); break;
		case ECompanionVoiceCommand::BOSS:     CmdName = TEXT("BOSS"); break;
		case ECompanionVoiceCommand::UPGRADE:  CmdName = TEXT("UPGRADE"); break;
		case ECompanionVoiceCommand::DISTRACT: CmdName = TEXT("DISTRACT"); break;
		default: break;
		}

		const TCHAR* PathName = Rec.bIsLocalFastPath ? TEXT("LOCAL_FAST_PATH") : TEXT("GEMINI_CLOUD");
		FString TruncTranscript = Rec.Transcript.Replace(TEXT("\n"), TEXT(" "));
		if (TruncTranscript.Len() > 20)
		{
			TruncTranscript = TruncTranscript.Left(17) + TEXT("...");
		}

		UE_LOG(LogTemp, Display, TEXT("| %3d | %-20s | %-10s | %-15s | %10.1fms | %6.2f |"),
			i + 1, *TruncTranscript, CmdName, PathName, Rec.LatencyMs, Rec.Confidence);
	}
	UE_LOG(LogTemp, Display, TEXT("========================================================================================="));
}

void UCompanionVoiceComponent::RunVoiceTest(bool bRunAutomatedSequence)
{
	if (bRunAutomatedSequence)
	{
		static const TArray<FString> TestPhrases = {
			TEXT("stay"),
			TEXT("wait here"),
			TEXT("follow me"),
			TEXT("to me"),
			TEXT("retreat"),
			TEXT("run"),
			TEXT("attack"),
			TEXT("pulse"),
			TEXT("exit"),
			TEXT("don't follow") // Negation word test -> must route to Gemini cloud
		};

		UE_LOG(LogTemp, Display, TEXT("[Companion.VoiceTest] Executing 10-attempt automated voice diagnostics test..."));

		for (int32 i = 0; i < TestPhrases.Num(); ++i)
		{
			ProcessTextCommand(TestPhrases[i]);
		}
	}

	PrintAttemptHistoryTable();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(510, 8.0f, FColor::Cyan,
			FString::Printf(TEXT("[Companion.VoiceTest]: Displayed table of %d attempts in Output Log."), AttemptHistory.Num()));
	}
}

void UCompanionVoiceComponent::DispatchCommand(ECompanionVoiceCommand Command, float Confidence, const FString& RawTranscript, bool bLocalPath, float LatencyMs)
{
	// Guarantee game thread execution
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [this, Command, Confidence, RawTranscript, bLocalPath, LatencyMs]()
		{
			DispatchCommand(Command, Confidence, RawTranscript, bLocalPath, LatencyMs);
		});
		return;
	}

	RecordAttempt(RawTranscript, Command, bLocalPath, LatencyMs, Confidence);

	FCompanionVoiceResult Result;
	Result.Command = Command;
	Result.Confidence = Confidence;
	Result.RawTranscript = RawTranscript;
	Result.bIsLocalFastPath = bLocalPath;
	Result.LatencyMs = LatencyMs;

	// Broadcast structured event delegate for Blueprint/Blackboard listeners
	OnCommandRecognized.Broadcast(Result);

	ACompanionGuide* Guide = GetCompanionGuide();

	// Log metrics (Item 1: Log LOCAL_FAST_PATH vs GEMINI_CLOUD)
	const FString PathStr = bLocalPath ? TEXT("LOCAL_FAST_PATH") : TEXT("GEMINI_CLOUD");
	UE_LOG(LogTemp, Log, TEXT("[CompanionVoice][%s] Command: %d | Confidence: %.2f | Latency: %.1f ms | Raw: %s"),
		*PathStr, (int32)Command, Confidence, LatencyMs, *RawTranscript);

	if (CVarCompanionVoiceDebug.GetValueOnGameThread() > 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(509, 4.0f, FColor::Silver,
			FString::Printf(TEXT("[VoiceStat]: %s | Latency: %.1fms | Conf: %.2f"), *PathStr, LatencyMs, Confidence));
	}

	// 1. Handle UNKNOWN / Parse Failure
	if (Command == ECompanionVoiceCommand::UNKNOWN)
	{
		if (CommandUnknownSoundCue)
		{
			UGameplayStatics::PlaySound2D(this, CommandUnknownSoundCue);
		}
		if (Guide)
		{
			Guide->SetTemporaryLightColor(FLinearColor(1.0f, 0.4f, 0.1f), 0.8f); // Amber confused cue
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(502, 3.5f, FColor::Yellow, TEXT("[Companion]: [?] Didn't catch that command."));
		}
		return;
	}

	// 2. Play Accepted Sound Cue on valid recognition
	if (CommandAcceptedSoundCue)
	{
		UGameplayStatics::PlaySound2D(this, CommandAcceptedSoundCue);
	}

	if (!Guide)
	{
		return;
	}

	FString ResponseMsg;

	switch (Command)
	{
	case ECompanionVoiceCommand::STAY:
		Guide->SetAnchorMode(true);
		ResponseMsg = TEXT("Holding position here.");
		if (GEngine) GEngine->AddOnScreenDebugMessage(502, 4.0f, FColor::Yellow, FString::Printf(TEXT("[Companion]: [STAY] %s"), *ResponseMsg));
		break;

	case ECompanionVoiceCommand::FOLLOW:
		Guide->SetAnchorMode(false);
		ResponseMsg = TEXT("Following close behind you!");
		if (GEngine) GEngine->AddOnScreenDebugMessage(502, 4.0f, FColor::Green, FString::Printf(TEXT("[Companion]: [FOLLOW] %s"), *ResponseMsg));
		break;

	case ECompanionVoiceCommand::RETREAT:
		Guide->RetreatToSafeDistance(ResponseMsg);
		if (GEngine) GEngine->AddOnScreenDebugMessage(502, 4.0f, FColor::Cyan, FString::Printf(TEXT("[Companion]: [RETREAT] %s"), *ResponseMsg));
		break;

	case ECompanionVoiceCommand::ATTACK:
		Guide->ConcussTarget(nullptr, ResponseMsg);
		if (GEngine) GEngine->AddOnScreenDebugMessage(502, 4.0f, FColor::Green, FString::Printf(TEXT("[Companion]: [ATTACK] %s"), *ResponseMsg));
		break;

	case ECompanionVoiceCommand::PULSE:
		Guide->TriggerEcholocationPulse();
		ResponseMsg = TEXT("Echo pulse emitted.");
		if (GEngine) GEngine->AddOnScreenDebugMessage(502, 4.0f, FColor::Green, FString::Printf(TEXT("[Companion]: [PULSE] %s"), *ResponseMsg));
		break;

	case ECompanionVoiceCommand::EXIT:
		Guide->GuideToExit(ResponseMsg);
		if (GEngine) GEngine->AddOnScreenDebugMessage(502, 4.0f, FColor::Green, FString::Printf(TEXT("[Companion]: [EXIT] %s"), *ResponseMsg));
		break;

	case ECompanionVoiceCommand::SCOUT:
		Guide->ScoutAhead(1400.0f);
		ResponseMsg = TEXT("Scouting hallway ahead.");
		if (GEngine) GEngine->AddOnScreenDebugMessage(502, 4.0f, FColor::Green, FString::Printf(TEXT("[Companion]: [SCOUT] %s"), *ResponseMsg));
		break;

	case ECompanionVoiceCommand::TRAPS:
		{
			int32 Found = 0;
			Guide->ScanForTraps(1500.0f, Found, ResponseMsg);
			if (GEngine) GEngine->AddOnScreenDebugMessage(502, 4.0f, FColor::Green, FString::Printf(TEXT("[Companion]: [TRAPS] %s"), *ResponseMsg));
		}
		break;

	case ECompanionVoiceCommand::DANGER:
		{
			bool bDanger = false;
			int32 Count = 0;
			Guide->SenseDanger(bDanger, Count, ResponseMsg);
			if (GEngine) GEngine->AddOnScreenDebugMessage(502, 4.0f, FColor::Green, FString::Printf(TEXT("[Companion]: [DANGER] %s"), *ResponseMsg));
		}
		break;

	case ECompanionVoiceCommand::BOSS:
		{
			float Pct = 0.0f;
			Guide->QueryBossHealth(ResponseMsg, Pct);
			if (GEngine) GEngine->AddOnScreenDebugMessage(502, 4.0f, FColor::Green, FString::Printf(TEXT("[Companion]: [BOSS] %s"), *ResponseMsg));
		}
		break;

	case ECompanionVoiceCommand::UPGRADE:
		Guide->OpenUpgradeInterface();
		ResponseMsg = TEXT("Opening upgrade forge.");
		if (GEngine) GEngine->AddOnScreenDebugMessage(502, 4.0f, FColor::Green, FString::Printf(TEXT("[Companion]: [UPGRADE] %s"), *ResponseMsg));
		break;

	case ECompanionVoiceCommand::DISTRACT:
		Guide->DistractEnemies();
		ResponseMsg = TEXT("Distracting monsters with strobe light!");
		if (GEngine) GEngine->AddOnScreenDebugMessage(502, 4.0f, FColor::Green, FString::Printf(TEXT("[Companion]: [DISTRACT] %s"), *ResponseMsg));
		break;

	default:
		break;
	}

	OnCommandExecuted.Broadcast(FName(*ResponseMsg), ResponseMsg);
}

void UCompanionVoiceComponent::WarmUpGeminiConnection()
{
	FString ActiveKey = GeminiApiKey;
	if (ActiveKey.IsEmpty())
	{
		ActiveKey = FPlatformMisc::GetEnvironmentVariable(TEXT("GEMINI_API_KEY"));
	}

	if (ActiveKey.IsEmpty())
	{
		return;
	}

	const FString Url = FString::Printf(TEXT("https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s"),
		*GeminiModel, *ActiveKey);

	// Tiny pre-flight ping
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> WarmUpReq = FHttpModule::Get().CreateRequest();
	WarmUpReq->SetURL(Url);
	WarmUpReq->SetVerb(TEXT("POST"));
	WarmUpReq->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	WarmUpReq->SetContentAsString(TEXT("{\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"ping\"}]}],\"generationConfig\":{\"maxOutputTokens\":1}}"));
	WarmUpReq->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSuccess) {
		UE_LOG(LogTemp, Log, TEXT("[CompanionVoice] HTTP connection warm-up finished (Success: %s)"), bSuccess ? TEXT("TRUE") : TEXT("FALSE"));
	});
	WarmUpReq->ProcessRequest();
}

void UCompanionVoiceComponent::SetLocalRecognizer(TScriptInterface<ICompanionLocalRecognizerInterface> InRecognizer)
{
	LocalRecognizer = InRecognizer;
}

void UCompanionVoiceComponent::SetGeminiApiKey(const FString& NewKey)
{
	GeminiApiKey = NewKey;
}

void UCompanionVoiceComponent::OpenWindowsMicrophoneSettings()
{
	FPlatformProcess::LaunchURL(TEXT("ms-settings:privacy-microphone"), nullptr, nullptr);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(508, 6.0f, FColor::Cyan, TEXT("[Companion]: [SETTINGS] Opened Windows Microphone Privacy Settings. Ensure desktop app access is ON."));
	}
}

ACompanionGuide* UCompanionVoiceComponent::GetCompanionGuide() const
{
	return Cast<ACompanionGuide>(GetOwner());
}
