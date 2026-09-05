////////////////////////////
//! \file MySkillDebugShape.cpp
//! \brief 스킬 판정 디버그 도형 생성과 소유 클라이언트 표시 유틸 구현 파일이다.

#include "MySkillDebugShape.h"

#include "../MyPlayerController.h"
#include "../Player/Components/MySkillControlComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

////////////////////////////
//! \author HanUl
//! \brief 선분 디버그 도형을 생성한다.
//! \param InStart 시작점
//! \param InEnd 끝점
//! \param InColor 표시 색상
//! \param InDuration 표시 유지 시간
//! \param InThickness 선 두께
//! \return 생성된 디버그 도형
FMySkillDebugShape FMySkillDebugShape::MakeLine(const FVector& InStart, const FVector& InEnd, const FColor& InColor, float InDuration, float InThickness)
{
	FMySkillDebugShape Shape;
	Shape.Type = EMySkillDebugShapeType::Line;
	Shape.Origin = InStart;
	Shape.EndPoint = InEnd;
	Shape.Color = InColor;
	Shape.Duration = InDuration;
	Shape.Thickness = InThickness;
	return Shape;
}

////////////////////////////
//! \author HanUl
//! \brief 수평 원 디버그 도형을 생성한다.
//! \param InOrigin 원 중심
//! \param InRadius 반경
//! \param InColor 표시 색상
//! \param InDuration 표시 유지 시간
//! \param InThickness 선 두께
//! \return 생성된 디버그 도형
FMySkillDebugShape FMySkillDebugShape::MakeCircle(const FVector& InOrigin, float InRadius, const FColor& InColor, float InDuration, float InThickness)
{
	FMySkillDebugShape Shape;
	Shape.Type = EMySkillDebugShapeType::Circle;
	Shape.Origin = InOrigin;
	Shape.Radius = InRadius;
	Shape.Color = InColor;
	Shape.Duration = InDuration;
	Shape.Thickness = InThickness;
	return Shape;
}

////////////////////////////
//! \author HanUl
//! \brief 부채꼴(양측 경계선 + 바깥 호) 디버그 도형을 생성한다.
//! \param InOrigin 부채꼴 원점
//! \param InDirection 중심 방향
//! \param InRadius 반경
//! \param InAngleDegrees 전체 각도
//! \param InColor 표시 색상
//! \param InDuration 표시 유지 시간
//! \param InThickness 선 두께
//! \return 생성된 디버그 도형
FMySkillDebugShape FMySkillDebugShape::MakeCone(const FVector& InOrigin, const FVector& InDirection, float InRadius, float InAngleDegrees, const FColor& InColor, float InDuration, float InThickness)
{
	FMySkillDebugShape Shape;
	Shape.Type = EMySkillDebugShapeType::Cone;
	Shape.Origin = InOrigin;
	Shape.Direction = InDirection;
	Shape.Radius = InRadius;
	Shape.AngleDegrees = InAngleDegrees;
	Shape.Color = InColor;
	Shape.Duration = InDuration;
	Shape.Thickness = InThickness;
	return Shape;
}

////////////////////////////
//! \author HanUl
//! \brief 방향 정렬 박스 디버그 도형을 생성한다.
//! \param InCenter 박스 중심
//! \param InExtent 박스 반크기
//! \param InDirection 박스 전방 방향
//! \param InColor 표시 색상
//! \param InDuration 표시 유지 시간
//! \param InThickness 선 두께
//! \return 생성된 디버그 도형
FMySkillDebugShape FMySkillDebugShape::MakeBox(const FVector& InCenter, const FVector& InExtent, const FVector& InDirection, const FColor& InColor, float InDuration, float InThickness)
{
	FMySkillDebugShape Shape;
	Shape.Type = EMySkillDebugShapeType::Box;
	Shape.Origin = InCenter;
	Shape.Extent = InExtent;
	Shape.Direction = InDirection;
	Shape.Color = InColor;
	Shape.Duration = InDuration;
	Shape.Thickness = InThickness;
	return Shape;
}

////////////////////////////
//! \author HanUl
//! \brief 구 디버그 도형을 생성한다.
//! \param InOrigin 구 중심
//! \param InRadius 반경
//! \param InColor 표시 색상
//! \param InDuration 표시 유지 시간
//! \param InThickness 선 두께
//! \return 생성된 디버그 도형
FMySkillDebugShape FMySkillDebugShape::MakeSphere(const FVector& InOrigin, float InRadius, const FColor& InColor, float InDuration, float InThickness)
{
	FMySkillDebugShape Shape;
	Shape.Type = EMySkillDebugShapeType::Sphere;
	Shape.Origin = InOrigin;
	Shape.Radius = InRadius;
	Shape.Color = InColor;
	Shape.Duration = InDuration;
	Shape.Thickness = InThickness;
	return Shape;
}

namespace MySkillDebugDraw
{
	////////////////////////////
	//! \author HanUl
	//! \brief 디버그 도형을 지정 월드에 즉시 그린다. Shipping/Test 빌드에서는 아무것도 하지 않는다.
	//! \param World 그릴 대상 월드
	//! \param Shape 표시할 디버그 도형
	//! \return 없음
	void DrawShape(const UWorld* World, const FMySkillDebugShape& Shape)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (!World)
		{
			return;
		}

