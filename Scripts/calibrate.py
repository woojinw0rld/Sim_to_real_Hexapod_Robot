"""
calibrate.py — 서보 각도 캘리브레이션 도우미

관절 하나씩 테스트 각도로 이동 → 실측값 입력 → 보정 계수 계산

== 사용법 ==
    python Scripts/calibrate.py

== 준비물 ==
    - 각도기 (서보 혼에 붙이면 정확)
    - Servo2040 USB 연결 + main.py 실행 중
    - UE5 Play 실행 (선택, 없으면 ROBOT_ONLY = True)
"""

import socket
import time
import sys

try:
    import serial
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

# ─────────────────────────────────────────────────────────────────────────────
# 설정
# ─────────────────────────────────────────────────────────────────────────────

ROBOT_PORT  = 'COM5'        # Servo2040 포트
ROBOT_BAUD  = 115200
ROBOT_ONLY  = False         # True: UE5 없이 서보만 테스트

SIM_HOST    = '127.0.0.1'
SIM_PORT    = 7777

# ─────────────────────────────────────────────────────────────────────────────
# 현재 캘리브레이션 값 (robot_pico.py / hexapod_interface.py 와 동일)
# ─────────────────────────────────────────────────────────────────────────────

STANDING_DEG = [0.0, 0.0, 60.0]      # [Hip, Thigh, Calf]
US_PER_DEG   = [6.8, 5.0, 5.0]       # [Hip, Thigh, Calf] — 보정 대상

# 테스트에 사용할 기준 각도 (서있는 자세 기준으로 ±이만큼 이동)
TEST_DELTA_DEG = [20.0, 20.0, 20.0]  # [Hip, Thigh, Calf]

# 다리 이름 매핑
JOINT_NAMES = ['Hip', 'Thigh', 'Calf']
LEG_NAMES   = ['Leg0(뒤R=R3)', 'Leg1(중R=R2)', 'Leg2(앞R=R1)',
                'Leg3(뒤L=L3)', 'Leg4(중L=L2)', 'Leg5(앞L=L1)']

# ─────────────────────────────────────────────────────────────────────────────
# 통신 유틸
# ─────────────────────────────────────────────────────────────────────────────

def make_packet(angles: list) -> bytes:
    body = ' '.join(f'{a:.4f}' for a in angles)
    return f'JOINTS {body}'.encode()


def standing_angles() -> list:
    angles = []
    for _ in range(6):
        angles += STANDING_DEG[:]
    return angles


def single_joint_angles(leg: int, joint: int, target_deg: float) -> list:
    """해당 관절만 target_deg, 나머지는 서있는 자세."""
    angles = standing_angles()
    angles[leg * 3 + joint] = target_deg
    return angles


def send(udp, ser, pkt: bytes):
    if udp:
        udp.sendto(pkt, (SIM_HOST, SIM_PORT))
    if ser:
        ser.write(pkt + b'\n')
        time.sleep(0.05)
        ser.read_all()   # 응답 버퍼 비우기


# ─────────────────────────────────────────────────────────────────────────────
# 캘리브레이션 결과 저장
# ─────────────────────────────────────────────────────────────────────────────

corrections = {}   # {joint_idx: [보정비율, ...]} — 다리별 측정값 누적


def record(joint: int, cmd_deg: float, measured_deg: float):
    """보정 비율 기록."""
    if abs(measured_deg) < 0.5:
        print('  [경고] 측정값이 너무 작습니다. 건너뜁니다.')
        return
    ratio = abs(cmd_deg) / abs(measured_deg)
    if joint not in corrections:
        corrections[joint] = []
    corrections[joint].append(ratio)
    new_val = US_PER_DEG[joint] * ratio
    print(f'  → 보정비율 {ratio:.3f}  |  new US_PER_DEG[{JOINT_NAMES[joint]}] 추정: {new_val:.2f}')


def print_summary():
    print('\n' + '='*60)
    print('  캘리브레이션 결과 요약')
    print('='*60)
    print(f'  {"관절":<8}  {"현재 US/deg":<14}  {"보정 US/deg":<14}  {"변화"}')
    print(f'  {"-"*50}')
    for j, name in enumerate(JOINT_NAMES):
        cur = US_PER_DEG[j]
        if j in corrections and corrections[j]:
            avg_ratio = sum(corrections[j]) / len(corrections[j])
            new_val   = cur * avg_ratio
            arrow     = '↑' if new_val > cur else '↓'
            print(f'  {name:<8}  {cur:<14.2f}  {new_val:<14.2f}  {arrow} {abs(new_val-cur):.2f}')
        else:
            print(f'  {name:<8}  {cur:<14.2f}  {"(미측정)":<14}')

    print()
    print('  robot_pico.py / hexapod_interface.py 에 아래 값으로 업데이트하세요:')
    print()
    vals = []
    for j in range(3):
        if j in corrections and corrections[j]:
            avg_ratio = sum(corrections[j]) / len(corrections[j])
            vals.append(US_PER_DEG[j] * avg_ratio)
        else:
            vals.append(US_PER_DEG[j])
    print(f'  US_PER_DEG = [{vals[0]:.2f}, {vals[1]:.2f}, {vals[2]:.2f}]')
    print('='*60)


