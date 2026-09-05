#include "WhisperSpeechSubsystem.h"

#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "MicRecorder.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "WhisperRuntimeWrapper.h"
#include "WhisperWavLoader.h"

DEFINE_LOG_CATEGORY_STATIC(LogWhisperSpeechSubsystem, Log, All);

namespace
{
	static constexpr TCHAR WhisperConfigSection[] = TEXT("WhisperRuntime");
	static constexpr TCHAR LegacyWhisperConfigSection[] = TEXT("ProjectP.Whisper");
	static constexpr TCHAR DefaultModelRelativePathKey[] = TEXT("DefaultModelRelativePath");
	static constexpr TCHAR DefaultLanguageKey[] = TEXT("DefaultLanguage");
	static constexpr TCHAR DefaultThreadCountKey[] = TEXT("DefaultThreadCount");
	static constexpr TCHAR RecordingOutputRelativePathKey[] = TEXT("RecordingOutputRelativePath");
	static constexpr TCHAR DefaultModelRelativePath[] = TEXT("Models/ggml-base.bin");
	static constexpr TCHAR DefaultRecordingOutputRelativePath[] = TEXT("Whisper/Recordings");
	static constexpr TCHAR DefaultLanguage[] = TEXT("ko");

	bool ReadWhisperConfigString(const TCHAR* Key, FString& OutValue)
	{
		if (!GConfig)
		{
			return false;
		}

		return GConfig->GetString(WhisperConfigSection, Key, OutValue, GGameIni)
			|| GConfig->GetString(LegacyWhisperConfigSection, Key, OutValue, GGameIni);
	}

	bool ReadWhisperConfigInt(const TCHAR* Key, int32& OutValue)
	{
		if (!GConfig)
		{
			return false;
		}

		return GConfig->GetInt(WhisperConfigSection, Key, OutValue, GGameIni)
			|| GConfig->GetInt(LegacyWhisperConfigSection, Key, OutValue, GGameIni);
	}

	FString NormalizeWhisperPath(const FString& Path)
	{
		FString NormalizedPath = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(NormalizedPath);
		FPaths::CollapseRelativeDirectories(NormalizedPath);
		return NormalizedPath;
	}

