// Fill out your copyright notice in the Description page of Project Settings.

#include "HexapodRobot.h"
#include "UObject/ConstructorHelpers.h"

AHexapodRobot::AHexapodRobot()
{
	PrimaryActorTick.bCanEverTick = true;

	// 메시 에셋 로드
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMeshAsset(
		TEXT("/Game/Robots/Meshes/body_frame.body_frame"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CoxaAsset(
		TEXT("/Game/Robots/Meshes/coxa-996.coxa-996"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FemurAsset(
		TEXT("/Game/Robots/Meshes/femur-996.femur-996"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TibiaAsset(
		TEXT("/Game/Robots/Meshes/Tibia-996.Tibia-996"));

	// 몸통 메시 (루트)
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	RootComponent = BodyMesh;
	BodyMesh->SetSimulatePhysics(true);
	if (BodyMeshAsset.Succeeded())
		BodyMesh->SetStaticMesh(BodyMeshAsset.Object);

	/*
	 * 6다리 위치 및 방향 설정
	 *
	 *  Leg3  Leg4  Leg5
	 *   [앞] [중] [뒤]
	 *  ================  (몸통)
	 *   [앞] [중] [뒤]
	 *  Leg0  Leg1  Leg2
	 */
	TArray<FVector> Offsets = {
		FVector( 20.f,  10.f, 0.f),  // Leg0: 앞 오른쪽
		FVector(  0.f,  10.f, 0.f),  // Leg1: 중 오른쪽
		FVector(-20.f,  10.f, 0.f),  // Leg2: 뒤 오른쪽
		FVector( 20.f, -10.f, 0.f),  // Leg3: 앞 왼쪽
		FVector(  0.f, -10.f, 0.f),  // Leg4: 중 왼쪽
		FVector(-20.f, -10.f, 0.f),  // Leg5: 뒤 왼쪽
	};

	TArray<FRotator> Rotations = {
		FRotator(0.f,  45.f, 0.f),   // Leg0
		FRotator(0.f,  90.f, 0.f),   // Leg1
		FRotator(0.f, 135.f, 0.f),   // Leg2
		FRotator(0.f, -45.f, 0.f),   // Leg3
		FRotator(0.f, -90.f, 0.f),   // Leg4
		FRotator(0.f,-135.f, 0.f),   // Leg5
	};

	UStaticMesh* CoxaMesh  = CoxaAsset.Succeeded()  ? CoxaAsset.Object  : nullptr;
	UStaticMesh* FemurMesh = FemurAsset.Succeeded() ? FemurAsset.Object : nullptr;
	UStaticMesh* TibiaMesh = TibiaAsset.Succeeded() ? TibiaAsset.Object : nullptr;

	Legs.SetNum(6);
	for (int32 i = 0; i < 6; i++)
	{
		InitializeLeg(i, Offsets[i], Rotations[i], CoxaMesh, FemurMesh, TibiaMesh);
	}
}

void AHexapodRobot::InitializeLeg(int32 LegIndex, FVector LegOffset, FRotator LegRotation,
                                   UStaticMesh* CoxaMesh, UStaticMesh* FemurMesh, UStaticMesh* TibiaMesh)
{
	FHexapodLeg& Leg = Legs[LegIndex];
	FString P = FString::Printf(TEXT("Leg%d"), LegIndex);

	// --- Hip (Coxa) ---
	Leg.Hip = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("%s_Hip"), *P));
	Leg.Hip->SetupAttachment(RootComponent);
	Leg.Hip->SetRelativeLocation(LegOffset);
	Leg.Hip->SetRelativeRotation(LegRotation);

	Leg.HipMesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("%s_HipMesh"), *P));
	Leg.HipMesh->SetupAttachment(Leg.Hip);
	Leg.HipMesh->SetSimulatePhysics(true);
	if (CoxaMesh) Leg.HipMesh->SetStaticMesh(CoxaMesh);

	Leg.HipConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(*FString::Printf(TEXT("%s_HipConstraint"), *P));
	Leg.HipConstraint->SetupAttachment(Leg.Hip);

	// --- Thigh (Femur) ---
	Leg.Thigh = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("%s_Thigh"), *P));
	Leg.Thigh->SetupAttachment(Leg.HipMesh);
	Leg.Thigh->SetRelativeLocation(FVector(10.f, 0.f, 0.f)); // Hip 끝점에 위치

	Leg.ThighMesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("%s_ThighMesh"), *P));
	Leg.ThighMesh->SetupAttachment(Leg.Thigh);
	Leg.ThighMesh->SetSimulatePhysics(true);
	if (FemurMesh) Leg.ThighMesh->SetStaticMesh(FemurMesh);

	Leg.ThighConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(*FString::Printf(TEXT("%s_ThighConstraint"), *P));
	Leg.ThighConstraint->SetupAttachment(Leg.Thigh);

	// --- Calf (Tibia) ---
	Leg.Calf = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("%s_Calf"), *P));
	Leg.Calf->SetupAttachment(Leg.ThighMesh);
	Leg.Calf->SetRelativeLocation(FVector(10.f, 0.f, 0.f)); // Thigh 끝점에 위치

	Leg.CalfMesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("%s_CalfMesh"), *P));
	Leg.CalfMesh->SetupAttachment(Leg.Calf);
	Leg.CalfMesh->SetSimulatePhysics(true);
	if (TibiaMesh) Leg.CalfMesh->SetStaticMesh(TibiaMesh);

	Leg.CalfConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(*FString::Printf(TEXT("%s_CalfConstraint"), *P));
	Leg.CalfConstraint->SetupAttachment(Leg.Calf);
}

