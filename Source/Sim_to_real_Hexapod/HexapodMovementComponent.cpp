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
	if (InputDirection.SizeSquared() > 0.01f)
	{
		GaitPhase = FMath::Fmod(GaitPhase + DeltaTime * WalkSpeed, 1.0f);
		CalculateStepAndMove(GaitPhase);
	}
	/*else {
		ResetToCenter();
	}*/
	// INPUT 없으면 아무것도 안 함 — Python JOINTS 명령이 제어권을 가짐
	// ...
}

void UHexapodMovementComponent::CalculateStepAndMove(float GlobalPhase) {
	
	float leftStride = (InputDirection.X * MaxStride) + (InputDirection.Y * TurnRate);
	float rightStride = (InputDirection.X * MaxStride) - (InputDirection.Y * TurnRate);

	// �ִ� ���� ����
	leftStride = FMath::Clamp(leftStride, -MaxStride, MaxStride);
	rightStride = FMath::Clamp(rightStride, -MaxStride, MaxStride);

	
	float phaseA = GlobalPhase;
	float phaseB = FMath::Fmod(GlobalPhase + 0.5f, 1.0f);

	// Group A: Leg5(L��), Leg1(R��), Leg3(L��)
	ApplyLegMovement(5, phaseA, true, leftStride);   // ���� ��
	ApplyLegMovement(1, phaseA, false, rightStride);  // ������ ��
	ApplyLegMovement(3, phaseA, true, leftStride);   // ���� ��

	// Group B: Leg2(R��), Leg4(L��), Leg0(R��)
	ApplyLegMovement(2, phaseB, false, rightStride);  // ������ ��
	ApplyLegMovement(4, phaseB, true, leftStride);   // ���� ��
	ApplyLegMovement(0, phaseB, false, rightStride);  // ������ ��
}


void UHexapodMovementComponent::ApplyLegMovement(int32 LegIndex, float Phase, bool bIsLeft, float CurrentStride) {
	if (!HexapodRobot) return;
	const TArray<FHexapodLeg>& Legs = HexapodRobot->GetLegs();
	if (!Legs.IsValidIndex(LegIndex)) return;

	float xOffset = 0.f;
	float zOffset = 0.f;

	if (Phase < 0.5f)  
	{
		float t = Phase * 2.0f;  
		xOffset = FMath::Lerp(-CurrentStride, CurrentStride, t);    
		zOffset = FMath::Sin(t * PI) * LiftAngle;					
	}
	else  
	{
		float t = (Phase - 0.5f) * 2.0f; 
		xOffset = FMath::Lerp(CurrentStride, -CurrentStride, t);
		zOffset = 0.f;
	}

	const FHexapodLeg& Leg = Legs[LegIndex];

	if (bIsLeft)
	{
		SetJointTarget(Leg.HipConstraint, xOffset);
		SetJointTarget(Leg.ThighConstraint, zOffset);
		SetJointTarget(Leg.CalfConstraint, zOffset * 0.6f+45); 
	}
	else 
	{
		SetJointTarget(Leg.HipConstraint, xOffset * -1.0f); 
		SetJointTarget(Leg.ThighConstraint,zOffset);
		SetJointTarget(Leg.CalfConstraint, zOffset * 0.6f + 45); 
	}
}

void UHexapodMovementComponent::ResetToCenter() {
	if (!HexapodRobot) return; 
	const TArray<FHexapodLeg>& Legs = HexapodRobot->GetLegs();

	for (const FHexapodLeg& Leg : Legs) {
		SetJointTarget(Leg.HipConstraint, 0.f);
		SetJointTarget(Leg.ThighConstraint, 0.f);
		SetJointTarget(Leg.CalfConstraint, 60.f);
	}
}

void UHexapodMovementComponent::SetJointTarget(UPhysicsConstraintComponent* Constraint, float Target) {
	if (!Constraint) return;
	Constraint->SetAngularOrientationTarget(FRotator(0.f, Target, 0.f));  
}