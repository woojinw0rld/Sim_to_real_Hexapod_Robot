"""
hexapod_interface.py — Hexapod Sim-to-Real PC 인터페이스

UE5 시뮬레이터와 실제 로봇(Pico)을 동시에 제어하는 통합 Python 인터페이스.

== 사용법 ==
    from hexapod_interface import HexapodInterface

    # UE5 시뮬레이터만
    iface = HexapodInterface(mode='sim')

    # 실제 로봇만
    iface = HexapodInterface(mode='robot', robot_port='COM3')

    # 둘 다 (Sim-to-Real 동기화)
    iface = HexapodInterface(mode='both', robot_port='COM3')

    # 18개 관절 각도 전송 (도 단위)
    obs = iface.send_joints([0, 0, 60] * 6)
    print(obs['angles'], obs['pos'], obs['rot'])

    # UE5 MovementComponent 보행 입력 (기존 Tripod Gait 사용 시)
    iface.send_input(x=1.0, y=0.0)   # 전진
    iface.send_input(x=0.0, y=0.5)   # 우회전

    # 서있는 자세로 리셋
    iface.reset()

    iface.close()

== 프로토콜 ==
    Python → UE5 (UDP):
        "JOINTS a0 a1 ... a17"   → ApplyJointTargets (18개 각도)
        "INPUT  x  y"            → SetMoveForward / SetMoveRight
        "RESET"                  → 서있는 자세
        "OBS_REQ"                → 관측값만 요청

    UE5 → Python (UDP 응답):
        "OBS a0...a17 px py pz roll pitch yaw"

    Python → Pico (Serial):
        동일한 텍스트 프로토콜 (JOINTS / RESET)

== 다리 인덱스 매핑 ==
    UE5 Leg:  0(뒤R)  1(중R)  2(앞R)  3(뒤L)  4(중L)  5(앞L)
    실제 로봇: R3      R2      R1      L3      L2      L1

== 관절 인덱스 ==
    angles[leg*3+0] = Hip   (Coxa)
    angles[leg*3+1] = Thigh (Femur)
    angles[leg*3+2] = Calf  (Tibia)
"""

import socket
import time
from typing import Optional

try:
    import serial
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False


# ─────────────────────────────────────────────────────────────────────────────
# 각도 → 서보 펄스 변환 파라미터
# (exinterface.txt 캘리브레이션 값과 동기화)
# ─────────────────────────────────────────────────────────────────────────────

# UE5 서있는 자세 기준 각도 [Hip, Thigh, Calf]
STANDING_DEG: list = [0.0, 0.0, 60.0]

# 각도 → 펄스 변환 비율 (μs/°) [Hip, Thigh, Calf]
# 튜닝 기준:
#   Hip:   MaxStride(25°) ≈ TRIPOD_GAIN_XY(170μs)  → 170/25 ≈ 6.8
#   Thigh: LiftAngle(40°) ≈ TRIPOD_GAIN_Z(170μs)   → 170/40 ≈ 4.25
#   Calf:  Tibia는 Thigh의 60% 적용               → 동일 비율 사용
US_PER_DEG: list = [6.8, 5.0, 5.0]

MIN_PULSE = 600
MAX_PULSE = 2200

# UE5 다리 인덱스 → 실제 로봇 다리명
UE5_TO_REAL_LEG: list = ['R3', 'R2', 'R1', 'L3', 'L2', 'L1']

# 실제 로봇 캘리브레이션 (exinterface.txt 에서 가져온 값)
CALIBRATION: dict = {
    # 오른쪽
    'R11': {'avg': 1491, 'inv': False},   # R3 Hip
    'R12': {'avg': 1524, 'inv': False},   # R3 Thigh
    'R13': {'avg': 1494, 'inv': False},   # R3 Calf
    'R21': {'avg': 1391, 'inv': False},   # R2 Hip
    'R22': {'avg': 1388, 'inv': False},   # R2 Thigh
    'R23': {'avg': 1493, 'inv': False},   # R2 Calf
    'R31': {'avg': 1475, 'inv': False},   # R1 Hip
    'R32': {'avg': 1607, 'inv': False},   # R1 Thigh
    'R33': {'avg': 1694, 'inv': False},   # R1 Calf
    # 왼쪽
    'L11': {'avg': 1415, 'inv': False},   # L3 Hip
    'L12': {'avg': 1497, 'inv': True},    # L3 Thigh (반전)
    'L13': {'avg': 1511, 'inv': True},    # L3 Calf  (반전)
    'L21': {'avg': 1464, 'inv': False},   # L2 Hip
    'L22': {'avg': 1489, 'inv': True},    # L2 Thigh (반전)
    'L23': {'avg': 1383, 'inv': True},    # L2 Calf  (반전)
    'L31': {'avg': 1625, 'inv': False},   # L1 Hip
    'L32': {'avg': 1527, 'inv': True},    # L1 Thigh (반전)
    'L33': {'avg': 1437, 'inv': True},    # L1 Calf  (반전)
}