# ─────────────────────────────────────────────────────────────────────────────
# 메인
# ─────────────────────────────────────────────────────────────────────────────

def main():
    # ── 연결 초기화 ──────────────────────────────────────────────────────────
    udp = None
    if not ROBOT_ONLY:
        udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        udp.settimeout(0.1)
        print(f'[UDP] UE5 연결 → {SIM_HOST}:{SIM_PORT}')

    ser = None
    if HAS_SERIAL and ROBOT_PORT:
        try:
            ser = serial.Serial(ROBOT_PORT, ROBOT_BAUD, timeout=0.2)
            print(f'[Serial] Servo2040 연결 → {ROBOT_PORT}')
            time.sleep(2.0)
            print('[Serial] 준비 완료')
        except Exception as e:
            print(f'[Serial] 연결 실패: {e}')

    if not udp and not ser:
        print('[오류] UE5도 Servo2040도 연결되지 않았습니다.')
        return

    # ── 서있는 자세로 초기화 ─────────────────────────────────────────────────
    send(udp, ser, make_packet(standing_angles()))
    time.sleep(1.0)

    print()
    print('='*60)
    print('  Hexapod 서보 캘리브레이션')
    print('='*60)
    print('  각 관절을 테스트 각도로 이동합니다.')
    print('  각도기로 실제 이동 각도를 측정하여 입력하세요.')
    print('  (서있는 자세 기준으로 얼마나 움직였는지)')
    print('  스킵: Enter만 누르기  |  종료: q + Enter')
    print()

    try:
        while True:
            # ── 관절 선택 ────────────────────────────────────────────────────
            print('[ 관절 선택 ]')
            print('  0: Hip   1: Thigh   2: Calf   s: 결과 보기   q: 종료')
            joint_input = input('  관절 번호 입력: ').strip().lower()

            if joint_input == 'q':
                break
            if joint_input == 's':
                print_summary()
                continue
            if joint_input not in ('0', '1', '2'):
                print('  0, 1, 2 중에서 선택하세요.')
                continue

            joint = int(joint_input)

            # ── 다리 선택 ────────────────────────────────────────────────────
            print(f'\n[ {JOINT_NAMES[joint]} 관절 — 다리 선택 ]')
            for i, name in enumerate(LEG_NAMES):
                print(f'  {i}: {name}')
            print('  a: 전체 다리 동시 테스트')
            leg_input = input('  다리 번호 입력: ').strip().lower()

            if leg_input == 'q':
                break
            if leg_input == '':
                continue

            legs_to_test = []
            if leg_input == 'a':
                legs_to_test = list(range(6))
            elif leg_input.isdigit() and 0 <= int(leg_input) <= 5:
                legs_to_test = [int(leg_input)]
            else:
                print('  0~5 또는 a를 입력하세요.')
                continue

            # ── 방향 선택 ────────────────────────────────────────────────────
            delta = TEST_DELTA_DEG[joint]
            print(f'\n  테스트 각도: STANDING({STANDING_DEG[joint]:.1f}°) + {delta:.1f}° 이동')
            print(f'  + 방향(양수) / - 방향(음수)?')
            dir_input = input('  방향 [+/-] (기본 +): ').strip()
            if dir_input == '-':
                delta = -delta

            target_deg = STANDING_DEG[joint] + delta

            # ── 이동 실행 ────────────────────────────────────────────────────
            if legs_to_test == list(range(6)):
                # 전체 다리
                angles = standing_angles()
                for leg in range(6):
                    angles[leg * 3 + joint] = target_deg
                send(udp, ser, make_packet(angles))
                print(f'\n  → 전체 {JOINT_NAMES[joint]} = {target_deg:.1f}° 전송 완료')
                print(f'     (서있는 자세 {STANDING_DEG[joint]:.1f}°에서 {delta:+.1f}° 이동)')
            else:
                leg = legs_to_test[0]
                angles = single_joint_angles(leg, joint, target_deg)
                send(udp, ser, make_packet(angles))
                print(f'\n  → {LEG_NAMES[leg]} {JOINT_NAMES[joint]} = {target_deg:.1f}° 전송 완료')
                print(f'     (서있는 자세 {STANDING_DEG[joint]:.1f}°에서 {delta:+.1f}° 이동)')

            # ── 실측값 입력 ──────────────────────────────────────────────────
            print()
            print(f'  각도기로 실제 이동 각도를 측정하세요.')
            print(f'  (서있는 자세 기준, {delta:+.1f}° 방향으로 몇 도 이동했나요?)')
            measured_input = input('  실측 이동 각도 입력 (스킵: Enter): ').strip()

            if measured_input == '':
                print('  스킵')
            elif measured_input.lower() == 'q':
                break
            else:
                try:
                    measured_deg = float(measured_input)
                    record(joint, delta, measured_deg)
                except ValueError:
                    print('  숫자를 입력하세요.')

            # 서있는 자세 복귀
            print()
            send(udp, ser, make_packet(standing_angles()))
            time.sleep(0.5)

    except KeyboardInterrupt:
        print('\nCtrl+C — 종료')

    finally:
        send(udp, ser, make_packet(standing_angles()))
        time.sleep(0.3)
        print_summary()
        if udp:
            udp.close()
        if ser:
            ser.close()


if __name__ == '__main__':
    main()
