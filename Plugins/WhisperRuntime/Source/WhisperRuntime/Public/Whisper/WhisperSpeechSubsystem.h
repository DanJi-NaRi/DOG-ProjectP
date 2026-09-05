#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MicRecorder.h"
#include "WhisperTypes.h"
#include "WhisperSpeechSubsystem.generated.h"

class FWhisperRuntimeWrapper;

UCLASS()
class WHISPERRUNTIME_API UWhisperSpeechSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "Whisper")
	FWhisperRecordingStartedDynamic OnRecordingStarted;

	UPROPERTY(BlueprintAssignable, Category = "Whisper")
	FWhisperRecordingStoppedDynamic OnRecordingStopped;

	UPROPERTY(BlueprintAssignable, Category = "Whisper")
	FWhisperTranscriptionCompletedDynamic OnTranscriptionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Whisper")
	FWhisperTranscriptionFailedDynamic OnTranscriptionFailed;

	UFUNCTION(BlueprintCallable, Category = "Whisper")
	void InitializeWhisper(const FString& ModelPath);

	UFUNCTION(BlueprintCallable, Category = "Whisper")
	void LoadModel(const FString& ModelPath);

	UFUNCTION(BlueprintCallable, Category = "Whisper")
	void LoadDefaultModel();

	UFUNCTION(BlueprintPure, Category = "Whisper")
	bool IsModelLoaded() const;

	UFUNCTION(BlueprintPure, Category = "Whisper")
	bool IsModelLoadInProgress() const;

	UFUNCTION(BlueprintCallable, Category = "Whisper")
	void TranscribeWavFile(const FString& WavPath, const FString& Language, int32 ThreadCount);

	UFUNCTION(BlueprintCallable, Category = "Whisper")
	void StartRecording();

	UFUNCTION(BlueprintCallable, Category = "Whisper")
	void StopRecording();

	UFUNCTION(BlueprintCallable, Category = "Whisper")
	void StopAndTranscribe(const FString& Language, int32 ThreadCount);

	UFUNCTION(BlueprintCallable, Category = "Whisper")
	void RecordAndTranscribe(float DurationSeconds, const FString& Language, int32 ThreadCount);

	UFUNCTION(BlueprintCallable, Category = "Whisper")
	void ShutdownWhisper();

	UFUNCTION(BlueprintPure, Category = "Whisper")
	FString GetDefaultModelPath() const;

	UFUNCTION(BlueprintPure, Category = "Whisper")
	FString GetLastRecordedWavPath() const;

private:
	TSharedPtr<FWhisperRuntimeWrapper, ESPMode::ThreadSafe> RuntimeWrapper;
	TUniquePtr<FMicRecorder> MicRecorder;

	bool bModelLoadInProgress = false;
	bool bTranscriptionInProgress = false;
	FString LoadedModelPath;
	FString LastRecordedWavPath;
	FTimerHandle TimedRecordingTimerHandle;
	FString PendingTimedRecordingLanguage;
	int32 PendingTimedRecordingThreadCount = 0;

	FString ResolveModelPath(const FString& ModelPath) const;
	FString ResolveAudioPath(const FString& AudioPath) const;
	FString BuildRecordedWavPath() const;
	FString GetDefaultLanguage() const;
	int32 GetDefaultThreadCount() const;
	bool CanStartTranscription(FString& OutErrorMessage) const;
	bool StopRecordingInternal(bool bSaveWav, FString& OutSavedWavPath, FString& OutErrorMessage);
	void TranscribeSamplesAsync(
		const TArray<float>& Samples,
		const FString& SourceAudioPath,
		const FString& Language,
		int32 ThreadCount);
	void BroadcastFailure(const FString& ErrorMessage);
};
