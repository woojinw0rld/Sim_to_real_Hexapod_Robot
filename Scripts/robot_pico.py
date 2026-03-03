"""
robot_pico.py — Hexapod Pico 서보 수신 제어기 (MicroPython)

PC의 hexapod_interface.py 에서 전송한 JOINTS 명령을 수신하여
서보에 직접 적용하는 "각도 수신형" 컨트롤러.

기존 exinterface.txt 의 gait 로직은 이제 PC Python 쪽에서 담당.
Pico는 각도 → 서보 펄스 변환만 수행.

== 수신 프로토콜 (USB 시리얼 or UART) ==
    "JOINTS a0 a1 ... a17\\n"   → 18개 관절 각도 적용
    "RESET\\n"                  → 모든 서보 중앙(avg) 위치로 복귀

== 응답 ==
    "OK\\n"  또는  "ERR\\n"

== 하드웨어 ==
    Pimoroni Servo2040 (MicroPython)
    USB 시리얼: sys.stdin/stdout (기본)
    UART:       uart = UART(1, ...) 주석 해제 후 사용
"""

import sys
import time
from servo import ServoCluster, servo2040
from machine import Pin, UART  # noqa: F401 — UART는 주석 해제 시 사용

# ─────────────────────────────────────────────────────────────────────────────
# [안전장치] 부팅 시 BOOT 버튼 → 코드 즉시 정지
# ─────────────────────────────────────────────────────────────────────────────
boot_button = Pin(servo2040.USER_SW, Pin.IN, Pin.PULL_UP)
if boot_button.value() == 0:
    print(">>> [SAFETY MODE] Boot button detected. Stopped.")
    sys.exit()

# ─────────────────────────────────────────────────────────────────────────────
# 통신 설정
# USB 시리얼: sys.stdin (PC ↔ Pico USB 연결 시 기본값)
# UART:       아래 줄 주석 해제 후 sys.stdin 대신 uart.readline() 사용
# ─────────────────────────────────────────────────────────────────────────────
# uart = UART(1, 115200, tx=Pin(20), rx=Pin(21))

# ─────────────────────────────────────────────────────────────────────────────
# 캘리브레이션 (exinterface.txt 와 동일)
# key 형식: 'R11' = R1다리 1번 관절(Hip), 'L32' = L3다리 2번 관절(Thigh)
# ─────────────────────────────────────────────────────────────────────────────
calibration = {
    # 오른쪽 앞 (R1) — UE5 Leg2
    'R11': {'avg': 1491, 'pin': servo2040.SERVO_13, 'inv': False},
    'R12': {'avg': 1524, 'pin': servo2040.SERVO_14, 'inv': False},
    'R13': {'avg': 1494, 'pin': servo2040.SERVO_15, 'inv': False},
    # 오른쪽 중 (R2) — UE5 Leg1
    'R21': {'avg': 1391, 'pin': servo2040.SERVO_7,  'inv': False},
    'R22': {'avg': 1388, 'pin': servo2040.SERVO_8,  'inv': False},
    'R23': {'avg': 1493, 'pin': servo2040.SERVO_9,  'inv': False},
    # 오른쪽 뒤 (R3) — UE5 Leg0
    'R31': {'avg': 1475, 'pin': servo2040.SERVO_1,  'inv': False},
    'R32': {'avg': 1607, 'pin': servo2040.SERVO_2,  'inv': False},
    'R33': {'avg': 1694, 'pin': servo2040.SERVO_3,  'inv': False},
    # 왼쪽 앞 (L1) — UE5 Leg5
    'L11': {'avg': 1415, 'pin': servo2040.SERVO_16, 'inv': False},
    'L12': {'avg': 1497, 'pin': servo2040.SERVO_17, 'inv': True},
    'L13': {'avg': 1511, 'pin': servo2040.SERVO_18, 'inv': True},
    # 왼쪽 중 (L2) — UE5 Leg4
    'L21': {'avg': 1464, 'pin': servo2040.SERVO_10, 'inv': False},
    'L22': {'avg': 1489, 'pin': servo2040.SERVO_11, 'inv': True},
    'L23': {'avg': 1383, 'pin': servo2040.SERVO_12, 'inv': True},
    # 왼쪽 뒤 (L3) — UE5 Leg3
    'L31': {'avg': 1625, 'pin': servo2040.SERVO_4,  'inv': False},
    'L32': {'avg': 1527, 'pin': servo2040.SERVO_5,  'inv': True},
    'L33': {'avg': 1437, 'pin': servo2040.SERVO_6,  'inv': True},
}

