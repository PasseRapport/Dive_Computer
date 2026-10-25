"""Dive Watch v1 reference protocol; no Bluetooth dependencies."""
from dataclasses import dataclass
import hashlib
import math
import struct
import uuid
import zlib

SERVICE = '6e400001-b5a3-f393-e0a9-e50e24dcca9e'
RX = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'
TX = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'
WATCH_ID = uuid.UUID('00000000-0000-4000-8000-000000000001')
MAX_FRAME = 4096

@dataclass(frozen=True)
class Frame:
    opcode: int
    request_id: int
    flags: int = 0
    payload: bytes = b''

    def encode(self):
        if not (1 <= self.request_id <= 65535 and 0 <= self.flags <= 3 and len(self.payload) <= MAX_FRAME-8):
            raise ValueError('invalid frame')
        return struct.pack('<HBBHH', 8+len(self.payload), 1, self.opcode, self.request_id, self.flags)+self.payload

class Decoder:
    def __init__(self):
        self.pending = bytearray()

    def feed(self, fragment):
        if len(fragment) > MAX_FRAME:
            raise ValueError('fragment too large')
        self.pending.extend(fragment)
        frames = []
        while len(self.pending) >= 2:
            n = struct.unpack_from('<H', self.pending)[0]
            if not 8 <= n <= MAX_FRAME:
                raise ValueError('invalid frame length')
            if len(self.pending) < n:
                break
            _, version, op, request, flags = struct.unpack_from('<HBBHH', self.pending)
            if version != 1 or request == 0 or flags & ~3:
                raise ValueError('unsupported frame')
            frames.append(Frame(op, request, flags, bytes(self.pending[8:n])))
            del self.pending[:n]
        return frames

@dataclass(frozen=True)
class DiveFile:
    id: uuid.UUID
    mode: int
    start_ms: int
    duration_ms: int
    blob: bytes

    @property
    def digest(self):
        return hashlib.sha256(self.blob).digest()

    def manifest(self):
        return self.id.bytes+struct.pack('<BqII', self.mode, self.start_ms, self.duration_ms, len(self.blob))+self.digest+struct.pack('<H',1024)

def generate_dives(scenario='normal'):
    result = []
    for i, seconds in enumerate((90, 120, 1800)):
        dive_id = uuid.UUID(int=(0x4000 << 64)+(0x8000000000000000+i+2))
        mode = 0 if i < 2 else 1
        start = -1 if scenario == 'unknown-time' else 1792836000000+i*600000
        max_depth = 1700 if i < 2 else 2400
        samples=[]
        for t in range(seconds+1):
            if scenario == 'gap' and 30 < t < 40:
                continue
            depth = max(0, int(math.sin(math.pi*t/seconds)*max_depth))
            samples.append(struct.pack('<IHh', t*1000, depth, -32768 if i==1 else 1820))
        event = struct.pack('<IIHBi',1,seconds*500,999 if scenario=='unknown-alert' else 1,1,max_depth)
        header = struct.pack('<4sBBH', b'DWLG',1,mode,1)+WATCH_ID.bytes+dive_id.bytes
        header += struct.pack('<qIIIH',start,seconds*1000,1000,len(samples),1)
        result.append(DiveFile(dive_id,mode,start,seconds*1000,header+b''.join(samples)+event))
    return result

class Server:
    def __init__(self, scenario='normal'):
        self.dives = generate_dives(scenario)
        self.scenario = scenario
        self.corrupted = False
        self.blocks_sent = 0

    def reply(self, frame):
        try:
            if frame.flags:
                raise ValueError('client request expected')
            if frame.opcode == 1:
                if frame.payload: raise ValueError('unexpected payload')
                def text(s):
                    b=s.encode('utf-8');return bytes([len(b)])+b
                body=WATCH_ID.bytes+b'\x01'+text('DiveWatch-SIM')+text('sim-0.1')
            elif frame.opcode == 2:
                if frame.payload: raise ValueError('unexpected payload')
                body=struct.pack('<BBH',1 if self.scenario=='active-dive' else 0,87,len(self.dives))
            elif frame.opcode == 3:
                if len(frame.payload)!=4: raise ValueError('cursor length')
                cursor=struct.unpack('<I',frame.payload)[0]
                if cursor!=0: raise ValueError('cursor')
                body=struct.pack('<IIH',1,0xffffffff,len(self.dives))+b''.join(d.manifest() for d in self.dives)
            elif frame.opcode == 4:
                if len(frame.payload)!=16: raise ValueError('id length')
                body=next(d.manifest() for d in self.dives if d.id.bytes==frame.payload)
            elif frame.opcode == 5:
                if len(frame.payload)!=36: raise ValueError('block request length')
                digest=frame.payload[:32];offset=struct.unpack_from('<I',frame.payload,32)[0]
                dive=next(d for d in self.dives if d.digest==digest)
                if offset%1024 or offset>=len(dive.blob):raise ValueError('offset')
                chunk=dive.blob[offset:offset+1024]
                crc=zlib.crc32(chunk)
                if self.scenario=='corrupt-block' and not self.corrupted:
                    chunk=bytes([chunk[0]^1])+chunk[1:];self.corrupted=True
                body=struct.pack('<IHI',offset,len(chunk),crc)+chunk
                self.blocks_sent+=1
            else:
                return Frame(frame.opcode,frame.request_id,3,struct.pack('<H',1))
            return Frame(frame.opcode,frame.request_id,1,body)
        except (ValueError, StopIteration, struct.error):
            return Frame(frame.opcode,frame.request_id,3,struct.pack('<H',2))
