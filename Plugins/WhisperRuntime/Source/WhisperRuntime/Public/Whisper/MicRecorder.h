#pragma once

#include "CoreMinimal.h"

class WHISPERRUNTIME_API FMicRecorder
{
public:
	FMicRecorder();
	~FMicRecorder();

	bool StartRecording(FString& OutErrorMessage);
	bool StopRecording(FString& OutErrorMessage);
	bool IsRecording() const;
	bool GetRecordedSamples(TArray<float>& OutSamples) const;
	bool SaveRecordedWav(const FString& OutputPath, FString& OutErrorMessage) const;
	void Reset();

	int32 GetSampleRate() const;
	int32 GetNumChannels() const;
	int32 GetSampleCount() const;

private:
	struct FImpl;
	TUniquePtr<FImpl> Impl;
};
