// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HexapodMovementComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SIM_TO_REAL_HEXAPOD_API UHexapodMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UHexapodMovementComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void SetMoveForward(float Value) { InputDirection.X = Value; }
	void SetMoveRight(float Value) { InputDirection.Y = Value; }


private:
	class AHexapodRobot* HexapodRobot = nullptr;
	FVector2D InputDirection = FVector2D::ZeroVector;  // X: Forward/Backward, Y: Right/Left


	//보행 타이머 속도. 클수록 발걸음 빨라짐
	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Gait", meta = (AllowPrivateAccess = "true"))
	float WalkSpeed = 2.0f;

	//다리가 앞뒤로 움직이는 최대 각도(도) 
	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Gait", meta = (AllowPrivateAccess = "true"))
	float MaxStride = 20.0f;

	//회전 시 좌/우 stride 차이량
	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Gait", meta = (AllowPrivateAccess = "true"))
	float TurnRate = 20.0f;

	//발을 들어올리는 최대 각도(도)
	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Gait", meta = (AllowPrivateAccess = "true"))
	float LiftAngle = 15.0f;

	//보행 상태  //0~1 반복하는 보행 타이머. 현재 보행 사이클 어디쯤인지
	float GaitPhase = 0.0f;

	//좌/우 stride 계산 후 6개 다리에 위상 배분
	void CalculateStepAndMove(float GlobalPhase);
	//다리 1개의 Swing/Stance 계산 → 관절 목표각도 설정
	void ApplyLegMovement(int32 LegIndex, float Phase, bool bIsLeft, float CurrentStride);
	//입력 없을 때 모든 관절을 0도로 복귀
	void ResetToCenter();
	//Constraint 하나에 목표각도 실제로 적용하는 헬퍼
	void SetJointTarget(class UPhysicsConstraintComponent* Constraint, float Target);
};