# ─────────────────────────────────────────────────────────────────────────────
# 변환 유틸리티
# ─────────────────────────────────────────────────────────────────────────────

def angle_to_pulse(leg_name: str, joint_idx: int, angle_deg: float) -> int:
    """
    UE5 관절 각도(도) → 서보 펄스폭(μs) 변환.

    Args:
        leg_name:  실제 로봇 다리명 ('R1'~'R3', 'L1'~'L3')
        joint_idx: 관절 인덱스 (0=Hip, 1=Thigh, 2=Calf)
        angle_deg: UE5 기준 관절 각도 (도)

    Returns:
        서보 펄스폭 (μs), MIN_PULSE~MAX_PULSE 범위로 클램핑
    """
    key = f"{leg_name}{joint_idx + 1}"   # 예: 'R31', 'L12'
    if key not in CALIBRATION:
        return 1500  # 기본 중앙값

    cal = CALIBRATION[key]
    offset = (angle_deg - STANDING_DEG[joint_idx]) * US_PER_DEG[joint_idx]
    if cal['inv']:
        offset = -offset

    return max(MIN_PULSE, min(MAX_PULSE, int(cal['avg'] + offset)))


def angles_to_pulses(ue5_angles: list) -> dict:
    """
    18개 UE5 관절 각도 리스트 → 실제 로봇 서보 펄스 딕셔너리 변환.

    Returns:
        {'R11': 1491, 'R12': 1600, ..., 'L33': 1437}
    """
    pulses = {}
    for leg_idx in range(6):
        leg_name = UE5_TO_REAL_LEG[leg_idx]
        for joint_idx in range(3):
            angle = ue5_angles[leg_idx * 3 + joint_idx]
            key = f"{leg_name}{joint_idx + 1}"
            pulses[key] = angle_to_pulse(leg_name, joint_idx, angle)
    return pulses


def parse_observation(raw: str) -> dict:
    """
    UE5 OBS 패킷 파싱.
    "OBS a0 a1 ... a17 px py pz roll pitch yaw"

    Returns:
        {'angles': [18 floats], 'pos': [x,y,z], 'rot': [roll,pitch,yaw]}
        또는 {} (파싱 실패 시)
    """
    tokens = raw.strip().split()
    if len(tokens) != 25 or tokens[0] != 'OBS':
        return {}
    values = list(map(float, tokens[1:]))
    return {
        'angles': values[:18],
        'pos':    values[18:21],
        'rot':    values[21:24],
    }


# ─────────────────────────────────────────────────────────────────────────────
# 메인 인터페이스 클래스
# ─────────────────────────────────────────────────────────────────────────────

