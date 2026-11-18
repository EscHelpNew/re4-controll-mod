"""Independent wire offsets: do not generate fixtures using the decoder."""
import base64
import copy
import json
import struct
import subprocess
import sys
import unittest
from pathlib import Path

from tools.re5_camera_probe import decode_camera_probe, decode_snapshot

ROOT=Path(__file__).resolve().parents[1]

def fixture():
    raw=bytearray(5808)
    raw[:16]=b'RE5CTL_CAM1'+b'\0'*5
    struct.pack_into('<IIIIQQ',raw,16,1,5808,28,13,900,8)
    # Input0: retained last nonzero RY at player0 +160.
    at=48+160
    struct.pack_into('<10Q',raw,at,5,200,0xABC,0xDEF,31,2,7,8,199,44)
    struct.pack_into('<4I',raw,at+80,12,2,3,0x00040001)
    struct.pack_into('<4i',raw,at+96,100,-200,300,-32767)
    struct.pack_into('<12f',raw,at+112,*([.25,.5,-.75,-1]*2),.1,.2,.3,.4)
    # Player1 success: input320 + last_component512 + last_success.
    at=48+2624+320+512
    struct.pack_into('<14Q',raw,at,*range(100,114))
    struct.pack_into('<6I',raw,at+112,13,1,24,0x1F,511,4095)
    struct.pack_into('<10f',raw,at+136,*range(10))
    struct.pack_into('<16f',raw,at+176,*range(16))
    struct.pack_into('<16f',raw,at+240,*range(20,36))
    struct.pack_into('<16f',raw,at+304,*range(40,56))
    struct.pack_into('<9f',raw,at+368,*range(60,69))
    struct.pack_into('<9f',raw,at+404,*range(70,79))
    struct.pack_into('<9f',raw,at+440,*range(80,89))
    struct.pack_into('<4f',raw,at+476,90,91,92,93)
    struct.pack_into('<5I',raw,at+492,101,102,103,104,105)
    struct.pack_into('<Q',raw,48+2624+2368+24*8,1234)
    # No-route decision is never attributed to either player.
    struct.pack_into('<Q',raw,5296,88)
    struct.pack_into('<3I',raw,5296+112,13,1,2)
    return raw

def envelope(raw):
    return {'camera_probe':{'status':'ok','encoding':'base64','abi':1,'bytes':5808,'data':base64.b64encode(raw).decode('ascii')}}

