"""
realtime_control.py — Hexapod 실시간 키보드 제어 (PC → UE5 + Servo2040 동시)

키보드 입력을 받아 Tripod Gait를 계산하고
UE5(UDP)와 Servo2040(Serial)에 동시에 전송합니다.

== 사용법 ==
    python Scripts/realtime_control.py

    # Servo2040 없이 UE5만 테스트:
    ROBOT_PORT = None

== 키 맵 ==
    W       전진
    S       후진
    A       좌회전
    D       우회전
    스페이스 정지 (서있는 자세)
    Q / ESC 종료

== 요구사항 ==
    pip install pyserial
    UE5 게임 실행 중 (Play 버튼)
"""

import socket
import time
import math
import sys

try:
    import serial
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

try:
    import msvcrt          # Windows 전용
    HAS_MSVCRT = True
except ImportError:
    HAS_MSVCRT = False

# ─────────────────────────────────────────────────────────────────────────────
# 설정
# ─────────────────────────────────────────────────────────────────────────────

SIM_HOST   = '127.0.0.1'
SIM_PORT   = 7777

ROBOT_PORT = 'COM5'        # Servo2040 포트 (없으면 None)
ROBOT_BAUD = 115200

LOOP_HZ    = 20            # 제어 루프 주파수 (초당 프레임)
LOOP_DT    = 1.0 / LOOP_HZ

# ─────────────────────────────────────────────────────────────────────────────
# Tripod Gait 파라미터 (HexapodMovementComponent 와 동일)
# ─────────────────────────────────────────────────────────────────────────────

MAX_STRIDE  = 25.0   # Hip 최대 스트라이드 각도 (도)
LIFT_ANGLE  = 30.0   # Thigh 들어올리는 각도 (도)
GAIT_SPEED  = 1.2    # 보행 사이클 속도 (Hz)
TURN_RATE   = 15.0   # 제자리 회전 시 차동 값

# 서있는 자세 기준값
HIP_STAND   =  0.0
THIGH_STAND =  0.0
CALF_STAND  = 60.0

# Tripod 그룹
# Group A: Leg5(왼앞) Leg1(오른중) Leg3(왼뒤)
# Group B: Leg2(오른앞) Leg4(왼중) Leg0(오른뒤)
GROUP_A = [5, 1, 3]
GROUP_B = [2, 4, 0]

# 왼쪽 다리 여부 (차동구동에서 stride 방향 반전용)
IS_LEFT = [False, False, False, True, True, True]  # Leg 0~5


def compute_tripod_angles(phase: float, fwd: float, turn: float) -> list:
    """
    Tripod Gait 관절 각도 계산.

    Args:
        phase : 0.0 ~ 1.0 보행 사이클 위상
        fwd   : 전진(+1) / 후진(-1)
        turn  : 우회전(+1) / 좌회전(-1)

    Returns:
        18개 float 리스트 [Hip0, Thigh0, Calf0, Hip1, ..., Calf5]

    이 로봇은 차동(좌우 반대 방향) = 직진, 동일 방향 = 회전
    """
    angles = []

    # 차동구동 — 이 로봇은 left/right 방향이 반대일 때 직진
    right_stride = -fwd * MAX_STRIDE + turn * TURN_RATE
    left_stride  =  fwd * MAX_STRIDE + turn * TURN_RATE

    phase_b = (phase + 0.5) % 1.0

    for leg in range(6):
        stride = left_stride if IS_LEFT[leg] else right_stride

        # 이 다리의 위상
        if leg in GROUP_A:
            p = phase
        else:
            p = phase_b

        # Swing(0.0~0.5): 발을 들고 앞으로 이동
        # Stance(0.5~1.0): 발을 내려 뒤로 밀기
        if p < 0.5:
            # Swing phase
            swing_t = p / 0.5          # 0→1
            hip   = stride * (swing_t * 2 - 1)   # 앞쪽(+stride) 에서 뒤쪽(-stride)
            thigh = THIGH_STAND + LIFT_ANGLE * math.sin(math.pi * swing_t)
            calf  = CALF_STAND
        else:
            # Stance phase
            stance_t = (p - 0.5) / 0.5   # 0→1
            hip   = stride * (1 - stance_t * 2)  # +stride → -stride
            thigh = THIGH_STAND
            calf  = CALF_STAND

        angles += [hip, thigh, calf]

    return angles


def make_joints_packet(angles: list) -> bytes:
    body = " ".join(f"{a:.4f}" for a in angles)
    return f"JOINTS {body}".encode()


def make_standing_packet() -> bytes:
    angles = [HIP_STAND, THIGH_STAND, CALF_STAND] * 6
    return make_joints_packet(angles)


