#!/usr/bin/env python3
"""Minimal ModBus RTU helper for avrModBus targets."""

import argparse
import struct
import uuid
import sys
import time

import serial


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def build_frame(slave: int, function: int, payload: bytes = b"") -> bytes:
    body = bytes([slave & 0xFF, function & 0xFF]) + payload
    crc = crc16(body)
    return body + struct.pack("<H", crc)


def expect_bytes(port: serial.Serial, length: int) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + port.timeout if port.timeout else None
    while len(data) < length:
        chunk = port.read(length - len(data))
        if chunk:
            data.extend(chunk)
            continue
        if deadline is not None and time.monotonic() > deadline:
            break
    return bytes(data)


def read_response(port: serial.Serial, minimum: int = 5) -> bytes:
    header = expect_bytes(port, minimum)
    if len(header) < minimum:
        raise RuntimeError("Timeout waiting for response header")
    if len(header) >= 3:
        byte_count = header[2]
        rest = expect_bytes(port, byte_count + 2)
        return header + rest
    rest = expect_bytes(port, 3)
    return header + rest


def transact(args, function, payload, response_len=None):
    with serial.Serial(args.port, baudrate=args.baud, bytesize=8, parity="N", stopbits=1, timeout=args.timeout) as ser:
        frame = build_frame(args.slave, function, payload)
        ser.reset_input_buffer()
        ser.write(frame)
        ser.flush()
        if response_len is None:
            resp = read_response(ser)
        else:
            resp = expect_bytes(ser, response_len)
            if len(resp) < response_len:
                raise RuntimeError("Timeout waiting for response")
        if len(resp) < 5:
            raise RuntimeError(f"Malformed response: {resp.hex()}")
        if not verify_crc(resp):
            raise RuntimeError(f"CRC mismatch: {resp.hex()}")
        return resp


def verify_crc(frame: bytes) -> bool:
    if len(frame) < 4:
        return False
    data = frame[:-2]
    received = struct.unpack("<H", frame[-2:])[0]
    return crc16(data) == received


def handle_read_registers(args, function):
    payload = struct.pack(">HH", args.address, args.count)
    resp = transact(args, function, payload)
    byte_count = resp[2]
    words = [struct.unpack(">H", resp[3 + i:3 + i + 2])[0] for i in range(0, byte_count, 2)]
    for idx, value in enumerate(words):
        print(f"{args.address + idx}: 0x{value:04X} ({value})")


def handle_read_coils(args):
    payload = struct.pack(">HH", args.address, args.count)
    resp = transact(args, 0x01, payload)
    byte_count = resp[2]
    data = resp[3:3 + byte_count]
    for i in range(args.count):
        byte_index = i // 8
        bit_index = i % 8
        bit = (data[byte_index] >> bit_index) & 0x01
        print(f"{args.address + i}: {bit}")


def handle_write_single(args, function):
    payload = struct.pack(">HH", args.address, args.value)
    resp = transact(args, function, payload, response_len=8)
    print(resp.hex())


def handle_set_config(args):
    if args.device_id is not None:
        handle_write_single(args, 0x06)
    if args.baud_enum is not None:
        payload = struct.pack(">HH", 1, args.baud_enum)
        resp = transact(args, 0x06, payload, response_len=8)
        print(resp.hex())
    if args.apply_reset:
        payload = struct.pack(">HH", 255, 0xAA55)
        resp = transact(args, 0x06, payload, response_len=8)
        print(resp.hex())




def words_to_uuid(words):
    data = b''.join(struct.pack('>H', w & 0xFFFF) for w in words)
    return str(uuid.UUID(bytes=data))


def handle_read_uuids(args):
    payload = struct.pack('>HH', 0, 16)
    resp = transact(args, 0x04, payload)
    byte_count = resp[2]
    if byte_count < 32:
        raise RuntimeError('UUID payload too short')
    words = [struct.unpack('>H', resp[3 + i:3 + i + 2])[0] for i in range(0, 32, 2)]
    device = words_to_uuid(words[:8])
    instance = words_to_uuid(words[8:16])
    print(f'Device type UUID : {device}')
    print(f'Device inst UUID : {instance}')

