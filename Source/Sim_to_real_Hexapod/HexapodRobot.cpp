// Fill out your copyright notice in the Description page of Project Settings.

#include "HexapodRobot.h"
#include "UObject/ConstructorHelpers.h"
#include "HexapodMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

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

	//카메라
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Spring Arm"));
	SpringArm->SetupAttachment(RootComponent);
	
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	
	if (BodyMeshAsset.Succeeded())
		BodyMesh->SetStaticMesh(BodyMeshAsset.Object);


	UStaticMesh* CoxaMesh  = CoxaAsset.Succeeded()  ? CoxaAsset.Object  : nullptr;
	UStaticMesh* FemurMesh = FemurAsset.Succeeded() ? FemurAsset.Object : nullptr;
	UStaticMesh* TibiaMesh = TibiaAsset.Succeeded() ? TibiaAsset.Object : nullptr;

	Legs.SetNum(6);
	for (int32 i = 0; i < 6; i++)
	{
		InitializeLeg(i, HipOffsets[i], HipRotations[i], CoxaMesh, FemurMesh, TibiaMesh);
	}

	MovementComponent = CreateDefaultSubobject<UHexapodMovementComponent>(TEXT("MovementComponent"));
}

void AHexapodRobot::InitializeLeg(int32 LegIndex, FVector LegOffset, FRotator LegRotation, UStaticMesh* CoxaMesh, UStaticMesh* FemurMesh, UStaticMesh* TibiaMesh)
{
	FHexapodLeg& Leg = Legs[LegIndex];
	FString P = FString::Printf(TEXT("Leg%d"), LegIndex);

	// ----------------------------------------------- Hip (Coxa) ---
	Leg.Hip = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("%s_Hip"), *P));
	Leg.Hip->SetupAttachment(RootComponent);
	Leg.Hip->SetRelativeLocation(LegOffset);
	Leg.Hip->SetRelativeRotation(LegRotation);

	Leg.HipMesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("%s_HipMesh"), *P));
	Leg.HipMesh->SetupAttachment(Leg.Hip);
	Leg.HipMesh->SetSimulatePhysics(false);
	if (CoxaMesh) Leg.HipMesh->SetStaticMesh(CoxaMesh);

	Leg.HipConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(*FString::Printf(TEXT("%s_HipConstraint"), *P));
	Leg.HipConstraint->SetupAttachment(Leg.Hip);
	Leg.HipConstraint->SetRelativeLocation(HipConstraintOffset);
	Leg.HipConstraint->SetRelativeRotation(HipConstraintRotation);

	// ----------------------------------------------- Thigh (Femur) ---
	Leg.Thigh = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("%s_Thigh"), *P));
	Leg.Thigh->SetupAttachment(RootComponent); // HipMesh 자식 아님 → 물리 바디 중첩 방지
	// 위치/방향은 OnConstruction에서 FK로 계산

	Leg.ThighMesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("%s_ThighMesh"), *P));
	Leg.ThighMesh->SetupAttachment(Leg.Thigh);
	Leg.ThighMesh->SetSimulatePhysics(false);
	if (FemurMesh) Leg.ThighMesh->SetStaticMesh(FemurMesh);

	Leg.ThighConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(*FString::Printf(TEXT("%s_ThighConstraint"), *P));
	Leg.ThighConstraint->SetupAttachment(Leg.Thigh);
	Leg.ThighConstraint->SetRelativeLocation(ThighConstraintOffset);
	Leg.ThighConstraint->SetRelativeRotation(ThighConstraintRotation);

	// ----------------------------------------------- Calf (Tibia) ---
	Leg.Calf = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("%s_Calf"), *P));
	Leg.Calf->SetupAttachment(RootComponent); // ThighMesh 자식 아님 → 물리 바디 중첩 방지
	// 위치/방향은 OnConstruction에서 FK로 계산

	Leg.CalfMesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("%s_CalfMesh"), *P));
	Leg.CalfMesh->SetupAttachment(Leg.Calf);
	Leg.CalfMesh->SetSimulatePhysics(false);
	if (TibiaMesh) Leg.CalfMesh->SetStaticMesh(TibiaMesh);

	Leg.CalfConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(*FString::Printf(TEXT("%s_CalfConstraint"), *P));
	Leg.CalfConstraint->SetupAttachment(Leg.Calf);
	Leg.CalfConstraint->SetRelativeLocation(CalfConstraintOffset);
	Leg.CalfConstraint->SetRelativeRotation(CalfConstraintRotation);
}


