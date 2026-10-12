# -*- coding: utf-8 -*-

import base64
import os
import socket
import ssl
import struct
import sys
import time

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

PROTO_INTERMEDIATE = 0xEEEEEEEE
REQ_PQ_MULTI = 0xBE7E8EF1
RES_PQ = 0x05162463

RESERVED_STARTS = {b'HEAD', b'POST', b'GET ', b'OPTI', b'\xdd\xdd\xdd\xdd',
                   b'\xee\xee\xee\xee', b'\xef\xef\xef\xef', b'\x16\x03\x01\x02'}

def ctr(key, iv):
    return Cipher(algorithms.AES(key), modes.CTR(iv)).encryptor()

def make_init(dc):

    while True:
        buf = bytearray(os.urandom(64))
        if buf[0] == 0xEF:
            continue
        if bytes(buf[:4]) in RESERVED_STARTS:
            continue
        if buf[4:8] == b'\x00\x00\x00\x00':
            continue
        break

    enc = ctr(bytes(buf[8:40]), bytes(buf[40:56]))
    rev = bytes(buf[8:56])[::-1]
    dec = ctr(rev[:32], rev[32:48])

    tail = struct.pack('<IHH', PROTO_INTERMEDIATE, dc & 0xFFFF,
                       struct.unpack('<H', os.urandom(2))[0])
    encrypted = enc.update(bytes(buf))
    for i in range(8):
        buf[56 + i] = tail[i] ^ (encrypted[56 + i] ^ buf[56 + i])
    return bytes(buf), enc, dec

def ws_connect(host, path='/apiws', timeout=10):
    ctx = ssl.create_default_context()
    raw = socket.create_connection((host, 443), timeout=timeout)
    sock = ctx.wrap_socket(raw, server_hostname=host)
    key = base64.b64encode(os.urandom(16)).decode()
    req = (
        'GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
        'Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n'
        'Sec-WebSocket-Protocol: binary\r\n\r\n' % (path, host, key)
    )
    sock.sendall(req.encode())
    head = b''
    while b'\r\n\r\n' not in head:
        chunk = sock.recv(1)
        if not chunk:
            raise IOError('соединение закрыто на рукопожатии')
        head += chunk
    status = head.split(b'\r\n', 1)[0].decode(errors='replace')
    if b' 101 ' not in head.split(b'\r\n', 1)[0]:
        raise IOError('не 101: ' + status)
    return sock

def ws_send(sock, payload):

    n = len(payload)
    frame = bytearray([0x82])
    if n < 126:
        frame.append(0x80 | n)
    elif n < 65536:
        frame.append(0x80 | 126)
        frame += struct.pack('>H', n)
    else:
        frame.append(0x80 | 127)
        frame += struct.pack('>Q', n)
    mask = os.urandom(4)
    frame += mask
    frame += bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
    sock.sendall(bytes(frame))

def ws_recv(sock, timeout=10):
    sock.settimeout(timeout)

    def need(n):
        out = b''
        while len(out) < n:
            chunk = sock.recv(n - len(out))
            if not chunk:
                raise IOError('соединение закрыто')
            out += chunk
        return out

    while True:
        head = need(2)
        opcode = head[0] & 0x0F
        length = head[1] & 0x7F
        if length == 126:
            length = struct.unpack('>H', need(2))[0]
        elif length == 127:
            length = struct.unpack('>Q', need(8))[0]
        body = need(length) if length else b''
        if opcode == 0x8:
            raise IOError('датацентр закрыл соединение')
        if opcode in (0x1, 0x2, 0x0):
            return body

def probe(host, dc, path='/apiws'):
    started = time.time()
    sock = ws_connect(host, path)
    init, enc, dec = make_init(dc)
    ws_send(sock, init)

    nonce = os.urandom(16)
    body = struct.pack('<I', REQ_PQ_MULTI) + nonce
    msg_id = (int(time.time()) << 32) | (os.urandom(2)[0] << 2)
    plain = struct.pack('<qqi', 0, msg_id, len(body)) + body
    frame = struct.pack('<i', len(plain)) + plain
    ws_send(sock, enc.update(frame))

    answer = dec.update(ws_recv(sock))
    sock.close()
    took = int((time.time() - started) * 1000)

    if len(answer) < 24:
        return False, 'ответ короче заголовка (%d Б)' % len(answer), took
    length = struct.unpack('<i', answer[:4])[0]
    auth_key_id = struct.unpack('<q', answer[4:12])[0]
    constructor = struct.unpack('<I', answer[24:28])[0] if len(answer) >= 28 else 0
    if auth_key_id != 0:
        return False, 'auth_key_id не ноль', took
    if constructor != RES_PQ:
        return False, 'не resPQ: %08x (длина %d)' % (constructor, length), took
    if answer[28:44] != nonce:
        return False, 'resPQ с чужим nonce', took
    return True, 'resPQ, nonce совпал', took

if __name__ == '__main__':
    dc = int(sys.argv[1]) if len(sys.argv) > 1 else 2
    hosts = sys.argv[2:] or ['kws%d.pclead.co.uk' % dc, 'kws%d.offshor.co.uk' % dc]
    for host in hosts:

        host, _, path = host.partition('/')
        path = '/' + path if path else '/apiws'
        try:
            ok, note, took = probe(host, dc, path)
            print('%-30s %s  %s  %d мс' % (host, 'OK  ' if ok else 'НЕТ ', note, took))
        except Exception as exc:
            print('%-30s НЕТ  %s: %s' % (host, type(exc).__name__, exc))
