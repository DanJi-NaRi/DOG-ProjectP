#include "WhisperRuntimeWrapper.h"

#include "Misc/ScopeLock.h"

#if WHISPERRUNTIME_WITH_WHISPER
THIRD_PARTY_INCLUDES_START
#include "whisper.h"
THIRD_PARTY_INCLUDES_END
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWhisperRuntimeWrapper, Log, All);

FWhisperRuntimeWrapper::~FWhisperRuntimeWrapper()
{
	Release();
}

bool FWhisperRuntimeWrapper::LoadModel(const FString& ModelPath, FString& OutErrorMessage)
{
#if WHISPERRUNTIME_WITH_WHISPER
	if (ModelPath.IsEmpty())
	{
		OutErrorMessage = TEXT("Whisper model path is empty.");
		return false;
	}

	FScopeLock Lock(&ContextCriticalSection);

	if (Context && LoadedModelPath == ModelPath)
	{
		return true;
	}

	if (Context)
	{
		whisper_free(Context);
		Context = nullptr;
		LoadedModelPath.Reset();
	}

	whisper_context_params ContextParams = whisper_context_default_params();
	const FTCHARToUTF8 ModelPathUtf8(*ModelPath);

	Context = whisper_init_from_file_with_params(ModelPathUtf8.Get(), ContextParams);
	if (!Context)
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to initialize Whisper model: %s"), *ModelPath);
		return false;
	}

	LoadedModelPath = ModelPath;
	UE_LOG(LogWhisperRuntimeWrapper, Log, TEXT("Whisper model loaded: %s"), *LoadedModelPath);
	return true;
#else
	OutErrorMessage = TEXT("Whisper runtime is not included in this build.");
	return false;
#endif
}

bool FWhisperRuntimeWrapper::IsModelLoaded() const
{
#if WHISPERRUNTIME_WITH_WHISPER
	FScopeLock Lock(&ContextCriticalSection);
	return Context != nullptr;
#else
	return false;
#endif
}

bool FWhisperRuntimeWrapper::TranscribeSamples(
	const TArray<float>& Samples,
	const FString& Language,
	int32 ThreadCount,
	FWhisperTranscriptionResult& OutResult,
	FString& OutErrorMessage)
{
#if WHISPERRUNTIME_WITH_WHISPER
	if (Samples.Num() <= 0)
	{
		OutErrorMessage = TEXT("Whisper transcription failed because the sample buffer is empty.");
		return false;
	}

	FScopeLock Lock(&ContextCriticalSection);

	if (!Context)
	{
		OutErrorMessage = TEXT("Whisper model is not loaded.");
		return false;
	}

	const FString LanguageToUse = Language.IsEmpty() ? TEXT("ko") : Language;
	const int32 ThreadCountToUse = FMath::Max(1, ThreadCount);
	const FTCHARToUTF8 LanguageUtf8(*LanguageToUse);

	whisper_full_params FullParams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
	FullParams.language = LanguageUtf8.Get();
	FullParams.translate = false;
	FullParams.print_progress = false;
	FullParams.print_special = false;
	FullParams.print_realtime = false;
	FullParams.print_timestamps = false;
	FullParams.n_threads = ThreadCountToUse;

	const int32 WhisperResult = whisper_full(
		Context,
		FullParams,
		Samples.GetData(),
		Samples.Num());

	if (WhisperResult != 0)
	{
		OutErrorMessage = FString::Printf(TEXT("whisper_full failed. Result code: %d"), WhisperResult);
		return false;
	}

	OutResult = FWhisperTranscriptionResult();
	OutResult.Language = LanguageToUse;
	OutResult.ModelPath = LoadedModelPath;

	const int32 SegmentCount = whisper_full_n_segments(Context);
	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		const int64 StartTime = whisper_full_get_segment_t0(Context, SegmentIndex);
		const int64 EndTime = whisper_full_get_segment_t1(Context, SegmentIndex);
		const char* SegmentTextUtf8 = whisper_full_get_segment_text(Context, SegmentIndex);

		FString SegmentText;
		if (SegmentTextUtf8)
		{
			const FUTF8ToTCHAR SegmentTextTchar(SegmentTextUtf8);
			SegmentText = FString(SegmentTextTchar.Get());
		}

		FWhisperTranscriptionSegment Segment;
		Segment.StartMs = static_cast<int32>(StartTime * 10);
		Segment.EndMs = static_cast<int32>(EndTime * 10);
		Segment.Text = SegmentText;
		OutResult.Segments.Add(Segment);

		OutResult.Text += SegmentText;
	}

	OutResult.Text.TrimStartAndEndInline();
	return true;
#else
	OutErrorMessage = TEXT("Whisper runtime is not included in this build.");
	return false;
#endif
}

void FWhisperRuntimeWrapper::Release()
{
#if WHISPERRUNTIME_WITH_WHISPER
	FScopeLock Lock(&ContextCriticalSection);

	if (Context)
	{
		whisper_free(Context);
		Context = nullptr;
	}

	LoadedModelPath.Reset();
#else
	LoadedModelPath.Reset();
#endif
}

FString FWhisperRuntimeWrapper::GetLoadedModelPath() const
{
	FScopeLock Lock(&ContextCriticalSection);
	return LoadedModelPath;
}
