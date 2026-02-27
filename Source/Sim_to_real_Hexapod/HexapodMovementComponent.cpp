// Fill out your copyright notice in the Description page of Project Settings.


#include "HexapodMovementComponent.h"
#include "HexapodRobot.h" 
#include "PhysicsEngine/PhysicsConstraintComponent.h"

// Sets default values for this component's properties
UHexapodMovementComponent::UHexapodMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UHexapodMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	HexapodRobot = Cast<AHexapodRobot>(GetOwner());
	if(HexapodRobot == nullptr){
		UE_LOG(LogTemp, Warning, TEXT("HexapodMovementComponent: Owner is not AHexapodRobot!"));
		return;
	}	
}


// Called every frame
void UHexapodMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	//UE_LOG(LogTemp, Display, TEXT("InputDirection X: %f\tY: %f"), InputDirection.X, InputDirection.Y);
	if (InputDirection.SizeSquared() > 0.01f)  // 입력이 있으면
	{
		GaitPhase = FMath::Fmod(GaitPhase + DeltaTime * WalkSpeed, 1.0f);
		CalculateStepAndMove(GaitPhase);
	}
	else  // 입력이 없으면
	{
		ResetToCenter();
	}
	// ...
}

void UHexapodMovementComponent::CalculateStepAndMove(float GlobalPhase) {
	// 차동구동: 입력에 따라 왼쪽/오른쪽 보폭 계산
	float leftStride = (InputDirection.X * MaxStride) + (InputDirection.Y * TurnRate);
	float rightStride = (InputDirection.X * MaxStride) - (InputDirection.Y * TurnRate);

	// 최대 보폭 제한
	leftStride = FMath::Clamp(leftStride, -MaxStride, MaxStride);
	rightStride = FMath::Clamp(rightStride, -MaxStride, MaxStride);

	// Tripod Gait: 두 그룹 Phase 계산
	float phaseA = GlobalPhase;
	float phaseB = FMath::Fmod(GlobalPhase + 0.5f, 1.0f);

	// Group A: Leg5(L앞), Leg1(R중), Leg3(L뒤)
	ApplyLegMovement(5, phaseA, true, leftStride);   // 왼쪽 앞
	ApplyLegMovement(1, phaseA, false, rightStride);  // 오른쪽 중
	ApplyLegMovement(3, phaseA, true, leftStride);   // 왼쪽 뒤

	// Group B: Leg2(R앞), Leg4(L중), Leg0(R뒤)
	ApplyLegMovement(2, phaseB, false, rightStride);  // 오른쪽 앞
	ApplyLegMovement(4, phaseB, true, leftStride);   // 왼쪽 중
	ApplyLegMovement(0, phaseB, false, rightStride);  // 오른쪽 뒤
}

/**
	* 다리 1개의 Swing/Stance 움직임을 계산하여 관절 각도 적용
	* @param LegIndex  다리 인덱스 (0~5)
	* @param Phase     현재 걸음 사이클 위치 (0.0 ~ 1.0)
		* Swing Phase(0.0 ~0.5) : 다리가 공중에서 앞으로 이동
		* Stance Phase(0.5 ~1.0) : 다리가 땅을 짚고 뒤로 밀어냄 → 몸통이 앞으로 
	* @param bIsLeft   왼쪽 다리 여부 (오른쪽이면 Hip 부호 반전)
	* @param CurrentStride 보폭 크기 (음수 = 후진)
*/
void UHexapodMovementComponent::ApplyLegMovement(int32 LegIndex, float Phase, bool bIsLeft, float CurrentStride) {
	if (!HexapodRobot) return;
	const TArray<FHexapodLeg>& Legs = HexapodRobot->GetLegs();
	if (!Legs.IsValidIndex(LegIndex)) return;

	float xOffset = 0.f;
	float zOffset = 0.f;

	if (Phase < 0.5f)  // Swing Phase: 공중에서 앞으로
	{
		float t = Phase * 2.0f;  // 0~0.5를 0~1로 정규화
		xOffset = FMath::Lerp(-CurrentStride, CurrentStride, t);    //Hio용 앞뒤 횡운동
		zOffset = FMath::Sin(t * PI) * LiftAngle;					//calf. Thigh용 상하운동 계산용. 
	}
	else  // Stance Phase: 땅 짚고 뒤로 밀기
	{
		float t = (Phase - 0.5f) * 2.0f;  // 0.5~1을 0~1로 정규화
		xOffset = FMath::Lerp(CurrentStride, -CurrentStride, t);
		zOffset = 0.f;
	}

	const FHexapodLeg& Leg = Legs[LegIndex];

	if (bIsLeft)
	{
		SetJointTarget(Leg.HipConstraint, xOffset);
		SetJointTarget(Leg.ThighConstraint, zOffset);
		SetJointTarget(Leg.CalfConstraint, zOffset * 0.6f);
	}
	else 
	{
		SetJointTarget(Leg.HipConstraint, xOffset * -1.0f);  // 오른쪽은 부호 반전
		SetJointTarget(Leg.ThighConstraint,zOffset);
		SetJointTarget(Leg.CalfConstraint, zOffset * 0.6f);
	}
}

void UHexapodMovementComponent::ResetToCenter() {
	if (!HexapodRobot) return; 
	const TArray<FHexapodLeg>& Legs = HexapodRobot->GetLegs();

	for (const FHexapodLeg& Leg : Legs) {
		SetJointTarget(Leg.HipConstraint, 0.f);
		SetJointTarget(Leg.ThighConstraint, 45.f);
		SetJointTarget(Leg.CalfConstraint, 60.f);
	}
}

void UHexapodMovementComponent::SetJointTarget(UPhysicsConstraintComponent* Constraint, float Target) {
	if (!Constraint) return;
	Constraint->SetAngularOrientationTarget(FRotator(0.f, Target, 0.f));  //Z축을 회전축으로 움직이기에,,,,,,,,,
	//UE_LOG(LogTemp, Display, TEXT("%f"), Target);
}