class HexapodInterface:
    """
    Hexapod Sim-to-Real 통합 인터페이스.

    Parameters
    ----------
    mode        : 'sim'   → UE5만
                  'robot' → 실제 로봇만
                  'both'  → UE5 + 실제 로봇 동기화
    sim_host    : UE5 실행 PC IP (기본 'localhost')
    sim_port    : HexapodNetworkComponent 수신 포트 (기본 7777)
    robot_port  : 시리얼 포트 ('COM3', '/dev/ttyACM0' 등)
    robot_baud  : 시리얼 보레이트 (기본 115200)
    timeout     : UDP/Serial 수신 타임아웃(초)
    """

    def __init__(
        self,
        mode: str = 'sim',
        sim_host: str = '127.0.0.1',
        sim_port: int = 7777,
        robot_port: Optional[str] = None,
        robot_baud: int = 115200,
        timeout: float = 0.1,
    ):
        self.mode    = mode
        self.timeout = timeout

        # ── UE5 UDP 소켓 ──────────────────────────────────────────────────────
        self._udp: Optional[socket.socket] = None
        self._sim_addr = (sim_host, sim_port)
        if mode in ('sim', 'both'):
            self._udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self._udp.settimeout(timeout)
            print(f"[HexapodInterface] UE5 UDP → {sim_host}:{sim_port}")

        # ── 실제 로봇 시리얼 ──────────────────────────────────────────────────
        self._ser = None
        if mode in ('robot', 'both') and robot_port:
            if not HAS_SERIAL:
                raise ImportError(
                    "pyserial 이 필요합니다: pip install pyserial"
                )
            import serial  # noqa: PLC0415
            self._ser = serial.Serial(robot_port, robot_baud, timeout=timeout)
            time.sleep(0.5)  # 포트 안정화 대기
            print(f"[HexapodInterface] 실제 로봇 Serial → {robot_port} @ {robot_baud}")

    # ─────────────────────────────────────────────────────────────────────────
    # 퍼블릭 API
    # ─────────────────────────────────────────────────────────────────────────

    def send_joints(self, angles: list) -> dict:
        """
        18개 관절 각도를 UE5(UDP)와 실제 로봇(Serial)에 동시 전송.

        Args:
            angles: [Hip0, Thigh0, Calf0, Hip1, ..., Calf5] — 도(degree) 단위
                    인덱싱: angles[leg*3+0]=Hip, [leg*3+1]=Thigh, [leg*3+2]=Calf

        Returns:
            UE5 관측값 딕셔너리 {'angles': [...], 'pos': [...], 'rot': [...]}
            UE5 없거나 타임아웃 시 {}
        """
        if len(angles) != 18:
            raise ValueError(f"관절 각도는 18개여야 합니다. 입력: {len(angles)}개")

        packet = "JOINTS " + " ".join(f"{a:.4f}" for a in angles)

        # UE5 전송
        if self._udp:
            self._udp.sendto(packet.encode(), self._sim_addr)

        # 실제 로봇 전송
        if self._ser:
            self._ser.write((packet + "\n").encode())

        return self._recv_observation()

    def send_input(self, x: float, y: float):
        """
        UE5 HexapodMovementComponent 에 이동 입력 전송.
        기존 Tripod Gait 보행 로직을 Python에서 제어할 때 사용.

        Args:
            x: 전진(+1.0) / 후진(-1.0)
            y: 우회전(+1.0) / 좌회전(-1.0)
        """
        packet = f"INPUT {x:.4f} {y:.4f}"
        if self._udp:
            self._udp.sendto(packet.encode(), self._sim_addr)
        # 참고: 실제 로봇에 INPUT 명령은 직접 적용 안 됨 (Pico는 각도만 처리)

    def reset(self) -> dict:
        """
        서있는 자세로 리셋 (Hip=0°, Thigh=0°, Calf=60°).
        UE5와 실제 로봇 모두 적용.

        Returns:
            UE5 관측값 딕셔너리
        """
        if self._udp:
            self._udp.sendto(b"RESET", self._sim_addr)
        if self._ser:
            self._ser.write(b"RESET\n")
        return self._recv_observation()

    def get_observation(self) -> dict:
        """
        UE5에서 현재 관측값만 요청 (관절 각도 변경 없이).

        Returns:
            {'angles': [18 floats], 'pos': [x,y,z], 'rot': [roll,pitch,yaw]}
        """
        if self._udp:
            self._udp.sendto(b"OBS_REQ", self._sim_addr)
        return self._recv_observation()

    def close(self):
        """소켓 및 시리얼 포트 닫기."""
        if self._udp:
            self._udp.close()
            self._udp = None
        if self._ser:
            self._ser.close()
            self._ser = None
        print("[HexapodInterface] 연결 종료")

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    # ─────────────────────────────────────────────────────────────────────────
    # 내부 헬퍼
    # ─────────────────────────────────────────────────────────────────────────

    def _recv_observation(self) -> dict:
        """UE5로부터 OBS 패킷 수신 및 파싱."""
        if not self._udp:
            return {}
        try:
            data, _ = self._udp.recvfrom(4096)
            return parse_observation(data.decode())
        except (socket.timeout, UnicodeDecodeError):
            return {}


# ─────────────────────────────────────────────────────────────────────────────
# 실행 예시
# ─────────────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    import math

    print("=== Hexapod Interface 테스트 ===")

    # mode='sim': UE5만 연결 (실제 로봇 없이 테스트)
    # mode='both', robot_port='COM3': 둘 다 연결
    with HexapodInterface(mode='sim', sim_host='127.0.0.1', sim_port=7777) as iface:

        # ── 1) 서있는 자세 리셋 ──────────────────────────────────────────────
        print("\n[1] 리셋...")
        obs = iface.reset()
        if obs:
            print(f"  관절 각도 (앞 6개): {obs['angles'][:6]}")
            print(f"  로봇 위치: {obs['pos']}")
        time.sleep(1.0)

        # ── 2) 정현파 Hip 스윙 (Swing 테스트) ───────────────────────────────
        print("\n[2] Hip 스윙 테스트 (±20°, 2초)...")
        for step in range(100):
            phase = step / 100.0
            angles = []
            for leg in range(6):
                hip   = math.sin(2 * math.pi * phase) * 20.0   # ±20°
                thigh = 0.0
                calf  = 60.0
                angles += [hip, thigh, calf]
            iface.send_joints(angles)
            time.sleep(0.02)

        # ── 3) UE5 MovementComponent 전진 입력 ───────────────────────────────
        print("\n[3] 전진 입력 테스트 (1초)...")
        for _ in range(50):
            iface.send_input(x=1.0, y=0.0)
            time.sleep(0.02)
        iface.send_input(x=0.0, y=0.0)  # 정지

        # ── 4) 관측값만 요청 ─────────────────────────────────────────────────
        print("\n[4] 현재 관측값:")
        obs = iface.get_observation()
        if obs:
            print(f"  각도: {[f'{a:.1f}' for a in obs['angles']]}")
            print(f"  위치: {obs['pos']}")
            print(f"  자세: {obs['rot']}")
        else:
            print("  (UE5 응답 없음 — 게임이 실행 중인지 확인)")

        print("\n완료!")
