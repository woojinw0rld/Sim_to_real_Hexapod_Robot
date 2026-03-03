// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HexapodNetworkComponent.generated.h"

// 전방 선언 — 헤더 의존성 최소화
class FSocket;

/**
 * UHexapodNetworkComponent
 *
 * Python ↔ UE5 UDP 통신 컴포넌트.
 * AHexapodRobot에 붙여서 사용.
 *
 * ── 수신 프로토콜 (Python → UE5) ──────────────────────────────────────────
 *  "JOINTS a0 a1 ... a17"   : 18개 관절 목표 각도 (도) → ApplyJointTargets()
 *  "INPUT  x  y"            : 이동 입력 → SetMoveForward / SetMoveRight
 *  "RESET"                  : 서있는 자세 (Hip=0, Thigh=0, Calf=60)
 *
 * ── 송신 프로토콜 (UE5 → Python) ──────────────────────────────────────────
 *  "OBS a0...a17 px py pz roll pitch yaw"  : 관절 각도 + 위치/자세
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SIM_TO_REAL_HEXAPOD_API UHexapodNetworkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHexapodNetworkComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	/** Python 에서 수신하는 UDP 포트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	int32 ListenPort = 7777;

	/** 매 패킷마다 관측값(OBS)을 Python 으로 되돌려 보낼지 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	bool bSendObservations = true;

private:
	FSocket* ListenSocket = nullptr;

	class AHexapodRobot*             HexapodRobot = nullptr;
	class UHexapodMovementComponent* MovementComp = nullptr;

	bool InitSocket();
	void CloseSocket();
	void ProcessPacket(const FString& Packet, const FString& SenderIP, int32 SenderPort);
	void SendObservation(const FString& IP, int32 Port);
};
