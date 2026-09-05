#include "MicRecorder.h"

#include "HAL/CriticalSection.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

#if WHISPERRUNTIME_WITH_WHISPER
THIRD_PARTY_INCLUDES_START
#include "miniaudio.h"
THIRD_PARTY_INCLUDES_END
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMicRecorder, Log, All);

namespace
{
	constexpr int32 WhisperMicSampleRate = 16000;
	constexpr int32 WhisperMicChannels = 1;

	void AppendFourCc(TArray<uint8>& Bytes, const char* FourCc)
	{
		Bytes.Add(static_cast<uint8>(FourCc[0]));
		Bytes.Add(static_cast<uint8>(FourCc[1]));
		Bytes.Add(static_cast<uint8>(FourCc[2]));
		Bytes.Add(static_cast<uint8>(FourCc[3]));
	}

	void AppendUInt16Le(TArray<uint8>& Bytes, uint16 Value)
	{
		Bytes.Add(static_cast<uint8>(Value & 0xFF));
		Bytes.Add(static_cast<uint8>((Value >> 8) & 0xFF));
	}

	void AppendUInt32Le(TArray<uint8>& Bytes, uint32 Value)
	{
		Bytes.Add(static_cast<uint8>(Value & 0xFF));
		Bytes.Add(static_cast<uint8>((Value >> 8) & 0xFF));
		Bytes.Add(static_cast<uint8>((Value >> 16) & 0xFF));
		Bytes.Add(static_cast<uint8>((Value >> 24) & 0xFF));
	}

	bool WriteFloatMonoSamplesToPcm16Wav(
		const FString& OutputPath,
		const TArray<float>& Samples,
		int32 SampleRate,
		int32 NumChannels,
		FString& OutErrorMessage)
	{
		if (OutputPath.IsEmpty())
		{
			OutErrorMessage = TEXT("Output WAV path is empty.");
			return false;
		}

		if (Samples.IsEmpty())
		{
			OutErrorMessage = TEXT("Cannot save WAV because recorded sample buffer is empty.");
			return false;
		}

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);

		const uint32 DataSize = static_cast<uint32>(Samples.Num() * sizeof(int16));
		const uint32 RiffSize = 36 + DataSize;
		const uint16 AudioFormatPcm = 1;
		const uint16 BitsPerSample = 16;
		const uint16 BlockAlign = static_cast<uint16>(NumChannels * BitsPerSample / 8);
		const uint32 ByteRate = SampleRate * BlockAlign;

		TArray<uint8> Bytes;
		Bytes.Reserve(44 + DataSize);

		AppendFourCc(Bytes, "RIFF");
		AppendUInt32Le(Bytes, RiffSize);
		AppendFourCc(Bytes, "WAVE");
		AppendFourCc(Bytes, "fmt ");
		AppendUInt32Le(Bytes, 16);
		AppendUInt16Le(Bytes, AudioFormatPcm);
		AppendUInt16Le(Bytes, static_cast<uint16>(NumChannels));
		AppendUInt32Le(Bytes, static_cast<uint32>(SampleRate));
		AppendUInt32Le(Bytes, ByteRate);
		AppendUInt16Le(Bytes, BlockAlign);
		AppendUInt16Le(Bytes, BitsPerSample);
		AppendFourCc(Bytes, "data");
		AppendUInt32Le(Bytes, DataSize);

		for (float Sample : Samples)
		{
			const float ClampedSample = FMath::Clamp(Sample, -1.0f, 1.0f);
			const int16 PcmSample = static_cast<int16>(FMath::RoundToInt(ClampedSample * 32767.0f));
			AppendUInt16Le(Bytes, static_cast<uint16>(PcmSample));
		}

		if (!FFileHelper::SaveArrayToFile(Bytes, *OutputPath))
		{
			OutErrorMessage = FString::Printf(TEXT("Failed to save recorded WAV file: %s"), *OutputPath);
			return false;
		}

		return true;
	}
}

struct FMicRecorder::FImpl
{
#if WHISPERRUNTIME_WITH_WHISPER
	ma_device Device;
	bool bDeviceInitialized = false;
#endif
	mutable FCriticalSection SamplesCriticalSection;
	TArray<float> Samples;
	bool bRecording = false;
	int32 SampleRate = WhisperMicSampleRate;
	int32 NumChannels = WhisperMicChannels;

#if WHISPERRUNTIME_WITH_WHISPER
	static void DataCallback(ma_device* Device, void* Output, const void* Input, ma_uint32 FrameCount)
	{
		(void)Output;

		if (!Device || !Device->pUserData || !Input || FrameCount == 0)
		{
			return;
		}

		FImpl* Recorder = static_cast<FImpl*>(Device->pUserData);
		const float* InputSamples = static_cast<const float*>(Input);
		const int32 IncomingSampleCount = static_cast<int32>(FrameCount) * Recorder->NumChannels;

		FScopeLock Lock(&Recorder->SamplesCriticalSection);
		Recorder->Samples.Append(InputSamples, IncomingSampleCount);
	}
#endif
};

