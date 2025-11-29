#!/usr/bin/env python3
"""
RTCM 메시지 파서 - 메시지 타입 확인용
"""

def parse_rtcm_messages(data):
    """RTCM 메시지에서 메시지 타입을 추출"""
    i = 0
    messages = []

    while i < len(data):
        # Preamble 찾기 (0xD3)
        if data[i] == 0xD3:
            if i + 5 >= len(data):  # 최소 헤더 크기 확인
                break

            # Length 파싱 (2바이트, 10비트)
            byte1 = data[i + 1]
            byte2 = data[i + 2]
            msg_len = ((byte1 & 0x03) << 8) | byte2

            # 전체 패킷 길이
            total_len = 3 + msg_len + 3  # header(3) + payload + CRC(3)

            if i + total_len > len(data):
                print(f"⚠️  위치 {i}: 불완전한 RTCM 패킷 (필요: {total_len}, 남은: {len(data)-i})")
                break

            # 메시지 타입 파싱 (페이로드의 첫 12비트)
            if msg_len >= 2:
                payload_start = i + 3
                msg_type = (data[payload_start] << 4) | ((data[payload_start + 1] >> 4) & 0x0F)

                messages.append({
                    'offset': i,
                    'msg_type': msg_type,
                    'length': msg_len,
                    'total_len': total_len
                })

                print(f"📡 위치 {i}: RTCM 메시지 타입 {msg_type}, 길이: {msg_len} bytes")

                # 1005 메시지 찾기
                if msg_type == 1005:
                    print(f"   ✅ RTCM 1005 메시지 발견!")
                    print(f"   패킷 데이터: {data[i:i+total_len].hex(' ')}")

                i += total_len
            else:
                print(f"⚠️  위치 {i}: 페이로드가 너무 짧음 ({msg_len} bytes)")
                i += 1
        else:
            i += 1

    return messages

def main():
    # 사용자가 제공한 데이터 (hex 형식으로 변환)
    # 첫 번째 메시지
    data1_hex = "D3 00 50 43 20 00 8A ED DB 5E 00 00 03 04 00 02 00 00 00 00 20 00 80 00 7D A4 A9 26 28 F9 C3 1E 02 83 75 23 E8 64 7F E8 F0 C0 D5 C2 01 63 F9 BE B3 20 FA 16 A8 40 1A 20 F2 62 01 C0 37 07 87 50 29 E2 4C AC AB B8 01 1B F0 BA DC 2F 80 00 00 00 00 00 00 04 45 7B D3"

    # 두 번째 메시지
    data2_hex = "D3 00 33 44 60 00 8A ED DB 5C 00 00 10 00 00 02 00 00 00 00 20 01 00 00 7A 9A E1 A3 53 77 9C EC 46 6C 68 EC 87 E1 34 7F 5E 79 07 13 80 1D 6C 95 D4 38 3D 03 AD 00 29 B9 A2"

    print("=" * 70)
    print("RTCM 메시지 분석")
    print("=" * 70)

    print("\n[데이터 세트 1 분석]")
    data1 = bytes.fromhex(data1_hex.replace(' ', ''))
    print(f"총 데이터 크기: {len(data1)} bytes")
    print(f"원본 데이터: {data1.hex(' ')}\n")
    messages1 = parse_rtcm_messages(data1)

    print("\n" + "=" * 70)
    print("\n[데이터 세트 2 분석]")
    data2 = bytes.fromhex(data2_hex.replace(' ', ''))
    print(f"총 데이터 크기: {len(data2)} bytes")
    print(f"원본 데이터: {data2.hex(' ')}\n")
    messages2 = parse_rtcm_messages(data2)

    # 결과 요약
    print("\n" + "=" * 70)
    print("요약")
    print("=" * 70)
    all_messages = messages1 + messages2

    if all_messages:
        print(f"총 발견된 RTCM 메시지: {len(all_messages)}개")
        for msg in all_messages:
            print(f"  - 메시지 타입 {msg['msg_type']} (오프셋: {msg['offset']}, 길이: {msg['length']} bytes)")

        # 1005 메시지 확인
        msg_1005 = [m for m in all_messages if m['msg_type'] == 1005]
        if msg_1005:
            print(f"\n✅ RTCM 1005 메시지 발견: {len(msg_1005)}개")
        else:
            print(f"\n❌ RTCM 1005 메시지 없음")
    else:
        print("발견된 RTCM 메시지가 없습니다.")

if __name__ == "__main__":
    main()
