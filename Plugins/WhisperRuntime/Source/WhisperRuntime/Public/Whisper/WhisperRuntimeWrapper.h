#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "WhisperTypes.h"

struct whisper_context;

class WHISPERRUNTIME_API FWhisperRuntimeWrapper
{
public:
	FWhisperRuntimeWrapper() = default;
	~FWhisperRuntimeWrapper();

	bool LoadModel(const FString& ModelPath, FString& OutErrorMessage);
	bool IsModelLoaded() const;
	bool TranscribeSamples(
		const TArray<float>& Samples,
		const FString& Language,
		int32 ThreadCount,
		FWhisperTranscriptionResult& OutResult,
		FString& OutErrorMessage);
	void Release();

	FString GetLoadedModelPath() const;

private:
	mutable FCriticalSection ContextCriticalSection;
	whisper_context* Context = nullptr;
	FString LoadedModelPath;
};