//bp에서 값이 오버라이딩 되어있으면 그거 cpp값으로 덮어 씌우는 기능을 가짐. 
void AHexapodRobot::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	for (int32 i = 0; i < 6; i++)
	{
		if (!Legs[i].Hip) continue;

		if (HipOffsets.IsValidIndex(i))
			Legs[i].Hip->SetRelativeLocation(HipOffsets[i]);

		if (HipRotations.IsValidIndex(i))
			Legs[i].Hip->SetRelativeRotation(HipRotations[i]);

		// FK: Thigh 월드 위치 = Hip위치 + Hip회전 * ThighOffset
		if (HipOffsets.IsValidIndex(i) && HipRotations.IsValidIndex(i) && Legs[i].Thigh)
		{
			FQuat HipQuat = HipRotations[i].Quaternion();
			FVector ThighWorldPos = HipOffsets[i] + HipQuat.RotateVector(ThighOffset);
			FQuat   ThighWorldQuat = HipQuat * ThighRotation.Quaternion();
			Legs[i].Thigh->SetRelativeLocation(ThighWorldPos);
			Legs[i].Thigh->SetRelativeRotation(ThighWorldQuat.Rotator());

			// FK: Calf 월드 위치 = Thigh위치 + Thigh회전 * CalfOffset
			if (Legs[i].Calf)
			{
				FVector CalfWorldPos = ThighWorldPos + ThighWorldQuat.RotateVector(CalfOffset);
				FQuat   CalfWorldQuat = ThighWorldQuat * CalfRotator.Quaternion();
				Legs[i].Calf->SetRelativeLocation(CalfWorldPos);
				Legs[i].Calf->SetRelativeRotation(CalfWorldQuat.Rotator());
			}
		}
		if (Legs[i].HipConstraint) {
			Legs[i].HipConstraint->SetRelativeLocation(HipConstraintOffset);
			Legs[i].HipConstraint->SetRelativeRotation(HipConstraintRotation);
		}
		if (Legs[i].ThighConstraint) {
			Legs[i].ThighConstraint->SetRelativeLocation(ThighConstraintOffset);
			Legs[i].ThighConstraint->SetRelativeRotation(ThighConstraintRotation);
		}
		if(Legs[i].CalfConstraint) {
			Legs[i].CalfConstraint->SetRelativeLocation(CalfConstraintOffset);
			Legs[i].CalfConstraint->SetRelativeRotation(CalfConstraintRotation);
		}
	}
}

void AHexapodRobot::BeginPlay()
{
	Super::BeginPlay();
	SetupLegConstraints();
	BodyMesh->SetSimulatePhysics(true);
	//BodyMesh->SetEnableGravity(false);
	BodyMesh->SetLinearDamping(1.f);
	BodyMesh->SetAngularDamping(1.f);
	for (auto& Leg : Legs)
	{
		Leg.HipMesh->SetSimulatePhysics(true);
		//Leg.HipMesh->SetEnableGravity(false);   // 추가
		Leg.ThighMesh->SetSimulatePhysics(true);
		//Leg.ThighMesh->SetEnableGravity(false); // 추가
		Leg.CalfMesh->SetSimulatePhysics(true);
		//Leg.CalfMesh->SetEnableGravity(false);
	}
	TArray<float> StandingPose;
	StandingPose.SetNum(18);
	for (int32 i = 0; i < 6; i++){
		StandingPose[i * 3 + 0] = 0.f;   // Hip
		StandingPose[i * 3 + 1] = 45.f;  // Thigh
		StandingPose[i * 3 + 2] = 60.f;  // Calf
	}
	ApplyJointTargets(StandingPose);
	UE_LOG(LogTemp, Warning, TEXT("BodyMesh mass: %f kg"), BodyMesh->GetMass());
	UE_LOG(LogTemp, Warning, TEXT("HipMesh mass: %f kg"), Legs[0].HipMesh->GetMass());
	UE_LOG(LogTemp, Warning, TEXT("ThighMesh mass: %f kg"), Legs[0].ThighMesh->GetMass());
	UE_LOG(LogTemp, Warning, TEXT("CalfMesh mass: %f kg"), Legs[0].CalfMesh->GetMass());
}

