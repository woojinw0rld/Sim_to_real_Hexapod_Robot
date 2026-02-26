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
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// RL Action: 18개 목표 각도 입력 (6다리 × 3관절)
	void ApplyJointTargets(const TArray<float>& Targets);

	// RL Observation: 18개 관절 현재 각도 반환
	TArray<float> GetJointAngles() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	// 몸통 메시
	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Robot", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* BodyMesh;

	// 6개 다리
	UPROPERTY(VisibleAnywhere, Category = "Robot", meta = (AllowPrivateAccess = "true"))
	TArray<FHexapodLeg> Legs;

	// 생성자에서 다리 컴포넌트 초기화
	void InitializeLeg(int32 LegIndex, FVector LegOffset, FRotator LegRotation, UStaticMesh* CoxaMesh, UStaticMesh* FemurMesh, UStaticMesh* TibiaMesh);

	// BeginPlay에서 물리 관절 연결 및 설정
	void SetupLegConstraints();

	/*
	 * 6다리 위치 및 방향 설정
	 *
	 *  Leg3  Leg4  Leg5
	 *   [뒤] [중] [앞]
	 *  ================  (몸통)
	 *   [뒤] [중] [앞]
	 *  Leg0  Leg1  Leg2
	 */
	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Robot|LegsPosition|Hip", meta = (AllowPrivateAccess = "true"))
	// 전부다 hip위치 기준.
	TArray<FVector> HipOffsets = {
			FVector(8.f,    10.f,  0.f),  // Leg0: 뒤 오른쪽
			FVector(11.f,    0.f,  0.f),  // Leg1: 중 오른쪽
			FVector(8.f,   -10.f,  0.f),  // Leg2: 앞 오른쪽
			FVector(-8.f,   10.f,  0.f),  // Leg3: 뒤 왼쪽
			FVector(-11.f,   0.f,  0.f),  // Leg4: 중 왼쪽
			FVector(-8.f,  -10.f,  0.f),  // Leg5: 앞 왼쪽
	};
	// 전부다 hip위치 기준.
	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Robot|LegsPosition|Hip", meta = (AllowPrivateAccess = "true"))
	TArray<FRotator> HipRotations = {
		FRotator(0.f,   45.f,   0.f),   // Leg0
		FRotator(0.f,    0.f,   0.f),   // Leg1
		FRotator(0.f,  -45.f,   0.f),   // Leg2
		FRotator(0.f,  135.f,   0.f),   // Leg3
		FRotator(0.f, -180.f,   0.f),   // Leg4
		FRotator(0.f, -135.f,   0.f),   // Leg5
	};


	// ----------------------------------------------- 다리 각 메쉬의 상대 위치 및 방향 설정 hip제외
	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Robot|LegsPosition|Thigh", meta = (AllowPrivateAccess = "true"))
	FVector ThighOffset = FVector(2.f, -2.5f, -1.5f);  // Hip에서 Thigh까지의 상대 위치
	UPROPERTY(EditAnywhere, BluePrintReadWrite, Category = "Robot|LegsPosition|Thigh", meta = (AllowPrivateAccess = "true"))
	FRotator ThighRotation = FRotator(10.f, 0.f, 90.f);

	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Robot|LegsPosition|Calf", meta = (AllowPrivateAccess = "true"))
	FVector CalfOffset = FVector(15.5f, -1.f, 2.5);
	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Robot|LegsPosition|Calf", meta = (AllowPrivateAccess = "true"))
	FRotator CalfRotator = FRotator(0.f, 90.f, 90.f);
	//FRotator(Pitch, Yaw, Roll)
	//       Y축    Z축   X축


	// -------------------------------------------------- Constraint 위치 및 방향 설정

	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Robot|ConstraintPosition|Hip", meta = (AllowPrivateAccess = "true"))
	FVector HipConstraintOffset = FVector(-3.f,0.f,0.f);    // 모든 다리의 HipConstraint 위치
	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Robot|ConstraintPosition|Hip", meta = (AllowPrivateAccess = "true"))
	FRotator HipConstraintRotation = FRotator(0.f, -90.f, 0.f);   

	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Robot|ConstraintPosition|Thigh", meta = (AllowPrivateAccess = "true"))
	FVector ThighConstraintOffset = FVector(0.f,0.f,2.5f);  // 모든 다리의 ThighConstraint 위치
	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Robot|ConstraintPosition|Thigh", meta = (AllowPrivateAccess = "true"))
	FRotator ThighConstraintRotation = FRotator(0.f, 0.f, 0.f);

	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Robot|ConstraintPosition|Calf", meta = (AllowPrivateAccess = "true"))
	FVector CalfConstraintOffset = FVector(0.f,0.f,10.f);    // 모든 다리의 CalfConstraint 위치
	UPROPERTY(VisibleAnywhere, BluePrintReadOnly, Category = "Robot|ConstraintPosition|Calf", meta = (AllowPrivateAccess = "true"))
	FRotator CalfConstraintRotation = FRotator(0.f, 0.f, 90.f);

};