	FString ResolveWhisperPluginContentPath(const FString& RelativePath)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("WhisperRuntime"));
		if (!Plugin.IsValid())
		{
			return FString();
		}

		return NormalizeWhisperPath(FPaths::Combine(Plugin->GetContentDir(), RelativePath));
	}

	UWhisperSpeechSubsystem* GetWhisperSubsystemFromWorld(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		UGameInstance* GameInstance = World->GetGameInstance();
		return GameInstance ? GameInstance->GetSubsystem<UWhisperSpeechSubsystem>() : nullptr;
	}

	void LoadWhisperModelCommand(const TArray<FString>& Args, UWorld* World)
	{
		UWhisperSpeechSubsystem* WhisperSubsystem = GetWhisperSubsystemFromWorld(World);
		if (!WhisperSubsystem)
		{
			UE_LOG(LogWhisperSpeechSubsystem, Warning, TEXT("WhisperRuntime.LoadModel failed. GameInstance subsystem is not available."));
			return;
		}

		if (Args.Num() > 0)
		{
			WhisperSubsystem->LoadModel(Args[0]);
		}
		else
		{
			WhisperSubsystem->LoadDefaultModel();
		}
	}

	void TranscribeWhisperWavCommand(const TArray<FString>& Args, UWorld* World)
	{
		UWhisperSpeechSubsystem* WhisperSubsystem = GetWhisperSubsystemFromWorld(World);
		if (!WhisperSubsystem)
		{
			UE_LOG(LogWhisperSpeechSubsystem, Warning, TEXT("WhisperRuntime.TranscribeWav failed. GameInstance subsystem is not available."));
			return;
		}

		if (Args.Num() <= 0)
		{
			UE_LOG(LogWhisperSpeechSubsystem, Warning, TEXT("Usage: WhisperRuntime.TranscribeWav <WavPath> [Language] [ThreadCount]"));
			return;
		}

		const FString Language = Args.Num() > 1 ? Args[1] : FString();
		const int32 ThreadCount = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 0;
		WhisperSubsystem->TranscribeWavFile(Args[0], Language, ThreadCount);
	}

	void StartWhisperRecordingCommand(const TArray<FString>& Args, UWorld* World)
	{
		UWhisperSpeechSubsystem* WhisperSubsystem = GetWhisperSubsystemFromWorld(World);
		if (!WhisperSubsystem)
		{
			UE_LOG(LogWhisperSpeechSubsystem, Warning, TEXT("WhisperRuntime.StartRecording failed. GameInstance subsystem is not available."));
			return;
		}

		WhisperSubsystem->StartRecording();
	}

	void StopWhisperRecordingCommand(const TArray<FString>& Args, UWorld* World)
	{
		UWhisperSpeechSubsystem* WhisperSubsystem = GetWhisperSubsystemFromWorld(World);
		if (!WhisperSubsystem)
		{
			UE_LOG(LogWhisperSpeechSubsystem, Warning, TEXT("WhisperRuntime.StopRecording failed. GameInstance subsystem is not available."));
			return;
		}

		WhisperSubsystem->StopRecording();
	}

	void StopAndTranscribeWhisperRecordingCommand(const TArray<FString>& Args, UWorld* World)
	{
		UWhisperSpeechSubsystem* WhisperSubsystem = GetWhisperSubsystemFromWorld(World);
		if (!WhisperSubsystem)
		{
			UE_LOG(LogWhisperSpeechSubsystem, Warning, TEXT("WhisperRuntime.StopAndTranscribe failed. GameInstance subsystem is not available."));
			return;
		}

		const FString Language = Args.Num() > 0 ? Args[0] : FString();
		const int32 ThreadCount = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
		WhisperSubsystem->StopAndTranscribe(Language, ThreadCount);
	}

	void RecordAndTranscribeWhisperCommand(const TArray<FString>& Args, UWorld* World)
	{
		UWhisperSpeechSubsystem* WhisperSubsystem = GetWhisperSubsystemFromWorld(World);
		if (!WhisperSubsystem)
		{
			UE_LOG(LogWhisperSpeechSubsystem, Warning, TEXT("WhisperRuntime.RecordAndTranscribe failed. GameInstance subsystem is not available."));
			return;
		}

		const float DurationSeconds = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 5.0f;
		const FString Language = Args.Num() > 1 ? Args[1] : FString();
		const int32 ThreadCount = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 0;
		WhisperSubsystem->RecordAndTranscribe(DurationSeconds, Language, ThreadCount);
	}

	static FAutoConsoleCommandWithWorldAndArgs GProjectPWhisperLoadModelCommand(
		TEXT("WhisperRuntime.LoadModel"),
		TEXT("Loads the default Whisper model or the specified model path."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LoadWhisperModelCommand));

	static FAutoConsoleCommandWithWorldAndArgs GProjectPWhisperTranscribeWavCommand(
		TEXT("WhisperRuntime.TranscribeWav"),
		TEXT("Transcribes a 16 kHz PCM WAV file. Usage: WhisperRuntime.TranscribeWav <WavPath> [Language] [ThreadCount]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TranscribeWhisperWavCommand));

	static FAutoConsoleCommandWithWorldAndArgs GProjectPWhisperStartRecordingCommand(
		TEXT("WhisperRuntime.StartRecording"),
		TEXT("Starts microphone recording for the Whisper MVP-1 flow."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartWhisperRecordingCommand));

	static FAutoConsoleCommandWithWorldAndArgs GProjectPWhisperStopRecordingCommand(
		TEXT("WhisperRuntime.StopRecording"),
		TEXT("Stops microphone recording and saves the captured WAV file."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StopWhisperRecordingCommand));

	static FAutoConsoleCommandWithWorldAndArgs GProjectPWhisperStopAndTranscribeCommand(
		TEXT("WhisperRuntime.StopAndTranscribe"),
		TEXT("Stops microphone recording, saves the captured WAV file, and transcribes it. Usage: WhisperRuntime.StopAndTranscribe [Language] [ThreadCount]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StopAndTranscribeWhisperRecordingCommand));

	static FAutoConsoleCommandWithWorldAndArgs GProjectPWhisperRecordAndTranscribeCommand(
		TEXT("WhisperRuntime.RecordAndTranscribe"),
		TEXT("Records microphone input for a fixed duration and transcribes it. Usage: WhisperRuntime.RecordAndTranscribe [Seconds] [Language] [ThreadCount]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RecordAndTranscribeWhisperCommand));
}

void UWhisperSpeechSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimedRecordingTimerHandle);
	}

	if (MicRecorder.IsValid())
	{
		FString ErrorMessage;
		MicRecorder->StopRecording(ErrorMessage);
	}

	MicRecorder.Reset();
	RuntimeWrapper.Reset();
	LoadedModelPath.Reset();
	LastRecordedWavPath.Reset();
	bModelLoadInProgress = false;
	bTranscriptionInProgress = false;

	Super::Deinitialize();
}

void UWhisperSpeechSubsystem::InitializeWhisper(const FString& ModelPath)
{
	LoadModel(ModelPath);
}

void UWhisperSpeechSubsystem::LoadModel(const FString& ModelPath)
{
	if (bModelLoadInProgress)
	{
		BroadcastFailure(TEXT("Whisper model load is already in progress."));
		return;
	}

	const FString ResolvedModelPath = ResolveModelPath(ModelPath);
	if (ResolvedModelPath.IsEmpty())
	{
		BroadcastFailure(TEXT("Whisper model path could not be resolved."));
		return;
	}

	if (!FPaths::FileExists(ResolvedModelPath))
	{
		BroadcastFailure(FString::Printf(TEXT("Whisper model file does not exist: %s"), *ResolvedModelPath));
		return;
	}

	bModelLoadInProgress = true;

	UE_LOG(LogWhisperSpeechSubsystem, Log, TEXT("Whisper model load started: %s"), *ResolvedModelPath);

	TWeakObjectPtr<UWhisperSpeechSubsystem> WeakThis(this);
	Async(EAsyncExecution::ThreadPool, [WeakThis, ResolvedModelPath]()
	{
		TSharedPtr<FWhisperRuntimeWrapper, ESPMode::ThreadSafe> NewWrapper = MakeShared<FWhisperRuntimeWrapper, ESPMode::ThreadSafe>();
		FString ErrorMessage;
		const bool bLoaded = NewWrapper->LoadModel(ResolvedModelPath, ErrorMessage);

		AsyncTask(ENamedThreads::GameThread, [WeakThis, NewWrapper, ResolvedModelPath, ErrorMessage, bLoaded]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			UWhisperSpeechSubsystem* Subsystem = WeakThis.Get();
			Subsystem->bModelLoadInProgress = false;

			if (!bLoaded)
			{
				Subsystem->BroadcastFailure(ErrorMessage);
				return;
			}

			Subsystem->RuntimeWrapper = NewWrapper;
			Subsystem->LoadedModelPath = ResolvedModelPath;

			UE_LOG(LogWhisperSpeechSubsystem, Log, TEXT("Whisper model load completed: %s"), *ResolvedModelPath);
		});
	});
}

void UWhisperSpeechSubsystem::LoadDefaultModel()
{
	LoadModel(GetDefaultModelPath());
}

bool UWhisperSpeechSubsystem::IsModelLoaded() const
{
	return RuntimeWrapper.IsValid() && RuntimeWrapper->IsModelLoaded();
}

bool UWhisperSpeechSubsystem::IsModelLoadInProgress() const
{
	return bModelLoadInProgress;
}

void UWhisperSpeechSubsystem::TranscribeWavFile(const FString& WavPath, const FString& Language, int32 ThreadCount)
{
	FString ErrorMessage;
	if (!CanStartTranscription(ErrorMessage))
	{
		BroadcastFailure(ErrorMessage);
		return;
	}

	const FString ResolvedAudioPath = ResolveAudioPath(WavPath);
	if (ResolvedAudioPath.IsEmpty() || !FPaths::FileExists(ResolvedAudioPath))
	{
		BroadcastFailure(FString::Printf(TEXT("WAV file does not exist: %s"), *ResolvedAudioPath));
		return;
	}

	const FString LanguageToUse = Language.IsEmpty() ? GetDefaultLanguage() : Language;
	const int32 ThreadCountToUse = ThreadCount > 0 ? ThreadCount : GetDefaultThreadCount();

	UE_LOG(LogWhisperSpeechSubsystem, Log, TEXT("Whisper transcription started. Wav: %s, Language: %s, Threads: %d"),
		*ResolvedAudioPath,
		*LanguageToUse,
		ThreadCountToUse);

	TWeakObjectPtr<UWhisperSpeechSubsystem> WeakThis(this);
	Async(EAsyncExecution::ThreadPool, [WeakThis, ResolvedAudioPath, LanguageToUse, ThreadCountToUse]()
	{
		FWhisperWavData WavData;
		FString ErrorMessage;
		const bool bLoadedWav = FWhisperWavLoader::LoadPcm16AsFloatMono(ResolvedAudioPath, WavData, ErrorMessage);

		AsyncTask(ENamedThreads::GameThread, [WeakThis, WavData, ResolvedAudioPath, LanguageToUse, ThreadCountToUse, ErrorMessage, bLoadedWav]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			if (!bLoadedWav)
			{
				WeakThis->BroadcastFailure(ErrorMessage);
				return;
			}

			WeakThis->TranscribeSamplesAsync(WavData.Samples, ResolvedAudioPath, LanguageToUse, ThreadCountToUse);
		});
	});
}