		const AMyPlayerController* LocalPlayerController = nullptr;
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			const AMyPlayerController* Candidate = Cast<AMyPlayerController>(Iterator->Get());
			if (Candidate && Candidate->IsLocalPlayerController())
			{
				LocalPlayerController = Candidate;
				break;
			}
		}

		if (!LocalPlayerController || !LocalPlayerController->IsSkillDebugLineEnabled())
		{
			return;
		}

		switch (Shape.Type)
		{
		case EMySkillDebugShapeType::Line:
			DrawDebugLine(World, Shape.Origin, Shape.EndPoint, Shape.Color, false, Shape.Duration, 0, Shape.Thickness);
			break;

		case EMySkillDebugShapeType::Circle:
			DrawDebugCircle(World, Shape.Origin, Shape.Radius, 32, Shape.Color, false, Shape.Duration, 0, Shape.Thickness,
				FVector(1, 0, 0), FVector(0, 1, 0), false);
			break;

		case EMySkillDebugShapeType::Cone:
		{
			const FVector Direction2D = Shape.Direction.GetSafeNormal2D();
			if (Direction2D.IsNearlyZero() || Shape.Radius <= 0.0f)
			{
				break;
			}

			const float CenterYaw = Direction2D.Rotation().Yaw;
			const float HalfAngle = Shape.AngleDegrees * 0.5f;
			const FVector LeftEdgeDirection = FRotator(0.0f, CenterYaw - HalfAngle, 0.0f).Vector();
			const FVector RightEdgeDirection = FRotator(0.0f, CenterYaw + HalfAngle, 0.0f).Vector();
			DrawDebugLine(World, Shape.Origin, Shape.Origin + LeftEdgeDirection * Shape.Radius, Shape.Color, false, Shape.Duration, 0, Shape.Thickness);
			DrawDebugLine(World, Shape.Origin, Shape.Origin + RightEdgeDirection * Shape.Radius, Shape.Color, false, Shape.Duration, 0, Shape.Thickness);

			// 바깥 호를 짧은 선분으로 근사한다
			constexpr int32 ArcSegmentCount = 16;
			FVector PreviousArcPoint = Shape.Origin + LeftEdgeDirection * Shape.Radius;
			for (int32 SegmentIndex = 1; SegmentIndex <= ArcSegmentCount; ++SegmentIndex)
			{
				const float SegmentYaw = CenterYaw - HalfAngle + (Shape.AngleDegrees * SegmentIndex / ArcSegmentCount);
				const FVector ArcPoint = Shape.Origin + FRotator(0.0f, SegmentYaw, 0.0f).Vector() * Shape.Radius;
				DrawDebugLine(World, PreviousArcPoint, ArcPoint, Shape.Color, false, Shape.Duration, 0, Shape.Thickness);
				PreviousArcPoint = ArcPoint;
			}
			break;
		}

		case EMySkillDebugShapeType::Box:
			DrawDebugBox(World, Shape.Origin, Shape.Extent, Shape.Direction.GetSafeNormal2D().ToOrientationQuat(), Shape.Color, false, Shape.Duration, 0, Shape.Thickness);
			break;

		case EMySkillDebugShapeType::Sphere:
			DrawDebugSphere(World, Shape.Origin, Shape.Radius, 32, Shape.Color, false, Shape.Duration, 0, Shape.Thickness);
			break;

		default:
			break;
		}
#endif
	}

	////////////////////////////
	//! \author HanUl
	//! \brief 디버그 도형을 현재 프로세스에서 로컬 조종 중인 스킬 소유자 화면에만 표시한다.
	//!        서버의 원격 Pawn에서는 그리거나 Client RPC를 전송하지 않는다.
	//! \param AvatarActor 스킬을 수행한 아바타
	//! \param Shape 표시할 디버그 도형
	//! \return 없음
	void DrawShapeForLocalOwner(AActor* AvatarActor, const FMySkillDebugShape& Shape)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
		if (!AvatarPawn || !AvatarPawn->IsLocallyControlled())
		{
			return;
		}

		DrawShape(AvatarActor->GetWorld(), Shape);
#endif
	}

	////////////////////////////
	//! \author HanUl
	//! \brief 디버그 도형을 스킬 소유자 화면에만 표시한다.
	//!        로컬 조종 아바타(리슨 호스트 본인, 소유 클라이언트)는 즉시 그리고,
	//!        서버 권위에서 원격 소유 아바타면 Client RPC로 소유 클라이언트에 전송한다.
	//! \param AvatarActor 스킬을 수행한 아바타
	//! \param Shape 표시할 디버그 도형
	//! \return 없음
	void DrawShapeForOwner(AActor* AvatarActor, const FMySkillDebugShape& Shape)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (!AvatarActor)
		{
			return;
		}

		const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
		if (AvatarPawn && AvatarPawn->IsLocallyControlled())
		{
			DrawShape(AvatarActor->GetWorld(), Shape);
			return;
		}

		if (AvatarActor->HasAuthority())
		{
			if (UMySkillControlComponent* SkillControlComponent = AvatarActor->FindComponentByClass<UMySkillControlComponent>())
			{
				SkillControlComponent->ClientDrawSkillDebugShape(Shape);
			}
		}
#endif
	}
}
