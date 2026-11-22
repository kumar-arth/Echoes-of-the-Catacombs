// Copyright Epic Games, Inc. All Rights Reserved.

#include "CompanionKeywordRecognizer.h"
#include "Misc/Char.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <sapi.h>
#include <objbase.h>
#include "Windows/HideWindowsPlatformTypes.h"
#ifdef PlaySound
#undef PlaySound
#endif
#endif

bool UCompanionKeywordRecognizer::ContainsNegationWord(const FString& InText)
{
	// Clean text and split into individual word tokens
	FString CleanText = InText.ToLower();
	for (int32 i = 0; i < CleanText.Len(); ++i)
	{
		TCHAR C = CleanText[i];
		if (!FChar::IsAlnum(C) && C != '\'')
		{
			CleanText[i] = ' ';
		}
	}

	TArray<FString> Tokens;
	CleanText.ParseIntoArray(Tokens, TEXT(" "), true);

	static const TArray<FString> NegationTokens = {
		TEXT("not"), TEXT("don't"), TEXT("dont"), TEXT("no"),
		TEXT("stop"), TEXT("never"), TEXT("cancel"), TEXT("neither"),
		TEXT("nor"), TEXT("won't"), TEXT("wont"), TEXT("can't"), TEXT("cant")
	};

	for (const FString& Token : Tokens)
	{
		if (NegationTokens.Contains(Token))
		{
			return true;
		}
	}

	return false;
}

bool UCompanionKeywordRecognizer::MatchesWholeWordOrPhrase(const FString& InNormalizedText, const TArray<FString>& InTokens, const TArray<FString>& TargetKeywords)
{
	const FString SpacedNormalized = FString::Printf(TEXT(" %s "), *InNormalizedText);

	for (const FString& Keyword : TargetKeywords)
	{
		if (!Keyword.Contains(TEXT(" ")))
		{
			// Single-word target: exact whole-token match
			if (InTokens.Contains(Keyword))
			{
				return true;
			}
		}
		else
		{
			// Multi-word phrase target: must be bounded by spaces or string boundaries
			const FString SpacedPhrase = FString::Printf(TEXT(" %s "), *Keyword.ToLower());
			if (SpacedNormalized.Contains(SpacedPhrase))
			{
				return true;
			}
		}
	}

	return false;
}

bool UCompanionKeywordRecognizer::RecognizeAudio(const TArray<int16>& InPcmSamples, int32 InSampleRate, int32 InChannels, FCompanionVoiceResult& OutResult)
{
	FString RecognizedTranscript;
	if (RunWindowsSapiSpeechRecognition(InPcmSamples, InSampleRate, InChannels, RecognizedTranscript))
	{
		if (!RecognizedTranscript.IsEmpty())
		{
			UE_LOG(LogTemp, Log, TEXT("[CompanionVoice][LocalSAPI] SAPI recognized transcript: '%s'"), *RecognizedTranscript);
			return RecognizeCommand(RecognizedTranscript, OutResult);
		}
	}

	return false;
}