void UWhisperSpeechSubsystem::StartRecording()
{
	if (!MicRecorder.IsValid())
	{
		MicRecorder = MakeUnique<FMicRecorder>();
	}

	if (MicRecorder->IsRecording())
	{
		BroadcastFailure(TEXT("Microphone recording is already in progress."));
		return;
	}

	FString ErrorMessage;
	if (!MicRecorder->StartRecording(ErrorMessage))
	{
		BroadcastFailure(ErrorMessage);
		return;
	}

	LastRecordedWavPath.Reset();
	OnRecordingStarted.Broadcast();
}

void UWhisperSpeechSubsystem::StopRecording()
{
	FString SavedWavPath;
	FString ErrorMessage;
	if (!StopRecordingInternal(true, SavedWavPath, ErrorMessage))
	{
		BroadcastFailure(ErrorMessage);
		return;
	}

	UE_LOG(LogWhisperSpeechSubsystem, Log, TEXT("Whisper microphone recording saved: %s"), *SavedWavPath);
}

void UWhisperSpeechSubsystem::StopAndTranscribe(const FString& Language, int32 ThreadCount)
{
	FString ErrorMessage;
	if (!CanStartTranscription(ErrorMessage))
	{
		BroadcastFailure(ErrorMessage);
		return;
	}

	if (!MicRecorder.IsValid())
	{
		BroadcastFailure(TEXT("Microphone recorder is not initialized."));
		return;
	}

	FString SavedWavPath;
	if (!StopRecordingInternal(true, SavedWavPath, ErrorMessage))
	{
		BroadcastFailure(ErrorMessage);
		return;
	}

	TArray<float> Samples;
	if (!MicRecorder->GetRecordedSamples(Samples))
	{
		BroadcastFailure(TEXT("Microphone recording has no captured samples."));
		return;
	}

	const FString LanguageToUse = Language.IsEmpty() ? GetDefaultLanguage() : Language;
	const int32 ThreadCountToUse = ThreadCount > 0 ? ThreadCount : GetDefaultThreadCount();
	TranscribeSamplesAsync(Samples, SavedWavPath, LanguageToUse, ThreadCountToUse);
}