def handle_write_multiple(args):
    values = [int(v, 0) for v in args.values]
    count = len(values)
    payload = struct.pack(">HHB", args.address, count, count * 2)
    payload += b"".join(struct.pack(">H", v) for v in values)
    resp = transact(args, 0x10, payload, response_len=8)
    print(resp.hex())


def build_parser():
    parser = argparse.ArgumentParser(description="Minimal ModBus RTU helper")
    parser.add_argument("--port", default="/dev/ttyU0", help="Serial port (default: /dev/ttyU0)")
    parser.add_argument("--baud", type=int, default=9600, help="Host serial baud (default: 9600)")
    parser.add_argument("--slave", type=int, default=1, help="Slave address (default: 1)")
    parser.add_argument("--timeout", type=float, default=1.0, help="Serial timeout in seconds")
    sub = parser.add_subparsers(dest="command", required=True)

    read_holding = sub.add_parser("read-holding", help="Read holding registers")
    read_holding.add_argument("address", type=int)
    read_holding.add_argument("count", type=int)
    read_holding.set_defaults(func=lambda a: handle_read_registers(a, 0x03))

    read_input = sub.add_parser("read-input", help="Read input registers")
    read_input.add_argument("address", type=int)
    read_input.add_argument("count", type=int)
    read_input.set_defaults(func=lambda a: handle_read_registers(a, 0x04))

    read_coils = sub.add_parser("read-coils", help="Read coil bits")
    read_coils.add_argument("address", type=int)
    read_coils.add_argument("count", type=int)
    read_coils.set_defaults(func=handle_read_coils)

    write_coil = sub.add_parser("write-coil", help="Write single coil (0 or 1)")
    write_coil.add_argument("address", type=int)
    write_coil.add_argument("value", type=int, choices=[0, 1])
    def set_coil(args):
        value = 0xFF00 if args.value else 0x0000
        args.value = value
        handle_write_single(args, 0x05)
    write_coil.set_defaults(func=set_coil)

    write_holding = sub.add_parser("write-holding", help="Write single holding register")
    write_holding.add_argument("address", type=int)
    write_holding.add_argument("value", type=lambda x: int(x, 0))
    write_holding.set_defaults(func=lambda a: handle_write_single(a, 0x06))

    write_multi = sub.add_parser("write-multi", help="Write multiple holding registers")
    write_multi.add_argument("address", type=int)
    write_multi.add_argument("values", nargs="+", help="Values (e.g., 0x1234 5 6)")
    write_multi.set_defaults(func=handle_write_multiple)

    set_cfg = sub.add_parser("set-config", help="Set device ID / baud enum and optionally apply reset")
    set_cfg.add_argument("--device-id", type=int)
    set_cfg.add_argument("--baud-enum", type=int)
    set_cfg.add_argument("--apply-reset", action="store_true", help="Write reset magic 0xAA55")
    def cfg_func(args):
        if args.device_id is not None:
            payload = struct.pack(">HH", 0, args.device_id)
            resp = transact(args, 0x06, payload, response_len=8)
            print(resp.hex())
        if args.baud_enum is not None:
            payload = struct.pack(">HH", 1, args.baud_enum)
            resp = transact(args, 0x06, payload, response_len=8)
            print(resp.hex())
        if args.apply_reset:
            payload = struct.pack(">HH", 255, 0xAA55)
            resp = transact(args, 0x06, payload, response_len=8)
            print(resp.hex())
    set_cfg.set_defaults(func=cfg_func)

    uuids = sub.add_parser("uuids", help="Read device and firmware UUIDs")
    uuids.set_defaults(func=handle_read_uuids)

    return parser


def main():
    parser = build_parser()
    args = parser.parse_args()
    try:
        args.func(args)
    except Exception as exc:  # pylint: disable=broad-except
        print(f"Error: {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
