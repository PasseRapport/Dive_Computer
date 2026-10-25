import hashlib
import struct
import unittest
import zlib
from protocol import Decoder, Frame, Server, generate_dives

class ProtocolTests(unittest.TestCase):
    def test_hello_golden(self):
        self.assertEqual(Frame(1,1).encode().hex(),'0800010101000000')
    def test_every_fragment_size(self):
        wire=Frame(5,44,1,bytes(range(256))*4).encode()
        for size in (1,2,7,20,244,4096):
            decoder=Decoder();frames=[]
            for offset in range(0,len(wire),size):frames.extend(decoder.feed(wire[offset:offset+size]))
            self.assertEqual(frames,[Frame(5,44,1,bytes(range(256))*4)])
    def test_malformed_frames(self):
        for wire in (b'\x07\x00',b'\x01\x10',bytes.fromhex('0800020101000000'),bytes.fromhex('0800010100000000')):
            with self.assertRaises(ValueError):Decoder().feed(wire)
    def test_catalog_and_full_import(self):
        server=Server()
        catalog=server.reply(Frame(3,1,payload=struct.pack('<I',0)))
        self.assertEqual(struct.unpack_from('<IIH',catalog.payload),(1,0xffffffff,3))
        for dive in server.dives:
            received=bytearray()
            for offset in range(0,len(dive.blob),1024):
                response=server.reply(Frame(5,2,payload=dive.digest+struct.pack('<I',offset)))
                position,n,crc=struct.unpack_from('<IHI',response.payload)
                chunk=response.payload[10:]
                self.assertEqual((position,n,crc),(offset,len(chunk),zlib.crc32(chunk)))
                received.extend(chunk)
            self.assertEqual(hashlib.sha256(received).digest(),dive.digest)
            self.assertEqual(received,dive.blob)
    def test_crc_fault_is_detectable_and_retry_recovers(self):
        server=Server('corrupt-block');dive=server.dives[0];request=Frame(5,1,payload=dive.digest+struct.pack('<I',0))
        first=server.reply(request).payload
        self.assertNotEqual(struct.unpack_from('<I',first,6)[0],zlib.crc32(first[10:]))
        second=server.reply(request).payload
        self.assertEqual(struct.unpack_from('<I',second,6)[0],zlib.crc32(second[10:]))
    def test_invalid_offsets_and_unknown_commands(self):
        server=Server();d=server.dives[0]
        for offset in (1,0xffffffff):self.assertEqual(server.reply(Frame(5,1,payload=d.digest+struct.pack('<I',offset))).flags,3)
        self.assertEqual(server.reply(Frame(200,2)).flags,3)
    def test_scenarios_are_repeatable(self):
        for scenario in ('normal','gap','unknown-time','unknown-alert','active-dive'):
            self.assertEqual([d.blob for d in generate_dives(scenario)],[d.blob for d in generate_dives(scenario)])

if __name__=='__main__':unittest.main()