# ─────────────────────────────────────────────────────────────────────────────
# 키보드 (Windows msvcrt / 크로스플랫폼 fallback)
# ─────────────────────────────────────────────────────────────────────────────

def get_key_nonblocking() -> str:
    """눌린 키를 즉시 반환. 없으면 '' 반환."""
    if HAS_MSVCRT:
        if msvcrt.kbhit():
            ch = msvcrt.getch()
            if ch in (b'\x00', b'\xe0'):   # 방향키 prefix
                msvcrt.getch()
                return ''
            return ch.decode('utf-8', errors='ignore').lower()
    return ''


# ─────────────────────────────────────────────────────────────────────────────
# 메인 제어 루프
# ─────────────────────────────────────────────────────────────────────────────

def main():
    # ── 소켓 초기화 ──────────────────────────────────────────────────────────
    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp.settimeout(0.05)
    print(f"[UDP] UE5 → {SIM_HOST}:{SIM_PORT}")

    # ── 시리얼 초기화 ─────────────────────────────────────────────────────────
    ser = None
    if ROBOT_PORT and HAS_SERIAL:
        try:
            ser = serial.Serial(ROBOT_PORT, ROBOT_BAUD, timeout=0.05)
            print(f"[Serial] Servo2040 → {ROBOT_PORT} @ {ROBOT_BAUD}")
            time.sleep(2.0)   # 부팅 대기
            print("[Serial] 준비 완료")
        except serial.SerialException as e:
            print(f"[Serial] 연결 실패: {e}")
            ser = None
    elif ROBOT_PORT and not HAS_SERIAL:
        print("[경고] pyserial 없음. pip install pyserial")

    def send(pkt: bytes):
        udp.sendto(pkt, (SIM_HOST, SIM_PORT))
        if ser:
            ser.write(pkt + b'\n')

    # ── 초기 자세 ─────────────────────────────────────────────────────────────
    send(make_standing_packet())
    print("\n=== Hexapod 실시간 제어 ===")
    print("  W   전진    S   후진")
    print("  A   좌회전  D   우회전")
    print("  스페이스  정지")
    print("  Q / ESC   종료\n")

    phase     = 0.0
    fwd, turn = 0.0, 0.0
    moving    = False

    try:
        while True:
            t0 = time.time()

            # ── 키 입력 처리 ──────────────────────────────────────────────────
            key = get_key_nonblocking()
            if key:
                if key in ('q', '\x1b'):          # Q 또는 ESC
                    print("종료")
                    break
                elif key == 'w':
                    fwd, turn = 1.0, 0.0;  moving = True
                elif key == 's':
                    fwd, turn = -1.0, 0.0; moving = True
                elif key == 'a':
                    fwd, turn = 0.0, -1.0; moving = True
                elif key == 'd':
                    fwd, turn = 0.0, 1.0;  moving = True
                elif key == ' ':
                    fwd, turn = 0.0, 0.0;  moving = False

            # ── 각도 계산 및 전송 ─────────────────────────────────────────────
            if moving and (fwd != 0.0 or turn != 0.0):
                phase = (phase + GAIT_SPEED * LOOP_DT) % 1.0
                angles = compute_tripod_angles(phase, fwd, turn)
                pkt = make_joints_packet(angles)
                label = f"fwd={fwd:+.1f} turn={turn:+.1f} phase={phase:.2f}"
            else:
                pkt = make_standing_packet()
                label = "서있는 자세"

            send(pkt)

            # OBS 수신 (비블로킹)
            try:
                data, _ = udp.recvfrom(4096)
                obs_str = data.decode().strip()
                tokens  = obs_str.split()
                if tokens and tokens[0] == 'OBS' and len(tokens) >= 25:
                    pos = tokens[19:22]
                    rot = tokens[22:25]
                    sys.stdout.write(
                        f"\r{label} | "
                        f"pos=[{','.join(f'{float(v):.1f}' for v in pos)}] "
                        f"rot=[{','.join(f'{float(v):.1f}' for v in rot)}]  "
                    )
                    sys.stdout.flush()
            except (socket.timeout, UnicodeDecodeError):
                sys.stdout.write(f"\r{label}  ")
                sys.stdout.flush()

            # ── 루프 속도 맞춤 ────────────────────────────────────────────────
            elapsed = time.time() - t0
            if elapsed < LOOP_DT:
                time.sleep(LOOP_DT - elapsed)

    except KeyboardInterrupt:
        print("\nCtrl+C — 종료")

    finally:
        send(make_standing_packet())
        time.sleep(0.3)
        udp.close()
        if ser:
            ser.close()
        print("연결 종료.")


if __name__ == '__main__':
    main()