# ─────────────────────────────────────────────────────────────────────────────
# 각도 변환 파라미터 (hexapod_interface.py 와 동일하게 유지)
# ─────────────────────────────────────────────────────────────────────────────
STANDING_DEG = [0.0, 0.0, 60.0]    # [Hip, Thigh, Calf] 서있는 자세 기준값
US_PER_DEG   = [6.8, 5.0, 5.0]     # [Hip, Thigh, Calf] 각도→펄스 변환비율
MIN_PULSE    = 600
MAX_PULSE    = 2200

# UE5 다리 인덱스 → 실제 로봇 다리명 (hexapod_interface.py 와 동일)
# Leg0=R3, Leg1=R2, Leg2=R1, Leg3=L3, Leg4=L2, Leg5=L1
UE5_TO_REAL_LEG = ['R3', 'R2', 'R1', 'L3', 'L2', 'L1']

# ─────────────────────────────────────────────────────────────────────────────
# 서보 초기화
# ─────────────────────────────────────────────────────────────────────────────
cluster      = None
pin_to_index = {}


def init_servos():
    global cluster, pin_to_index
    pins_mask = 0
    sorted_pins = sorted([calibration[k]['pin'] for k in calibration])
    for i, pin in enumerate(sorted_pins):
        pins_mask |= (1 << pin)
        pin_to_index[pin] = i
    cluster = ServoCluster(0, 0, pins_mask)
    cluster.enable_all()
    time.sleep(0.3)
    print("Servos initialized.")


def send_pulse(pin, pulse_us):
    if pin in pin_to_index:
        p = max(MIN_PULSE, min(MAX_PULSE, int(pulse_us)))
        cluster.pulse(pin_to_index[pin], p)


def center_all():
    """모든 서보를 기본 위치(avg)로 이동 — 서있는 자세."""
    for key in calibration:
        send_pulse(calibration[key]['pin'], calibration[key]['avg'])


# ─────────────────────────────────────────────────────────────────────────────
# 각도 → 펄스 변환 (hexapod_interface.py 의 angle_to_pulse() 와 동일 로직)
# ─────────────────────────────────────────────────────────────────────────────

def angle_to_pulse(leg_name, joint_idx, angle_deg):
    """
    UE5 관절 각도(도) → 서보 펄스폭(μs) 변환.

    Args:
        leg_name  : 실제 로봇 다리명 ('R1'~'R3', 'L1'~'L3')
        joint_idx : 0=Hip, 1=Thigh, 2=Calf
        angle_deg : UE5 기준 각도 (도)
    """
    key = leg_name + str(joint_idx + 1)
    if key not in calibration:
        return 1500
    cal = calibration[key]
    offset = (angle_deg - STANDING_DEG[joint_idx]) * US_PER_DEG[joint_idx]
    if cal['inv']:
        offset = -offset
    return max(MIN_PULSE, min(MAX_PULSE, int(cal['avg'] + offset)))


def apply_joint_angles(angles):
    """18개 UE5 관절 각도를 서보에 동시 적용."""
    if len(angles) != 18:
        return
    for leg_idx in range(6):
        leg_name = UE5_TO_REAL_LEG[leg_idx]
        for joint_idx in range(3):
            angle = angles[leg_idx * 3 + joint_idx]
            pulse = angle_to_pulse(leg_name, joint_idx, angle)
            key = leg_name + str(joint_idx + 1)
            send_pulse(calibration[key]['pin'], pulse)


# ─────────────────────────────────────────────────────────────────────────────
# 명령 파싱 및 처리
# ─────────────────────────────────────────────────────────────────────────────

def process_command(line):
    """
    한 줄 명령 처리.

    지원 명령:
        "JOINTS a0 a1 ... a17"  → 18개 각도 적용
        "RESET"                 → 서있는 자세 복귀

    Returns:
        'OK' 또는 'ERR'
    """
    parts = line.strip().split()
    if not parts:
        return 'ERR'

    cmd = parts[0].upper()

    if cmd == 'JOINTS' and len(parts) == 19:
        try:
            angles = [float(x) for x in parts[1:]]
            apply_joint_angles(angles)
            return 'OK'
        except ValueError:
            return 'ERR'

    elif cmd == 'RESET':
        center_all()
        return 'OK'

    return 'ERR'


# ─────────────────────────────────────────────────────────────────────────────
# 메인 루프 — USB 시리얼 수신
# ─────────────────────────────────────────────────────────────────────────────

def main():
    init_servos()
    center_all()
    print("=== Hexapod Pico Controller ===")
    print("Protocol: JOINTS a0..a17 | RESET")
    print("Listening on USB serial...")

    buf = ''
    try:
        while True:
            ch = sys.stdin.read(1)
            if ch == '\n':
                result = process_command(buf)
                sys.stdout.write(result + '\n')
                buf = ''
            else:
                buf += ch
    except KeyboardInterrupt:
        print("\nTerminated.")
        if cluster:
            cluster.disable_all()


if __name__ == '__main__':
    main()
