#pragma once

#include "CoreMinimal.h"

struct WHISPERRUNTIME_API FWhisperWavData
{
	int32 SampleRate = 0;
	int32 NumChannels = 0;
	TArray<float> Samples;
};

class WHISPERRUNTIME_API FWhisperWavLoader
{
public:
	static bool LoadPcm16AsFloatMono(const FString& WavPath, FWhisperWavData& OutWavData, FString& OutErrorMessage);
};
