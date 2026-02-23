// Fill out your copyright notice in the Description page of Project Settings.


#include "HexapodRobot.h"

// Sets default values
AHexapodRobot::AHexapodRobot()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	RootComponent = Body;
	
	Leg1_Hip = CreateDefaultSubobject<USceneComponent>(TEXT("Leg1_Hip"));
	Leg1_Hip->SetupAttachment(Body);
	Leg1_Hip_Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Leg1_Hip_Mesh"));
	Leg1_Hip_Mesh->SetupAttachment(Leg1_Hip);

	Leg1_Thigh = CreateDefaultSubobject<USceneComponent>(TEXT("Leg1_Thigh"));
	Leg1_Thigh->SetupAttachment(Leg1_Hip);
	Leg1_Thigh_Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Leg1_Thigh_Mesh"));
	Leg1_Thigh_Mesh->SetupAttachment(Leg1_Thigh);

	Leg1_Calf = CreateDefaultSubobject<USceneComponent>(TEXT("Leg1_Calf"));
	Leg1_Calf->SetupAttachment(Leg1_Thigh);
	Leg1_Calf_Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Leg1_Calf_Mesh"));
	Leg1_Calf_Mesh->SetupAttachment(Leg1_Calf);

}

// Called when the game starts or when spawned
void AHexapodRobot::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AHexapodRobot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AHexapodRobot::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

