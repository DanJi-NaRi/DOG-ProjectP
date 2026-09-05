#include "CPP_BossAttackData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UAnimMontage* UCPP_BossAttackData::GetAttackMontage() const
{
	return AttackMontage;
}

TSubclassOf<ACPP_BossTelegraphActor> UCPP_BossAttackData::GetTelegraphActorClass() const
{
	return TelegraphActorClass;
}

const TArray<FBossAttackWindowData>& UCPP_BossAttackData::GetAttackWindows() const
{
	return AttackWindows;
}

////////////////////////////
//! \author HanSeul
//! \brief Finds the attack window data that matches the supplied local window identifier.
//! \param WindowId Local identifier sent by the montage notify.
//! \return Matching attack window data, or nullptr when no match exists.
const FBossAttackWindowData* UCPP_BossAttackData::FindAttackWindow(FName WindowId) const
{
	if (WindowId.IsNone())
	{
		return nullptr;
	}

	return AttackWindows.FindByPredicate(
		[WindowId](const FBossAttackWindowData& AttackWindow)
		{
			return AttackWindow.WindowId == WindowId;
		}
	);
}

#if WITH_EDITOR
EDataValidationResult UCPP_BossAttackData::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	bool bHasError = SuperResult == EDataValidationResult::Invalid;

	if (AttackWindows.IsEmpty())
	{
		Context.AddError(FText::FromString(TEXT("AttackWindows must contain at least one window.")));
		bHasError = true;
	}

	TSet<FName> UsedWindowIds;
	for (int32 WindowIndex = 0; WindowIndex < AttackWindows.Num(); ++WindowIndex)
	{
		const FBossAttackWindowData& AttackWindow = AttackWindows[WindowIndex];
		const FString WindowLabel = FString::Printf(TEXT("AttackWindows[%d]"), WindowIndex);

		if (AttackWindow.WindowId.IsNone())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s WindowId must not be None."), *WindowLabel)));
			bHasError = true;
		}
		else if (UsedWindowIds.Contains(AttackWindow.WindowId))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s duplicates WindowId '%s'."), *WindowLabel, *AttackWindow.WindowId.ToString())));
			bHasError = true;
		}
		else
		{
			UsedWindowIds.Add(AttackWindow.WindowId);
		}

		if (AttackWindow.HitShapes.IsEmpty())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s must contain at least one HitShape."), *WindowLabel)));
			bHasError = true;
		}

		if (AttackWindow.DamageCoefficient < 0.0f)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s DamageCoefficient must be zero or greater."), *WindowLabel)));
			bHasError = true;
		}

		if (AttackWindow.CurseGaugeAmount < 0.0f || AttackWindow.CurseGaugeAmount > 100.0f)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s CurseGaugeAmount must be between 0 and 100."), *WindowLabel)));
			bHasError = true;
		}

		for (int32 ShapeIndex = 0; ShapeIndex < AttackWindow.HitShapes.Num(); ++ShapeIndex)
		{
			const FBossHitShapeData& HitShape = AttackWindow.HitShapes[ShapeIndex];
			const FString ShapeLabel = FString::Printf(TEXT("%s.HitShapes[%d]"), *WindowLabel, ShapeIndex);

			if (HitShape.HalfHeight <= 0.0f)
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("%s HalfHeight must be greater than zero."), *ShapeLabel)));
				bHasError = true;
			}

			switch (HitShape.Shape)
			{
			case EBossAttackShape::Circle:
			case EBossAttackShape::Sector:
				if (HitShape.InnerRadius < 0.0f || HitShape.OuterRadius <= 0.0f || HitShape.InnerRadius >= HitShape.OuterRadius)
				{
					Context.AddError(FText::FromString(FString::Printf(TEXT("%s requires 0 <= InnerRadius < OuterRadius."), *ShapeLabel)));
					bHasError = true;
				}

				if (HitShape.Shape == EBossAttackShape::Sector
					&& (HitShape.SectorAngleDegrees <= 0.0f || HitShape.SectorAngleDegrees > 360.0f))
				{
					Context.AddError(FText::FromString(FString::Printf(TEXT("%s SectorAngleDegrees must be greater than 0 and no greater than 360."), *ShapeLabel)));
					bHasError = true;
				}
				break;

			case EBossAttackShape::Rectangle:
				if (HitShape.ForwardLength <= 0.0f || HitShape.HalfWidth <= 0.0f)
				{
					Context.AddError(FText::FromString(FString::Printf(TEXT("%s ForwardLength and HalfWidth must be greater than zero."), *ShapeLabel)));
					bHasError = true;
				}
				break;

			default:
				Context.AddError(FText::FromString(FString::Printf(TEXT("%s has an unsupported Shape value."), *ShapeLabel)));
				bHasError = true;
				break;
			}
		}
	}

	return bHasError ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
