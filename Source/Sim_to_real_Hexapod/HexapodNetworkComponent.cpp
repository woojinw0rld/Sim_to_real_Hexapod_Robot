// Fill out your copyright notice in the Description page of Project Settings.

#include "HexapodNetworkComponent.h"
#include "HexapodRobot.h"
#include "HexapodMovementComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

UHexapodNetworkComponent::UHexapodNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 생명주기
// ─────────────────────────────────────────────────────────────────────────────

void UHexapodNetworkComponent::BeginPlay()
{
	Super::BeginPlay();

	HexapodRobot = Cast<AHexapodRobot>(GetOwner());
	if (!HexapodRobot)
	{
		UE_LOG(LogTemp, Warning, TEXT("HexapodNetworkComponent: Owner가 AHexapodRobot이 아닙니다."));
		return;
	}
	MovementComp = HexapodRobot->FindComponentByClass<UHexapodMovementComponent>();

	if (InitSocket())
		UE_LOG(LogTemp, Log, TEXT("HexapodNetworkComponent: UDP 포트 %d 에서 수신 대기 중"), ListenPort);
	else
		UE_LOG(LogTemp, Error, TEXT("HexapodNetworkComponent: UDP 포트 %d 열기 실패"), ListenPort);
}

void UHexapodNetworkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseSocket();
	Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────────────────────
// 소켓 초기화 / 종료
// ─────────────────────────────────────────────────────────────────────────────

bool UHexapodNetworkComponent::InitSocket()
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSub) return false;

	ListenSocket = SocketSub->CreateSocket(NAME_DGram, TEXT("HexapodUDP"), false);
	if (!ListenSocket) return false;

	TSharedRef<FInternetAddr> Addr = SocketSub->CreateInternetAddr();
	Addr->SetAnyAddress();
	Addr->SetPort(ListenPort);

	ListenSocket->SetNonBlocking(true);
	ListenSocket->SetReuseAddr(true);

	return ListenSocket->Bind(*Addr);
}

void UHexapodNetworkComponent::CloseSocket()
{
	if (ListenSocket)
	{
		ListenSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 매 프레임 수신 처리
// ─────────────────────────────────────────────────────────────────────────────

void UHexapodNetworkComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!ListenSocket) return;

	uint32 PendingSize = 0;
	while (ListenSocket->HasPendingData(PendingSize) && PendingSize > 0)
	{
		// 버퍼 할당 (+1: null 종단 문자)
		TArray<uint8> Buffer;
		Buffer.SetNum(static_cast<int32>(PendingSize) + 1);

		int32 BytesRead = 0;
		TSharedRef<FInternetAddr> SenderAddr =
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();

		if (ListenSocket->RecvFrom(Buffer.GetData(), Buffer.Num(), BytesRead, *SenderAddr)
		    && BytesRead > 0)
		{
			Buffer[BytesRead] = 0;  // null 종단
			FString Packet = UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData()));
			Packet.TrimEndInline();

			const FString IP   = SenderAddr->ToString(false);
			const int32   Port = SenderAddr->GetPort();

			ProcessPacket(Packet, IP, Port);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 수신 패킷 처리
// ─────────────────────────────────────────────────────────────────────────────

void UHexapodNetworkComponent::ProcessPacket(const FString& Packet,
                                              const FString& SenderIP, int32 SenderPort)
{
	TArray<FString> Tokens;
	Packet.ParseIntoArray(Tokens, TEXT(" "), true);
	if (Tokens.Num() == 0) return;

	const FString& Cmd = Tokens[0];

	// ── JOINTS a0 a1 ... a17 ─────────────────────────────────────────────────
	if (Cmd == TEXT("JOINTS") && Tokens.Num() == 19 && HexapodRobot)
	{
		TArray<float> Angles;
		Angles.SetNum(18);
		for (int32 i = 0; i < 18; i++)
			Angles[i] = FCString::Atof(*Tokens[i + 1]);

		HexapodRobot->ApplyJointTargets(Angles);
	}
	// ── INPUT x y ─────────────────────────────────────────────────────────────
	else if (Cmd == TEXT("INPUT") && Tokens.Num() == 3 && MovementComp)
	{
		MovementComp->SetMoveForward(FCString::Atof(*Tokens[1]));
		MovementComp->SetMoveRight  (FCString::Atof(*Tokens[2]));
	}
	// ── RESET ─────────────────────────────────────────────────────────────────
	else if (Cmd == TEXT("RESET") && HexapodRobot)
	{
		TArray<float> Standing;
		Standing.SetNum(18);
		for (int32 i = 0; i < 6; i++)
		{
			Standing[i * 3 + 0] = 0.f;   // Hip
			Standing[i * 3 + 1] = 0.f;   // Thigh
			Standing[i * 3 + 2] = 60.f;  // Calf
		}
		HexapodRobot->ApplyJointTargets(Standing);
	}
	// ── OBS_REQ (그 외 명령) : 아무 동작 없이 관측값만 반환 ──────────────────

	// 모든 패킷에 대해 관측값 전송
	if (bSendObservations && HexapodRobot)
		SendObservation(SenderIP, SenderPort);
}

// ─────────────────────────────────────────────────────────────────────────────
// 관측값 전송 (UE5 → Python)
// 포맷: "OBS a0 a1 ... a17 px py pz roll pitch yaw\n"
// ─────────────────────────────────────────────────────────────────────────────

void UHexapodNetworkComponent::SendObservation(const FString& IP, int32 Port)
{
	if (!ListenSocket) return;

	const TArray<float> Angles = HexapodRobot->GetJointAngles();
	const FVector       Pos    = HexapodRobot->GetActorLocation();
	const FRotator      Rot    = HexapodRobot->GetActorRotation();

	FString Msg = TEXT("OBS");
	for (float A : Angles)
		Msg += FString::Printf(TEXT(" %.4f"), A);
	Msg += FString::Printf(TEXT(" %.4f %.4f %.4f %.4f %.4f %.4f\n"),
	                       Pos.X, Pos.Y, Pos.Z, Rot.Roll, Rot.Pitch, Rot.Yaw);

	const FTCHARToUTF8 Converted(*Msg);
	const uint8* Data    = reinterpret_cast<const uint8*>(Converted.Get());
	const int32  DataLen = Converted.Length();

	ISocketSubsystem*       SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	TSharedPtr<FInternetAddr> Dest    = SocketSub->CreateInternetAddr();
	bool bValid = false;
	Dest->SetIp(*IP, bValid);
	Dest->SetPort(Port);

	if (bValid)
	{
		int32 Sent = 0;
		ListenSocket->SendTo(Data, DataLen, Sent, *Dest);
	}
}