bool UCompanionKeywordRecognizer::RecognizeCommand(const FString& InText, FCompanionVoiceResult& OutResult)
{
	OutResult.RawTranscript = InText;
	OutResult.bIsLocalFastPath = true;

	const FString RawLower = InText.ToLower().TrimStartAndEnd();
	if (RawLower.IsEmpty())
	{
		OutResult.Command = ECompanionVoiceCommand::UNKNOWN;
		OutResult.Confidence = 0.0f;
		return false;
	}

	// 1. Negation words ("don't", "not", "stop", "no") MUST route to Gemini cloud
	if (ContainsNegationWord(RawLower))
	{
		UE_LOG(LogTemp, Log, TEXT("[CompanionVoice][LocalRecognizer] Negation detected in '%s' -> Routing to Gemini cloud."), *InText);
		OutResult.Command = ECompanionVoiceCommand::UNKNOWN;
		OutResult.Confidence = 0.0f;
		return false;
	}

	// 2. Tokenize normalized text for whole-word matching
	FString Normalized = RawLower;
	for (int32 i = 0; i < Normalized.Len(); ++i)
	{
		const TCHAR C = Normalized[i];
		if (!FChar::IsAlnum(C) && C != '\'')
		{
			Normalized[i] = ' ';
		}
	}

	TArray<FString> Tokens;
	Normalized.ParseIntoArray(Tokens, TEXT(" "), true);
	if (Tokens.Num() == 0)
	{
		OutResult.Command = ECompanionVoiceCommand::UNKNOWN;
		OutResult.Confidence = 0.0f;
		return false;
	}

	// -------------------------------------------------------------------------
	// Core Commands & Exact Whole-Word / Multi-word Synonyms
	// -------------------------------------------------------------------------

	// STAY: "stay", "wait", "hold", "freeze", "anchor", "stand by", "stay here", "stay put", "wait here"
	static const TArray<FString> StayKeywords = {
		TEXT("stay"), TEXT("wait"), TEXT("hold"), TEXT("freeze"),
		TEXT("anchor"), TEXT("stand by"), TEXT("stay here"), TEXT("stay put"), TEXT("wait here")
	};
	if (MatchesWholeWordOrPhrase(Normalized, Tokens, StayKeywords))
	{
		OutResult.Command = ECompanionVoiceCommand::STAY;
		OutResult.Confidence = 1.0f;
		return OutResult.Confidence >= 0.70f;
	}

	// FOLLOW: "follow", "to me", "heel", "come back", "come with me", "with me", "let's go", "keep up", "close up", "return"
	static const TArray<FString> FollowKeywords = {
		TEXT("follow"), TEXT("to me"), TEXT("heel"), TEXT("come back"),
		TEXT("come with me"), TEXT("with me"), TEXT("let's go"), TEXT("keep up"),
		TEXT("close up"), TEXT("return")
	};
	if (MatchesWholeWordOrPhrase(Normalized, Tokens, FollowKeywords))
	{
		OutResult.Command = ECompanionVoiceCommand::FOLLOW;
		OutResult.Confidence = 1.0f;
		return OutResult.Confidence >= 0.70f;
	}

	// RETREAT: "retreat", "run", "fall back", "safe distance", "get back", "stay back", "keep distance", "flee", "back off"
	// Requires higher confidence (>= 0.85f)
	static const TArray<FString> RetreatKeywords = {
		TEXT("retreat"), TEXT("run"), TEXT("fall back"), TEXT("safe distance"),
		TEXT("get back"), TEXT("stay back"), TEXT("keep distance"), TEXT("flee"), TEXT("back off")
	};
	if (MatchesWholeWordOrPhrase(Normalized, Tokens, RetreatKeywords))
	{
		OutResult.Command = ECompanionVoiceCommand::RETREAT;
		OutResult.Confidence = 0.90f; // Exceeds 0.85 threshold
		return OutResult.Confidence >= 0.85f;
	}

	// ATTACK: "attack", "concuss", "stun", "knock out", "strike", "blast"
	// Requires higher confidence (>= 0.85f)
	static const TArray<FString> AttackKeywords = {
		TEXT("attack"), TEXT("concuss"), TEXT("stun"), TEXT("knock out"), TEXT("strike"), TEXT("blast")
	};
	if (MatchesWholeWordOrPhrase(Normalized, Tokens, AttackKeywords))
	{
		OutResult.Command = ECompanionVoiceCommand::ATTACK;
		OutResult.Confidence = 0.90f; // Exceeds 0.85 threshold
		return OutResult.Confidence >= 0.85f;
	}

	// PULSE: "pulse", "send pulse", "send a pulse", "echo", "ping", "sonar", "sound wave", "acoustic pulse"
	static const TArray<FString> PulseKeywords = {
		TEXT("pulse"), TEXT("send pulse"), TEXT("send a pulse"), TEXT("echo"),
		TEXT("ping"), TEXT("sonar"), TEXT("sound wave"), TEXT("acoustic pulse")
	};
	if (MatchesWholeWordOrPhrase(Normalized, Tokens, PulseKeywords))
	{
		OutResult.Command = ECompanionVoiceCommand::PULSE;
		OutResult.Confidence = 1.0f;
		return OutResult.Confidence >= 0.70f;
	}

	// EXIT: "exit", "escape", "way out", "portal"
	static const TArray<FString> ExitKeywords = {
		TEXT("exit"), TEXT("escape"), TEXT("way out"), TEXT("portal")
	};
	if (MatchesWholeWordOrPhrase(Normalized, Tokens, ExitKeywords))
	{
		OutResult.Command = ECompanionVoiceCommand::EXIT;
		OutResult.Confidence = 1.0f;
		return OutResult.Confidence >= 0.70f;
	}

	// SCOUT
	static const TArray<FString> ScoutKeywords = {
		TEXT("scout"), TEXT("look ahead"), TEXT("check ahead"), TEXT("explore")
	};
	if (MatchesWholeWordOrPhrase(Normalized, Tokens, ScoutKeywords))
	{
		OutResult.Command = ECompanionVoiceCommand::SCOUT;
		OutResult.Confidence = 0.95f;
		return OutResult.Confidence >= 0.70f;
	}

	// TRAPS
	static const TArray<FString> TrapsKeywords = {
		TEXT("traps"), TEXT("trap"), TEXT("spikes"), TEXT("hazard"), TEXT("hazards")
	};
	if (MatchesWholeWordOrPhrase(Normalized, Tokens, TrapsKeywords))
	{
		OutResult.Command = ECompanionVoiceCommand::TRAPS;
		OutResult.Confidence = 1.0f;
		return OutResult.Confidence >= 0.70f;
	}

	// DANGER
	static const TArray<FString> DangerKeywords = {
		TEXT("danger"), TEXT("threat"), TEXT("safe")
	};
	if (MatchesWholeWordOrPhrase(Normalized, Tokens, DangerKeywords))
	{
		OutResult.Command = ECompanionVoiceCommand::DANGER;
		OutResult.Confidence = 0.90f;
		return OutResult.Confidence >= 0.70f;
	}

	// BOSS
	static const TArray<FString> BossKeywords = {
		TEXT("boss")
	};
	if (MatchesWholeWordOrPhrase(Normalized, Tokens, BossKeywords))
	{
		OutResult.Command = ECompanionVoiceCommand::BOSS;
		OutResult.Confidence = 0.95f;
		return OutResult.Confidence >= 0.70f;
	}

	// UPGRADE
	static const TArray<FString> UpgradeKeywords = {
		TEXT("upgrade"), TEXT("forge"), TEXT("enhance"), TEXT("craft")
	};
	if (MatchesWholeWordOrPhrase(Normalized, Tokens, UpgradeKeywords))
	{
		OutResult.Command = ECompanionVoiceCommand::UPGRADE;
		OutResult.Confidence = 1.0f;
		return OutResult.Confidence >= 0.70f;
	}

	// DISTRACT
	static const TArray<FString> DistractKeywords = {
		TEXT("distract"), TEXT("decoy"), TEXT("lure"), TEXT("bait")
	};
	if (MatchesWholeWordOrPhrase(Normalized, Tokens, DistractKeywords))
	{
		OutResult.Command = ECompanionVoiceCommand::DISTRACT;
		OutResult.Confidence = 0.95f;
		return OutResult.Confidence >= 0.70f;
	}

	OutResult.Command = ECompanionVoiceCommand::UNKNOWN;
	OutResult.Confidence = 0.0f;
	return false;
}

