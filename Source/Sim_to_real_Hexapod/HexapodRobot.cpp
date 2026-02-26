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
}

void AHexapodRobot::InitializeLeg(int32 LegIndex, FVector LegOffset, FRotator LegRotation,
                                   UStaticMesh* CoxaMesh, UStaticMesh* FemurMesh, UStaticMesh* TibiaMesh)
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
	BodyMesh->SetSimulatePhysics(false);
	for (auto& Leg : Legs)
	{
		Leg.HipMesh->SetSimulatePhysics(true);
		Leg.ThighMesh->SetSimulatePhysics(true);
		Leg.CalfMesh->SetSimulatePhysics(true);
	}
	SetupLegConstraints();
	TArray<float> ZeroTargets;
	ZeroTargets.Init(0.f, 18);
	ApplyJointTargets(ZeroTargets);
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
		Leg.HipConstraint->SetAngularOrientationDrive(true, false);
		Leg.HipConstraint->SetAngularDriveParams(1500.f, 100.f, 328.f); // Stiffness, Damping, ForceLimit

		// Thigh 관절: HipMesh <-> ThighMesh
		// 수직 회전만 허용 (Pitch ±90도)
		Leg.ThighConstraint->SetConstrainedComponents(Leg.HipMesh, NAME_None, Leg.ThighMesh, NAME_None);
		Leg.ThighConstraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.ThighConstraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.ThighConstraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.ThighConstraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 90.f);
		Leg.ThighConstraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.ThighConstraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.ThighConstraint->SetAngularOrientationDrive(true, false);
		Leg.ThighConstraint->SetAngularDriveParams(1500.f, 100.f, 328.f);

		// Calf 관절: ThighMesh <-> CalfMesh
		// 수직 회전만 허용 (Pitch ±60도)
		Leg.CalfConstraint->SetConstrainedComponents(Leg.ThighMesh, NAME_None, Leg.CalfMesh, NAME_None);
		Leg.CalfConstraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.CalfConstraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.CalfConstraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Leg.CalfConstraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 60.f);
		Leg.CalfConstraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.CalfConstraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.f);
		Leg.CalfConstraint->SetAngularOrientationDrive(true, false);
		Leg.CalfConstraint->SetAngularDriveParams(1500.f, 100.f, 328.f);
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
		Angles[i * 3 + 0] = Legs[i].HipMesh->GetRelativeRotation().Yaw;
		Angles[i * 3 + 1] = Legs[i].ThighMesh->GetRelativeRotation().Yaw;
		Angles[i * 3 + 2] = Legs[i].CalfMesh->GetRelativeRotation().Yaw;
	}

	return Angles;
}
