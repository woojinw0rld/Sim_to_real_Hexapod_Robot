// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HexapodRobot.generated.h"

UCLASS()
class SIM_TO_REAL_HEXAPOD_API AHexapodRobot : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AHexapodRobot();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
private:
	UPROPERTY(VisibleAnywhere, Category = "Robot")
	USceneComponent* Body;


	/*
	* UE계층
	 Body (SceneComponent)
    ├ Leg1_Hip
	│	 │	└ Leg1_Hip_Mesh
    │   └ Leg1_Thigh
	│		│	└ Leg1_Thigh_Mesh
    │		└ Leg1_Calf
	│			└ Leg1_Calf_Mesh
    ├ Leg2_Hip

	
	*/

	///Leg 1 SceneComponent  //관절의 위치와 회전을 제어하는 컴포넌트 
	//현실과의 오차를 줄이기위해서는 관절위치와 정확하게 일치해야함.
	UPROPERTY(VisibleAnywhere, Category = "Robot")
	USceneComponent* Leg1_Hip;  //몸통 쪽

	UPROPERTY(VisibleAnywhere, Category = "Robot")
	USceneComponent* Leg1_Thigh; //다리 중간

	UPROPERTY(VisibleAnywhere, Category = "Robot")
	USceneComponent* Leg1_Calf; // 다리 끝

	// Leg 1 StaticMeshComponent // 외형담당 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = "Robot")
	UStaticMeshComponent* Leg1_Hip_Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Robot")
	UStaticMeshComponent* Leg1_Thigh_Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Robot")
	UStaticMeshComponent* Leg1_Calf_Mesh;
};
