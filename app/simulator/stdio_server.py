#!/usr/bin/env python3
"""Host-only integration harness. Hex frames on stdin/stdout; no radio validation."""
import argparse
import sys
from protocol import Decoder, Server

parser=argparse.ArgumentParser()
parser.add_argument('--scenario',default='normal')
args=parser.parse_args()
server=Server(args.scenario)
for line in sys.stdin:
    if len(line)>8193:
        raise ValueError('frame too long')
    frame=Decoder().feed(bytes.fromhex(line.strip()))
    if len(frame)!=1:raise ValueError('exactly one frame required')
    print(server.reply(frame[0]).encode().hex(),flush=True)
