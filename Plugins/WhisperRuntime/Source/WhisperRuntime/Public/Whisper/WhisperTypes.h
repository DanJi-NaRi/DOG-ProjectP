#pragma once

#include "CoreMinimal.h"
#include "WhisperTypes.generated.h"

USTRUCT(BlueprintType)
struct WHISPERRUNTIME_API FWhisperTranscriptionSegment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Whisper")
	int32 StartMs = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Whisper")
	int32 EndMs = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Whisper")
	FString Text;
};

USTRUCT(BlueprintType)
struct WHISPERRUNTIME_API FWhisperTranscriptionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Whisper")
	FString Text;

	UPROPERTY(BlueprintReadOnly, Category = "Whisper")
	FString SourceAudioPath;

	UPROPERTY(BlueprintReadOnly, Category = "Whisper")
	FString ModelPath;

	UPROPERTY(BlueprintReadOnly, Category = "Whisper")
	FString Language;

	UPROPERTY(BlueprintReadOnly, Category = "Whisper")
	TArray<FWhisperTranscriptionSegment> Segments;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWhisperRecordingStartedDynamic);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWhisperRecordingStoppedDynamic);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWhisperTranscriptionCompletedDynamic, const FWhisperTranscriptionResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWhisperTranscriptionFailedDynamic, const FString&, ErrorMessage);