FMicRecorder::FMicRecorder()
	: Impl(MakeUnique<FImpl>())
{
}

FMicRecorder::~FMicRecorder()
{
	FString ErrorMessage;
	StopRecording(ErrorMessage);
}

bool FMicRecorder::StartRecording(FString& OutErrorMessage)
{
#if WHISPERRUNTIME_WITH_WHISPER
	if (Impl->bRecording)
	{
		OutErrorMessage = TEXT("Microphone recording is already in progress.");
		return false;
	}

	Reset();

	ma_device_config Config = ma_device_config_init(ma_device_type_capture);
	Config.capture.format = ma_format_f32;
	Config.capture.channels = Impl->NumChannels;
	Config.sampleRate = Impl->SampleRate;
	Config.dataCallback = FImpl::DataCallback;
	Config.pUserData = Impl.Get();

	ma_result Result = ma_device_init(nullptr, &Config, &Impl->Device);
	if (Result != MA_SUCCESS)
	{
		OutErrorMessage = FString::Printf(TEXT("Microphone device initialization failed. miniaudio result: %d"), static_cast<int32>(Result));
		return false;
	}

	Impl->bDeviceInitialized = true;

	Result = ma_device_start(&Impl->Device);
	if (Result != MA_SUCCESS)
	{
		ma_device_uninit(&Impl->Device);
		Impl->bDeviceInitialized = false;
		OutErrorMessage = FString::Printf(TEXT("Microphone recording start failed. miniaudio result: %d"), static_cast<int32>(Result));
		return false;
	}

	Impl->bRecording = true;
	UE_LOG(LogMicRecorder, Log, TEXT("Microphone recording started. SampleRate: %d, Channels: %d"), Impl->SampleRate, Impl->NumChannels);
	return true;
#else
	OutErrorMessage = TEXT("Microphone recording is not included in this build.");
	return false;
#endif
}

bool FMicRecorder::StopRecording(FString& OutErrorMessage)
{
#if WHISPERRUNTIME_WITH_WHISPER
	if (!Impl->bRecording && !Impl->bDeviceInitialized)
	{
		return true;
	}

	if (Impl->bDeviceInitialized)
	{
		if (Impl->bRecording)
		{
			ma_result Result = ma_device_stop(&Impl->Device);
			if (Result != MA_SUCCESS)
			{
				OutErrorMessage = FString::Printf(TEXT("Microphone recording stop returned miniaudio result: %d"), static_cast<int32>(Result));
			}
		}

		ma_device_uninit(&Impl->Device);
		Impl->bDeviceInitialized = false;
	}

	Impl->bRecording = false;
	UE_LOG(LogMicRecorder, Log, TEXT("Microphone recording stopped. Samples: %d"), GetSampleCount());
	return OutErrorMessage.IsEmpty();
#else
	return true;
#endif
}

bool FMicRecorder::IsRecording() const
{
	return Impl->bRecording;
}

bool FMicRecorder::GetRecordedSamples(TArray<float>& OutSamples) const
{
	FScopeLock Lock(&Impl->SamplesCriticalSection);
	OutSamples = Impl->Samples;
	return !OutSamples.IsEmpty();
}

bool FMicRecorder::SaveRecordedWav(const FString& OutputPath, FString& OutErrorMessage) const
{
	TArray<float> SamplesCopy;
	GetRecordedSamples(SamplesCopy);
	return WriteFloatMonoSamplesToPcm16Wav(OutputPath, SamplesCopy, Impl->SampleRate, Impl->NumChannels, OutErrorMessage);
}

void FMicRecorder::Reset()
{
	FScopeLock Lock(&Impl->SamplesCriticalSection);
	Impl->Samples.Reset();
}

int32 FMicRecorder::GetSampleRate() const
{
	return Impl->SampleRate;
}

int32 FMicRecorder::GetNumChannels() const
{
	return Impl->NumChannels;
}

int32 FMicRecorder::GetSampleCount() const
{
	FScopeLock Lock(&Impl->SamplesCriticalSection);
	return Impl->Samples.Num();
}