void UWhisperSpeechSubsystem::RecordAndTranscribe(float DurationSeconds, const FString& Language, int32 ThreadCount)
{
	if (DurationSeconds <= 0.0f)
	{
		BroadcastFailure(TEXT("RecordAndTranscribe duration must be greater than 0."));
		return;
	}

	if (bTranscriptionInProgress || bModelLoadInProgress)
	{
		BroadcastFailure(TEXT("Whisper is busy and cannot start timed recording."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		BroadcastFailure(TEXT("RecordAndTranscribe failed because World is not available."));
		return;
	}

	if (World->GetTimerManager().IsTimerActive(TimedRecordingTimerHandle))
	{
		BroadcastFailure(TEXT("Timed microphone recording is already in progress."));
		return;
	}

	StartRecording();
	if (!MicRecorder.IsValid() || !MicRecorder->IsRecording())
	{
		return;
	}

	PendingTimedRecordingLanguage = Language;
	PendingTimedRecordingThreadCount = ThreadCount;

	TWeakObjectPtr<UWhisperSpeechSubsystem> WeakThis(this);
	World->GetTimerManager().SetTimer(
		TimedRecordingTimerHandle,
		[WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->StopAndTranscribe(WeakThis->PendingTimedRecordingLanguage, WeakThis->PendingTimedRecordingThreadCount);
			}
		},
		DurationSeconds,
		false);

	UE_LOG(LogWhisperSpeechSubsystem, Log, TEXT("Timed Whisper recording started for %.2f seconds."), DurationSeconds);
}

void UWhisperSpeechSubsystem::ShutdownWhisper()
{
	if (bTranscriptionInProgress)
	{
		BroadcastFailure(TEXT("Whisper shutdown skipped because transcription is in progress."));
		return;
	}

	RuntimeWrapper.Reset();
	LoadedModelPath.Reset();
	LastRecordedWavPath.Reset();
	UE_LOG(LogWhisperSpeechSubsystem, Log, TEXT("Whisper runtime shutdown completed."));
}

FString UWhisperSpeechSubsystem::GetDefaultModelPath() const
{
	FString RelativePath;
	ReadWhisperConfigString(DefaultModelRelativePathKey, RelativePath);

	RelativePath.TrimStartAndEndInline();
	if (RelativePath.IsEmpty())
	{
		RelativePath = DefaultModelRelativePath;
	}

	return ResolveModelPath(RelativePath);
}

FString UWhisperSpeechSubsystem::GetLastRecordedWavPath() const
{
	return LastRecordedWavPath;
}

FString UWhisperSpeechSubsystem::ResolveModelPath(const FString& ModelPath) const
{
	FString ResolvedPath = ModelPath;
	ResolvedPath.TrimStartAndEndInline();

	if (ResolvedPath.IsEmpty())
	{
		ResolvedPath = DefaultModelRelativePath;
	}

	if (FPaths::IsRelative(ResolvedPath))
	{
		const FString PluginContentPath = ResolveWhisperPluginContentPath(ResolvedPath);
		if (!PluginContentPath.IsEmpty() && FPaths::FileExists(PluginContentPath))
		{
			return PluginContentPath;
		}

		FString ProjectContentPath = NormalizeWhisperPath(FPaths::Combine(FPaths::ProjectContentDir(), ResolvedPath));
		if (FPaths::FileExists(ProjectContentPath))
		{
			return ProjectContentPath;
		}

		FString BinaryOutputPath = NormalizeWhisperPath(FPaths::Combine(FPlatformProcess::BaseDir(), TEXT("WhisperRuntime"), ResolvedPath));
		if (FPaths::FileExists(BinaryOutputPath))
		{
			return BinaryOutputPath;
		}

		ResolvedPath = !PluginContentPath.IsEmpty() ? PluginContentPath : BinaryOutputPath;
	}

	return NormalizeWhisperPath(ResolvedPath);
}

FString UWhisperSpeechSubsystem::ResolveAudioPath(const FString& AudioPath) const
{
	FString ResolvedPath = AudioPath;
	ResolvedPath.TrimStartAndEndInline();

	if (ResolvedPath.IsEmpty())
	{
		return TEXT("");
	}

	if (!FPaths::IsRelative(ResolvedPath))
	{
		FPaths::NormalizeFilename(ResolvedPath);
		return ResolvedPath;
	}

	FString ContentPath = FPaths::Combine(FPaths::ProjectContentDir(), ResolvedPath);
	FPaths::NormalizeFilename(ContentPath);
	if (FPaths::FileExists(ContentPath))
	{
		return ContentPath;
	}

	FString ProjectPath = FPaths::Combine(FPaths::ProjectDir(), ResolvedPath);
	FPaths::NormalizeFilename(ProjectPath);
	return ProjectPath;
}

FString UWhisperSpeechSubsystem::BuildRecordedWavPath() const
{
	FString RelativePath;
	ReadWhisperConfigString(RecordingOutputRelativePathKey, RelativePath);

	RelativePath.TrimStartAndEndInline();
	if (RelativePath.IsEmpty())
	{
		RelativePath = DefaultRecordingOutputRelativePath;
	}

	const FString FileName = FString::Printf(TEXT("whisper_mic_%s.wav"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	FString OutputPath = FPaths::Combine(FPaths::ProjectSavedDir(), RelativePath, FileName);
	FPaths::NormalizeFilename(OutputPath);
	return OutputPath;
}

FString UWhisperSpeechSubsystem::GetDefaultLanguage() const
{
	FString Language;
	ReadWhisperConfigString(DefaultLanguageKey, Language);

	Language.TrimStartAndEndInline();
	return Language.IsEmpty() ? DefaultLanguage : Language;
}

int32 UWhisperSpeechSubsystem::GetDefaultThreadCount() const
{
	int32 ThreadCount = 8;
	ReadWhisperConfigInt(DefaultThreadCountKey, ThreadCount);

	return FMath::Max(1, ThreadCount);
}

bool UWhisperSpeechSubsystem::CanStartTranscription(FString& OutErrorMessage) const
{
	if (bTranscriptionInProgress)
	{
		OutErrorMessage = TEXT("Whisper transcription is already in progress.");
		return false;
	}

	if (bModelLoadInProgress)
	{
		OutErrorMessage = TEXT("Whisper model is still loading.");
		return false;
	}

	if (!RuntimeWrapper.IsValid() || !RuntimeWrapper->IsModelLoaded())
	{
		OutErrorMessage = TEXT("Whisper model is not loaded. Run WhisperRuntime.LoadModel first.");
		return false;
	}

	return true;
}

bool UWhisperSpeechSubsystem::StopRecordingInternal(bool bSaveWav, FString& OutSavedWavPath, FString& OutErrorMessage)
{
	if (!MicRecorder.IsValid())
	{
		OutErrorMessage = TEXT("Microphone recorder is not initialized.");
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimedRecordingTimerHandle);
	}

	if (!MicRecorder->StopRecording(OutErrorMessage))
	{
		return false;
	}

	if (bSaveWav)
	{
		OutSavedWavPath = BuildRecordedWavPath();
		if (!MicRecorder->SaveRecordedWav(OutSavedWavPath, OutErrorMessage))
		{
			return false;
		}

		LastRecordedWavPath = OutSavedWavPath;
	}

	OnRecordingStopped.Broadcast();
	return true;
}

void UWhisperSpeechSubsystem::TranscribeSamplesAsync(
	const TArray<float>& Samples,
	const FString& SourceAudioPath,
	const FString& Language,
	int32 ThreadCount)
{
	FString ErrorMessage;
	if (!CanStartTranscription(ErrorMessage))
	{
		BroadcastFailure(ErrorMessage);
		return;
	}

	TSharedPtr<FWhisperRuntimeWrapper, ESPMode::ThreadSafe> Wrapper = RuntimeWrapper;
	const FString LanguageToUse = Language.IsEmpty() ? GetDefaultLanguage() : Language;
	const int32 ThreadCountToUse = ThreadCount > 0 ? ThreadCount : GetDefaultThreadCount();

	bTranscriptionInProgress = true;
	UE_LOG(LogWhisperSpeechSubsystem, Log, TEXT("Whisper sample transcription started. Source: %s, Language: %s, Threads: %d, Samples: %d"),
		*SourceAudioPath,
		*LanguageToUse,
		ThreadCountToUse,
		Samples.Num());

	TWeakObjectPtr<UWhisperSpeechSubsystem> WeakThis(this);
	Async(EAsyncExecution::ThreadPool, [WeakThis, Wrapper, Samples, SourceAudioPath, LanguageToUse, ThreadCountToUse]()
	{
		FString ErrorMessage;
		FWhisperTranscriptionResult Result;
		const bool bSucceeded = Wrapper->TranscribeSamples(Samples, LanguageToUse, ThreadCountToUse, Result, ErrorMessage);
		Result.SourceAudioPath = SourceAudioPath;

		AsyncTask(ENamedThreads::GameThread, [WeakThis, Result, ErrorMessage, bSucceeded]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			UWhisperSpeechSubsystem* Subsystem = WeakThis.Get();
			Subsystem->bTranscriptionInProgress = false;

			if (!bSucceeded)
			{
				Subsystem->BroadcastFailure(ErrorMessage);
				return;
			}

			UE_LOG(LogWhisperSpeechSubsystem, Log, TEXT("Whisper transcription completed. Text: %s"), *Result.Text);
			Subsystem->OnTranscriptionCompleted.Broadcast(Result);
		});
	});
}

void UWhisperSpeechSubsystem::BroadcastFailure(const FString& ErrorMessage)
{
	const FString MessageToUse = ErrorMessage.IsEmpty() ? TEXT("Unknown Whisper error.") : ErrorMessage;
	UE_LOG(LogWhisperSpeechSubsystem, Warning, TEXT("%s"), *MessageToUse);
	OnTranscriptionFailed.Broadcast(MessageToUse);
}
