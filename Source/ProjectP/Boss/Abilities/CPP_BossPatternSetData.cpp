#include "CPP_BossPatternSetData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

////////////////////////////
//! \author HanSeul
//! \brief Returns the boss pattern entries configured for the requested boss phase.
//! \param Phase Boss phase whose pattern list should be returned.
//! \return Pattern entries for the requested phase, or an empty list for unsupported phases.
const TArray<FBossPatternEntry>& UCPP_BossPatternSetData::GetPatternsForPhase(EBossPhase Phase) const
{
	static const TArray<FBossPatternEntry> EmptyPatterns;

	switch (Phase)
	{
	case EBossPhase::Phase1:
		return Phase1Patterns;
	case EBossPhase::Phase2:
		return Phase2Patterns;
	case EBossPhase::None:
	case EBossPhase::Transition:
	default:
		return EmptyPatterns;
	}
}

#if WITH_EDITOR
EDataValidationResult UCPP_BossPatternSetData::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	bool bHasError = SuperResult == EDataValidationResult::Invalid;

	auto ValidatePatternEntries = [&Context, &bHasError](const TArray<FBossPatternEntry>& PatternEntries, const FString& ListName)
	{
		for (int32 PatternIndex = 0; PatternIndex < PatternEntries.Num(); ++PatternIndex)
		{
			const FBossPatternEntry& PatternEntry = PatternEntries[PatternIndex];
			if (!PatternEntry.AbilityClass)
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("%s[%d] AbilityClass must not be None."), *ListName, PatternIndex)));
				bHasError = true;
			}
		}
	};

	ValidatePatternEntries(Phase1Patterns, TEXT("Phase1Patterns"));
	ValidatePatternEntries(Phase2Patterns, TEXT("Phase2Patterns"));

	return bHasError ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
