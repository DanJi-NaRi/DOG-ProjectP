////////////////////////////
//! \file MySkillDebugShape.h
//! \brief 스킬 판정 디버그 도형 정의와 소유 클라이언트 표시 유틸 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "MySkillDebugShape.generated.h"

class AActor;

UENUM()
enum class EMySkillDebugShapeType : uint8
{
	Line,
	Circle,
	Cone,
	Box,
	Sphere
};

////////////////////////////
//! \struct FMySkillDebugShape
//! \author HanUl
//! \brief 스킬 판정 범위를 소유 클라이언트 화면에 표시하기 위한 디버그 도형 데이터다.
//!        서버 판정 코드가 도형을 만들어 DrawShapeForOwner로 넘기면 소유자에게만 표시된다.
USTRUCT()
struct PROJECTP_API FMySkillDebugShape
{
	GENERATED_BODY()

	UPROPERTY()
	EMySkillDebugShapeType Type = EMySkillDebugShapeType::Line;

	//! \brief 도형 기준점. Line은 시작점, Box는 중심
	UPROPERTY()
	FVector Origin = FVector::ZeroVector;

	//! \brief Cone/Box의 전방 방향
	UPROPERTY()
	FVector Direction = FVector::ForwardVector;

	//! \brief Line의 끝점
	UPROPERTY()
	FVector EndPoint = FVector::ZeroVector;

	//! \brief Box의 반크기
	UPROPERTY()
	FVector Extent = FVector::ZeroVector;

	//! \brief Circle/Cone/Sphere의 반경
	UPROPERTY()
	float Radius = 0.0f;

	//! \brief Cone의 전체 각도
	UPROPERTY()
	float AngleDegrees = 0.0f;

	UPROPERTY()
	FColor Color = FColor::Red;

	UPROPERTY()
	float Duration = 1.0f;

	UPROPERTY()
	float Thickness = 2.0f;

	static FMySkillDebugShape MakeLine(const FVector& InStart, const FVector& InEnd, const FColor& InColor, float InDuration, float InThickness = 2.0f);
	static FMySkillDebugShape MakeCircle(const FVector& InOrigin, float InRadius, const FColor& InColor, float InDuration, float InThickness = 2.0f);
	static FMySkillDebugShape MakeCone(const FVector& InOrigin, const FVector& InDirection, float InRadius, float InAngleDegrees, const FColor& InColor, float InDuration, float InThickness = 2.0f);
	static FMySkillDebugShape MakeBox(const FVector& InCenter, const FVector& InExtent, const FVector& InDirection, const FColor& InColor, float InDuration, float InThickness = 2.0f);
	static FMySkillDebugShape MakeSphere(const FVector& InOrigin, float InRadius, const FColor& InColor, float InDuration, float InThickness = 1.0f);
};

namespace MySkillDebugDraw
{
	//! \brief 도형을 지정 월드에 즉시 그린다.
	PROJECTP_API void DrawShape(const UWorld* World, const FMySkillDebugShape& Shape);

	//! \brief 도형을 현재 프로세스에서 로컬 조종 중인 스킬 소유자 화면에만 표시한다.
	//!        서버에서 원격 소유 클라이언트로 전송하지 않는다.
	PROJECTP_API void DrawShapeForLocalOwner(AActor* AvatarActor, const FMySkillDebugShape& Shape);

	//! \brief 도형을 스킬 소유자 화면에만 표시한다. 아바타가 로컬 조종 중이면(리슨 호스트 본인 포함) 즉시 그리고,
	//!        서버 권위에서 원격 소유 아바타면 SkillControlComponent Client RPC로 소유 클라이언트에 전송한다.
	PROJECTP_API void DrawShapeForOwner(AActor* AvatarActor, const FMySkillDebugShape& Shape);
}