bool UCompanionKeywordRecognizer::RunWindowsSapiSpeechRecognition(const TArray<int16>& InPcmSamples, int32 InSampleRate, int32 InChannels, FString& OutRecognizedText)
{
#if PLATFORM_WINDOWS
	if (InPcmSamples.Num() == 0 || InSampleRate <= 0 || InChannels <= 0)
	{
		return false;
	}

	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	bool bNeedCoUninit = SUCCEEDED(hr);

	ISpRecognizer* pRecognizer = nullptr;
	hr = CoCreateInstance(CLSID_SpInprocRecognizer, NULL, CLSCTX_INPROC_SERVER, IID_ISpRecognizer, (void**)&pRecognizer);
	if (FAILED(hr) || !pRecognizer)
	{
		if (bNeedCoUninit) CoUninitialize();
		return false;
	}

	// Configure WAVE format
	WAVEFORMATEX Wf;
	FMemory::Memzero(&Wf, sizeof(Wf));
	Wf.wFormatTag = WAVE_FORMAT_PCM;
	Wf.nChannels = (WORD)InChannels;
	Wf.nSamplesPerSec = (DWORD)InSampleRate;
	Wf.wBitsPerSample = 16;
	Wf.nBlockAlign = (Wf.nChannels * Wf.wBitsPerSample) / 8;
	Wf.nAvgBytesPerSec = Wf.nSamplesPerSec * Wf.nBlockAlign;

	const int32 DataBytes = InPcmSamples.Num() * sizeof(int16);
	HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, DataBytes);
	if (!hGlobal)
	{
		pRecognizer->Release();
		if (bNeedCoUninit) CoUninitialize();
		return false;
	}

	void* pDest = GlobalLock(hGlobal);
	if (pDest)
	{
		FMemory::Memcpy(pDest, InPcmSamples.GetData(), DataBytes);
		GlobalUnlock(hGlobal);
	}

	IStream* pStream = nullptr;
	CreateStreamOnHGlobal(hGlobal, 1, &pStream);

	ISpStream* pSpStream = nullptr;
	hr = CoCreateInstance(CLSID_SpStream, NULL, CLSCTX_INPROC_SERVER, IID_ISpStream, (void**)&pSpStream);
	if (FAILED(hr) || !pSpStream)
	{
		if (pStream) pStream->Release();
		pRecognizer->Release();
		if (bNeedCoUninit) CoUninitialize();
		return false;
	}

	pSpStream->SetBaseStream(pStream, SPDFID_WaveFormatEx, &Wf);
	pRecognizer->SetInput(pSpStream, 1);

	ISpRecoContext* pRecoContext = nullptr;
	hr = pRecognizer->CreateRecoContext(&pRecoContext);
	if (FAILED(hr) || !pRecoContext)
	{
		pSpStream->Release();
		pStream->Release();
		pRecognizer->Release();
		if (bNeedCoUninit) CoUninitialize();
		return false;
	}

	ISpRecoGrammar* pGrammar = nullptr;
	hr = pRecoContext->CreateGrammar(1, &pGrammar);
	if (FAILED(hr) || !pGrammar)
	{
		pRecoContext->Release();
		pSpStream->Release();
		pStream->Release();
		pRecognizer->Release();
		if (bNeedCoUninit) CoUninitialize();
		return false;
	}

	// Restricted grammar for core commands and common variations
	SPSTATEHANDLE hRuleState = 0;
	pGrammar->GetRule(L"Commands", 1, SPRAF_TopLevel | SPRAF_Active, 1, &hRuleState);

	const wchar_t* GrammarWords[] = {
		L"stay", L"wait", L"hold", L"freeze",
		L"follow", L"heel", L"return",
		L"retreat", L"run", L"flee",
		L"attack", L"strike", L"concuss", L"blast", L"stun",
		L"pulse", L"send pulse", L"echo", L"ping", L"sonar",
		L"exit", L"escape",
		L"traps", L"danger", L"boss", L"upgrade", L"distract"
	};

	for (const wchar_t* Word : GrammarWords)
	{
		pGrammar->AddWordTransition(hRuleState, NULL, Word, L" ", SPWT_LEXICAL, 1.0f, NULL);
	}
	pGrammar->Commit(0);
	pGrammar->SetRuleState(L"Commands", NULL, SPRS_ACTIVE);

	HANDLE hEvent = pRecoContext->GetNotifyEventHandle();
	pRecoContext->SetNotifyWin32Event();
	pRecoContext->SetInterest(SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_FALSE_RECOGNITION) | SPFEI(SPEI_END_SR_STREAM),
	                          SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_FALSE_RECOGNITION) | SPFEI(SPEI_END_SR_STREAM));

	// Wait up to 200 ms for in-process recognition on this short audio segment
	WaitForSingleObject(hEvent, 200);

	bool bRecognized = false;
	SPEVENT Event;
	while (pRecoContext->GetEvents(1, &Event, NULL) == S_OK)
	{
		if (Event.eEventId == SPEI_RECOGNITION)
		{
			ISpRecoResult* pRecoResult = reinterpret_cast<ISpRecoResult*>(Event.lParam);
			if (pRecoResult)
			{
				WCHAR* pText = nullptr;
				if (SUCCEEDED(pRecoResult->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, 1, &pText, NULL)) && pText)
				{
					OutRecognizedText = FString(pText).TrimStartAndEnd();
					CoTaskMemFree(pText);
					bRecognized = !OutRecognizedText.IsEmpty();
				}
			}
		}
	}

	pGrammar->Release();
	pRecoContext->Release();
	pSpStream->Release();
	pStream->Release();
	pRecognizer->Release();
	if (bNeedCoUninit) CoUninitialize();

	return bRecognized;
#else
	return false;
#endif
}
