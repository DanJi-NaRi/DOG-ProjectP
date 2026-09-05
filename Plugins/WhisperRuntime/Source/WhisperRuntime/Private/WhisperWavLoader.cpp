#include "WhisperWavLoader.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	bool ReadFourCc(const TArray<uint8>& Bytes, int64 Offset, const char* FourCc)
	{
		if (Offset < 0 || Offset + 4 > Bytes.Num())
		{
			return false;
		}

		return Bytes[Offset + 0] == static_cast<uint8>(FourCc[0])
			&& Bytes[Offset + 1] == static_cast<uint8>(FourCc[1])
			&& Bytes[Offset + 2] == static_cast<uint8>(FourCc[2])
			&& Bytes[Offset + 3] == static_cast<uint8>(FourCc[3]);
	}

	uint16 ReadUInt16Le(const TArray<uint8>& Bytes, int64 Offset)
	{
		return static_cast<uint16>(Bytes[Offset])
			| static_cast<uint16>(Bytes[Offset + 1] << 8);
	}

	uint32 ReadUInt32Le(const TArray<uint8>& Bytes, int64 Offset)
	{
		return static_cast<uint32>(Bytes[Offset])
			| (static_cast<uint32>(Bytes[Offset + 1]) << 8)
			| (static_cast<uint32>(Bytes[Offset + 2]) << 16)
			| (static_cast<uint32>(Bytes[Offset + 3]) << 24);
	}

	int16 ReadInt16Le(const TArray<uint8>& Bytes, int64 Offset)
	{
		const uint16 UnsignedValue = ReadUInt16Le(Bytes, Offset);
		return static_cast<int16>(UnsignedValue);
	}
}

bool FWhisperWavLoader::LoadPcm16AsFloatMono(const FString& WavPath, FWhisperWavData& OutWavData, FString& OutErrorMessage)
{
	OutWavData = FWhisperWavData();

	if (WavPath.IsEmpty())
	{
		OutErrorMessage = TEXT("WAV path is empty.");
		return false;
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *WavPath))
	{
		OutErrorMessage = FString::Printf(TEXT("Failed to load WAV file: %s"), *WavPath);
		return false;
	}

	if (Bytes.Num() < 12 || !ReadFourCc(Bytes, 0, "RIFF") || !ReadFourCc(Bytes, 8, "WAVE"))
	{
		OutErrorMessage = FString::Printf(TEXT("File is not a RIFF/WAVE file: %s"), *WavPath);
		return false;
	}

	bool bFoundFormat = false;
	bool bFoundData = false;
	uint16 AudioFormat = 0;
	uint16 NumChannels = 0;
	uint32 SampleRate = 0;
	uint16 BitsPerSample = 0;
	int64 DataOffset = 0;
	uint32 DataSize = 0;

	int64 Offset = 12;
	while (Offset + 8 <= Bytes.Num())
	{
		const int64 ChunkHeaderOffset = Offset;
		const uint32 ChunkSize = ReadUInt32Le(Bytes, ChunkHeaderOffset + 4);
		const int64 ChunkDataOffset = ChunkHeaderOffset + 8;
		const int64 ChunkEndOffset = ChunkDataOffset + ChunkSize;

		if (ChunkEndOffset > Bytes.Num())
		{
			OutErrorMessage = FString::Printf(TEXT("Invalid WAV chunk size in file: %s"), *WavPath);
			return false;
		}

		if (ReadFourCc(Bytes, ChunkHeaderOffset, "fmt "))
		{
			if (ChunkSize < 16)
			{
				OutErrorMessage = FString::Printf(TEXT("Invalid WAV fmt chunk in file: %s"), *WavPath);
				return false;
			}

			AudioFormat = ReadUInt16Le(Bytes, ChunkDataOffset + 0);
			NumChannels = ReadUInt16Le(Bytes, ChunkDataOffset + 2);
			SampleRate = ReadUInt32Le(Bytes, ChunkDataOffset + 4);
			BitsPerSample = ReadUInt16Le(Bytes, ChunkDataOffset + 14);
			bFoundFormat = true;
		}
		else if (ReadFourCc(Bytes, ChunkHeaderOffset, "data"))
		{
			DataOffset = ChunkDataOffset;
			DataSize = ChunkSize;
			bFoundData = true;
		}

		Offset = ChunkEndOffset + (ChunkSize & 1);
	}

	if (!bFoundFormat || !bFoundData)
	{
		OutErrorMessage = FString::Printf(TEXT("WAV file is missing fmt or data chunk: %s"), *WavPath);
		return false;
	}

	if (AudioFormat != 1)
	{
		OutErrorMessage = FString::Printf(TEXT("Only PCM WAV is supported. AudioFormat: %u"), AudioFormat);
		return false;
	}

	if (BitsPerSample != 16)
	{
		OutErrorMessage = FString::Printf(TEXT("Only 16-bit PCM WAV is supported. BitsPerSample: %u"), BitsPerSample);
		return false;
	}

	if (SampleRate != 16000)
	{
		OutErrorMessage = FString::Printf(TEXT("Whisper expects 16 kHz WAV input. Current sample rate: %u"), SampleRate);
		return false;
	}

	if (NumChannels != 1 && NumChannels != 2)
	{
		OutErrorMessage = FString::Printf(TEXT("Only mono or stereo WAV is supported. Channels: %u"), NumChannels);
		return false;
	}

	if (DataSize < sizeof(int16) * NumChannels)
	{
		OutErrorMessage = FString::Printf(TEXT("WAV data chunk is empty: %s"), *WavPath);
		return false;
	}

	OutWavData.SampleRate = static_cast<int32>(SampleRate);
	OutWavData.NumChannels = static_cast<int32>(NumChannels);

	const int64 FrameCount = DataSize / (sizeof(int16) * NumChannels);
	OutWavData.Samples.Reserve(static_cast<int32>(FrameCount));

	for (int64 FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex)
	{
		const int64 SampleOffset = DataOffset + FrameIndex * sizeof(int16) * NumChannels;

		if (NumChannels == 1)
		{
			const float MonoSample = static_cast<float>(ReadInt16Le(Bytes, SampleOffset)) / 32768.0f;
			OutWavData.Samples.Add(MonoSample);
		}
		else
		{
			const float Left = static_cast<float>(ReadInt16Le(Bytes, SampleOffset + 0)) / 32768.0f;
			const float Right = static_cast<float>(ReadInt16Le(Bytes, SampleOffset + sizeof(int16))) / 32768.0f;
			OutWavData.Samples.Add((Left + Right) * 0.5f);
		}
	}

	if (OutWavData.Samples.IsEmpty())
	{
		OutErrorMessage = FString::Printf(TEXT("No audio samples were decoded from WAV file: %s"), *WavPath);
		return false;
	}

	return true;
}

