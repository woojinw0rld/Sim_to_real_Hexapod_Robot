// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "HexapodRobot.generated.h"


USTRUCT()
struct FHexapodLeg
{
	GENERATED_BODY()

	// 관절 피벗 (회전 기준점)
	UPROPERTY(VisibleAnywhere)
	USceneComponent* Hip = nullptr;		// Coxa: 몸통-다리 연결부

	UPROPERTY(VisibleAnywhere)
	USceneComponent* Thigh = nullptr;	// Femur: 다리 중간

	UPROPERTY(VisibleAnywhere)
	USceneComponent* Calf = nullptr;	// Tibia: 다리 끝

	// 메시 (시각적 표현)
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* HipMesh = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* ThighMesh = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* CalfMesh = nullptr;

	// 물리 관절 (강화학습 Action/Observation 연결점)
	UPROPERTY(VisibleAnywhere)
	UPhysicsConstraintComponent* HipConstraint = nullptr;

	UPROPERTY(VisibleAnywhere)
	UPhysicsConstraintComponent* ThighConstraint = nullptr;

	UPROPERTY(VisibleAnywhere)
	UPhysicsConstraintComponent* CalfConstraint = nullptr;
};

UCLASS()
class SIM_TO_REAL_HEXAPOD_API AHexapodRobot : public APawn
{
	GENERATED_BODY()

public:
	AHexapodRobot();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// RL Action: 18개 목표 각도 입력 (6다리 × 3관절)
	void ApplyJointTargets(const TArray<float>& Targets);

	// RL Observation: 18개 관절 현재 각도 반환
	TArray<float> GetJointAngles() const;

private:
	// 몸통 메시
	UPROPERTY(VisibleAnywhere, Category = "Robot")
	UStaticMeshComponent* BodyMesh;

	// 6개 다리
	UPROPERTY(VisibleAnywhere, Category = "Robot")
	TArray<FHexapodLeg> Legs;

	// 생성자에서 다리 컴포넌트 초기화
	void InitializeLeg(int32 LegIndex, FVector LegOffset, FRotator LegRotation,
	                   UStaticMesh* CoxaMesh, UStaticMesh* FemurMesh, UStaticMesh* TibiaMesh);

	// BeginPlay에서 물리 관절 연결 및 설정
	void SetupLegConstraints();
};