void AHexapodRobot::SetupLegConstraints()
{
	for (int32 i = 0; i < 6; i++)
	{
		FHexapodLeg& Leg = Legs[i];
		/*
			Twist  = X축
			Swing1 = Z축  z축을 중심으로 회전 적용.
			Swing2 = Y축
		*/
		// Hip 관절: Body <-> HipMesh
		// 수평 회전만 허용 (Yaw ±90도)
		Leg.HipConstraint->SetConstrainedComponents(BodyMesh, NAME_None, Leg.HipMesh, NAME_None);
		Leg.HipConstraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.HipConstraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.HipConstraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.HipConstraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 90.f);
		Leg.HipConstraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.HipConstraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.HipConstraint->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
		Leg.HipConstraint->SetAngularOrientationDrive(true, false); // Swing만 (Twist는 Locked)
		Leg.HipConstraint->SetAngularDriveParams(50000.f, 200.f, 50000.f); // Stiffness, Damping, ForceLimit

		// Thigh 관절: HipMesh <-> ThighMesh
		// 수직 회전만 허용 (Pitch ±90도)
		Leg.ThighConstraint->SetConstrainedComponents(Leg.HipMesh, NAME_None, Leg.ThighMesh, NAME_None);
		Leg.ThighConstraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.ThighConstraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.ThighConstraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.ThighConstraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 90.f);
		Leg.ThighConstraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.ThighConstraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.ThighConstraint->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
		Leg.ThighConstraint->SetAngularOrientationDrive(true, false);
		Leg.ThighConstraint->SetAngularDriveParams(50000.f, 200.f, 50000.f);

		// Calf 관절: ThighMesh <-> CalfMesh
		// 수직 회전만 허용 (Pitch ±60도)
		Leg.CalfConstraint->SetConstrainedComponents(Leg.ThighMesh, NAME_None, Leg.CalfMesh, NAME_None);
		Leg.CalfConstraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.CalfConstraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.CalfConstraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.CalfConstraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 90.f);
		Leg.CalfConstraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.CalfConstraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.CalfConstraint->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
		Leg.CalfConstraint->SetAngularOrientationDrive(true, false);
		Leg.CalfConstraint->SetAngularDriveParams(50000.f, 200.f, 50000.f);

		FVector Force, Torque;
		Leg.HipConstraint->GetConstraintForce(Force, Torque);
		UE_LOG(LogTemp, Warning, TEXT("Leg%d Constraint Force: %s"), i, *Force.ToString());

	}
}

void AHexapodRobot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
}

void AHexapodRobot::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AHexapodRobot::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AHexapodRobot::MoveRight);
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
		Legs[i].HipConstraint->SetAngularOrientationTarget(FRotator(   0.f, Targets[i * 3 + 0], 0.f));
		Legs[i].ThighConstraint->SetAngularOrientationTarget(FRotator( 0.f, Targets[i * 3 + 1], 0.f));
		Legs[i].CalfConstraint->SetAngularOrientationTarget(FRotator(  0.f, Targets[i * 3 + 2], 0.f));
	}

}

// RL Observation: 현재 관절 각도 18개 반환
TArray<float> AHexapodRobot::GetJointAngles() const
{
	TArray<float> Angles;
	Angles.SetNum(18);

	for (int32 i = 0; i < 6; i++)
	{
		// Hip: HipMesh와 BodyMesh 사이 상대 회전
		FQuat BodyW = BodyMesh->GetComponentQuat();
		FQuat HipW = Legs[i].HipMesh->GetComponentQuat();
		FQuat HipRel = BodyW.Inverse() * HipW;
		Angles[i * 3 + 0] = HipRel.Rotator().Yaw;

		// Thigh: ThighMesh와 HipMesh 사이 상대 회전
		FQuat ThighW = Legs[i].ThighMesh->GetComponentQuat();
		FQuat ThighRel = HipW.Inverse() * ThighW;
		Angles[i * 3 + 1] = ThighRel.Rotator().Yaw;

		// Calf: CalfMesh와 ThighMesh 사이 상대 회전
		FQuat CalfW = Legs[i].CalfMesh->GetComponentQuat();
		FQuat CalfRel = ThighW.Inverse() * CalfW;
		Angles[i * 3 + 2] = CalfRel.Rotator().Yaw;
	}

	return Angles;
}


void AHexapodRobot::MoveForward(float Value)
{
	MovementComponent->SetMoveForward(Value);
}

void AHexapodRobot::MoveRight(float Value)
{
	MovementComponent->SetMoveRight(Value);
}