void AHexapodRobot::BeginPlay()
{
	Super::BeginPlay();
	SetupLegConstraints();
}

void AHexapodRobot::SetupLegConstraints()
{
	for (int32 i = 0; i < 6; i++)
	{
		FHexapodLeg& Leg = Legs[i];

		// Hip 관절: Body <-> HipMesh
		// 수평 회전만 허용 (Yaw ±45도)
		Leg.HipConstraint->SetConstrainedComponents(BodyMesh, NAME_None, Leg.HipMesh, NAME_None);
		Leg.HipConstraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 45.f);
		Leg.HipConstraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.HipConstraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.HipConstraint->SetAngularOrientationDrive(true, false);
		Leg.HipConstraint->SetAngularDriveParams(1000.f, 100.f, 1000.f); // Stiffness, Damping, ForceLimit

		// Thigh 관절: HipMesh <-> ThighMesh
		// 수직 회전만 허용 (Pitch ±60도)
		Leg.ThighConstraint->SetConstrainedComponents(Leg.HipMesh, NAME_None, Leg.ThighMesh, NAME_None);
		Leg.ThighConstraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 60.f);
		Leg.ThighConstraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.ThighConstraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.ThighConstraint->SetAngularOrientationDrive(true, false);
		Leg.ThighConstraint->SetAngularDriveParams(1000.f, 100.f, 1000.f);

		// Calf 관절: ThighMesh <-> CalfMesh
		// 수직 회전만 허용 (Pitch ±60도)
		Leg.CalfConstraint->SetConstrainedComponents(Leg.ThighMesh, NAME_None, Leg.CalfMesh, NAME_None);
		Leg.CalfConstraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 60.f);
		Leg.CalfConstraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.CalfConstraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.CalfConstraint->SetAngularOrientationDrive(true, false);
		Leg.CalfConstraint->SetAngularDriveParams(1000.f, 100.f, 1000.f);
	}
}

void AHexapodRobot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AHexapodRobot::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

// RL Action: 18개 목표 각도 → 관절 드라이브 적용
// Targets[i*3+0] = Leg i의 Hip 목표각도
// Targets[i*3+1] = Leg i의 Thigh 목표각도
// Targets[i*3+2] = Leg i의 Calf 목표각도
void AHexapodRobot::ApplyJointTargets(const TArray<float>& Targets)
{
	if (Targets.Num() != 18) return;

	for (int32 i = 0; i < 6; i++)
	{
		Legs[i].HipConstraint->SetAngularOrientationTarget(FRotator(Targets[i * 3 + 0], 0.f, 0.f));
		Legs[i].ThighConstraint->SetAngularOrientationTarget(FRotator(Targets[i * 3 + 1], 0.f, 0.f));
		Legs[i].CalfConstraint->SetAngularOrientationTarget(FRotator(Targets[i * 3 + 2], 0.f, 0.f));
	}
}

// RL Observation: 현재 관절 각도 18개 반환
TArray<float> AHexapodRobot::GetJointAngles() const
{
	TArray<float> Angles;
	Angles.SetNum(18);

	for (int32 i = 0; i < 6; i++)
	{
		Angles[i * 3 + 0] = Legs[i].HipMesh->GetRelativeRotation().Yaw;
		Angles[i * 3 + 1] = Legs[i].ThighMesh->GetRelativeRotation().Pitch;
		Angles[i * 3 + 2] = Legs[i].CalfMesh->GetRelativeRotation().Pitch;
	}

	return Angles;
}