class CameraProbeDecoderTests(unittest.TestCase):
    def test_independent_offsets_two_players_and_unrouted(self):
        out=decode_camera_probe(fixture())
        self.assertEqual(out['header'],dict(abi=1,size=5808,sequence=28,request_sequence=13,calls=900,unrouted_calls=8))
        record=out['players'][0]['last_nonzero_ry']
        self.assertEqual(record['raw'],[100,-200,300,-32767])
        self.assertEqual(record['route'],dict(raw=0x40001,controller=1,native_style=4))
        self.assertEqual(record['request_sequence'],12) # retained previous lease remains explicit
        self.assertAlmostEqual(record['pitch'],.1)
        self.assertEqual(record['shaped'],[.25,.5,-.75,-1])
        self.assertEqual(record['float_bits']['shaped'],['3e800000','3f000000','bf400000','bf800000'])
        r=out['players'][1]['last_success']
        self.assertEqual(r['body_generation'],113)
        self.assertEqual(r['reason'],24);self.assertEqual(r['reason_name'],'applied')
        self.assertEqual(r['stage_name'],'component_output_before_desired_collision')
        self.assertEqual(r['expected_world'],list(range(16)))
        self.assertEqual(r['world_again'],list(range(40,56)))
        self.assertEqual(r['native']['eye'],[60,61,62])
        self.assertEqual(r['result']['up'],[73,74,75])
        self.assertEqual(r['arm']['target'],[86,87,88])
        self.assertEqual(r['anchor'],[91,92,93]);self.assertEqual(r['reserved'],[101,102,103,104,105])
        self.assertEqual(out['players'][1]['reasons']['applied'],1234)
        self.assertEqual(out['last_unrouted']['reason_name'],'no_fresh_frame')
        self.assertFalse(out['players'][0]['last_input']['present'])

    def test_float_bits_survive_nonfinite_and_negative_zero(self):
        raw=fixture();struct.pack_into('<III',raw,48+160+112,0x7FC12345,0xFF800000,0x80000000)
        r=decode_camera_probe(raw)['players'][0]['last_nonzero_ry']
        self.assertEqual(r['shaped'][:2],[None,None])
        self.assertEqual(r['float_bits']['shaped'][:3],['7fc12345','ff800000','80000000'])
        json.dumps(r,allow_nan=False)

    def test_header_corruption_and_incomplete_sequence_rejected(self):
        original=fixture()
        for at,value in ((0,0),(16,2),(20,5800),(24,29)):
            raw=original.copy()
            if at==0:raw[0]=value
            else:struct.pack_into('<I',raw,at,value)
            with self.subTest(at=at),self.assertRaises(ValueError):decode_camera_probe(raw)
        for raw in (original[:-1],original+b'X',b'X'*100000):
            with self.assertRaises(ValueError):decode_camera_probe(raw)

    def test_envelope_is_bounded_and_strict(self):
        good=envelope(fixture());self.assertEqual(decode_snapshot(good)['header']['calls'],900)
        for field,value in (('abi',2),('abi',True),('bytes',5807),('encoding','hex'),('data','!'*7744),('data','A'*100000),('data',None)):
            bad=copy.deepcopy(good);bad['camera_probe'][field]=value
            with self.subTest(field=field,value=str(value)[:8]),self.assertRaises(ValueError):decode_snapshot(bad)
        self.assertEqual(decode_snapshot({}),{'status':'missing'})
        self.assertEqual(decode_snapshot({'camera_probe':None,'camera_probe_status':'identity_changed'}),{'status':'identity_changed'})
        self.assertEqual(decode_snapshot({'camera_probe':{'status':'changing'}}),{'status':'changing'})

    def test_unknown_reason_and_flags_preserved(self):
        raw=fixture();struct.pack_into('<I',raw,5296+120,333);struct.pack_into('<I',raw,5296+128,0x80000000)
        r=decode_camera_probe(raw)['last_unrouted']
        self.assertEqual(r['reason'],333);self.assertEqual(r['reason_name'],'unknown_333');self.assertEqual(r['flags'],0x80000000)

    def test_cli_watch_wrapper_and_plain_snapshot(self):
        row={'sample':2,'host_time_utc':'retained','data':envelope(fixture())}
        run=subprocess.run([sys.executable,'-B',str(ROOT/'tools/re5_camera_probe.py')],input=json.dumps(row)+'\n'+json.dumps(envelope(fixture()))+'\n',text=True,capture_output=True)
        self.assertEqual(run.returncode,0,run.stderr)
        rows=[json.loads(s) for s in run.stdout.splitlines()]
        self.assertEqual(len(rows),2);self.assertEqual(rows[0]['sample'],2)
        self.assertEqual(rows[0]['camera_probe']['header']['calls'],900)
        self.assertEqual(rows[1]['camera_probe']['header']['size'],5808)

    def test_cli_pretty_snapshot_and_missing_input_error(self):
        run=subprocess.run([sys.executable,'-B',str(ROOT/'tools/re5_camera_probe.py')],input=json.dumps(envelope(fixture()),indent=2),text=True,capture_output=True)
        self.assertEqual(run.returncode,0,run.stderr)
        self.assertEqual(json.loads(run.stdout)['camera_probe']['header']['calls'],900)
        run=subprocess.run([sys.executable,'-B',str(ROOT/'tools/re5_camera_probe.py'),str(ROOT/'build/no-camera-fixture26.json')],text=True,capture_output=True)
        self.assertEqual(run.returncode,1);self.assertNotIn('Traceback',run.stderr)
