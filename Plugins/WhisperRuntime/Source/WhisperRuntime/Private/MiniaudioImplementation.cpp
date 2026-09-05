#include "CoreMinimal.h"

#if WHISPERRUNTIME_WITH_WHISPER
THIRD_PARTY_INCLUDES_START
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
THIRD_PARTY_INCLUDES_END
#endif
