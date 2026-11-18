"""Decode retained RE5 camera ABI1 observations without game-memory writes.

The overlay transports the complete fixed-size wire. Float values are paired
with their original IEEE754 bits; unavailable groups remain governed by the
record's available mask, not by whether their bytes happen to be zero.
"""
import argparse
import base64
import binascii
import json
import math
from pathlib import Path
import struct
import sys

WIRE_BYTES=5808
MARKER=b'RE5CTL_CAM1'+b'\0'*5
REASONS=(
    'none','no_actor','no_fresh_frame','first_person','legacy_free','not_mode','bad_distance','bad_owner','native_read','source_invalid',
    'anchor_busy','release_mismatch','heading_missing','actor_blocked','body_identity','epoch_mismatch','world_read','world_changed',
    'world_not_committed','head_read','anchor_height','math_rejected','same_pose','not_writable','applied','post_desired',
    'post_desired_read','camera_busy','reserved28','reserved29','reserved30','reserved31')
STAGES={1:'component_output_before_desired_collision',2:'post_desired_before_collision'}
FLAGS=('frame_valid','heading_valid','body_active','body_anchor','arm_valid','scope','modern','first_person','input_anchored')
AVAILABLE=('frame','native_eye_target','native_up','result_eye_target','result_up','body','expected_world','world','world_again','arm','anchor','fov')


def _unpack(raw,at,names,code):
    return dict(zip(names.split(),struct.unpack_from('<'+code*len(names.split()),raw,at)))


def _floats(raw,at,count):
    bits=struct.unpack_from('<'+'I'*count,raw,at)
    values=struct.unpack_from('<'+'f'*count,raw,at)
    return [v if math.isfinite(v) else None for v in values],[f'{b:08x}' for b in bits]


def _input(raw,at):
    out=_unpack(raw,at,'call tick actor command frame generation look_epoch look_release validation_tick pad_sample','Q')
    out.update(_unpack(raw,at+80,'request_sequence action flags route','I'))
    route=out['route'];out['route']=dict(raw=route,controller=route&65535,native_style=route>>16)
    out['raw']=list(struct.unpack_from('<4i',raw,at+96))
    out['float_bits']={}
    for name,offset,n in (('shaped',112,4),('filtered',128,4),('pitch',144,1),('native_heading',148,1),('look_yaw',152,1),('look_pitch',156,1)):
        values,bits=_floats(raw,at+offset,n)
        out[name]=values if n>1 else values[0];out['float_bits'][name]=bits if n>1 else bits[0]
    out['present']=out['call']!=0
    out['flag_names']=[name for i,name in enumerate(FLAGS) if out['flags']&(1<<i)]
    return out


def _decision(raw,at):
    out=_unpack(raw,at,'call tick actor command camera component input_frame input_tick generation look_epoch look_release body_frame body_epoch body_generation','Q')
    out.update(_unpack(raw,at+112,'request_sequence stage reason action flags available','I'))
    out['present']=out['call']!=0
    out['reason_name']=REASONS[out['reason']] if out['reason']<len(REASONS) else f"unknown_{out['reason']}"
    out['stage_name']=STAGES.get(out['stage'],f"unknown_{out['stage']}")
    out['flag_names']=[name for i,name in enumerate(FLAGS) if out['flags']&(1<<i)]
    out['available_names']=[name for i,name in enumerate(AVAILABLE) if out['available']&(1<<i)]
    out['float_bits']={}
    names='input_pitch body_pitch heading native_heading look_integral body_integral height distance forward_x forward_z'.split()
    for i,name in enumerate(names):
        values,bits=_floats(raw,at+136+4*i,1);out[name]=values[0];out['float_bits'][name]=bits[0]
    for name,offset,count in (('expected_world',176,16),('world',240,16),('world_again',304,16),('anchor',480,3),('fov',476,1)):
        values,bits=_floats(raw,at+offset,count);out[name]=values if count>1 else values[0];out['float_bits'][name]=bits if count>1 else bits[0]
    for name,offset in (('native',368),('result',404),('arm',440)):
        out[name]={};out['float_bits'][name]={}
        for i,vector in enumerate(('eye','up','target')):
            values,bits=_floats(raw,at+offset+12*i,3);out[name][vector]=values;out['float_bits'][name][vector]=bits
    out['reserved']=list(struct.unpack_from('<5I',raw,at+492))
    return out


def decode_camera_probe(raw: bytes) -> dict:
    """Validate and decode exactly one stable5808-byte ABI1 native wire."""
    if not isinstance(raw,(bytes,bytearray)) or len(raw)!=WIRE_BYTES:
        raise ValueError('camera wire must contain exactly5808 bytes')
    if raw[:16]!=MARKER:raise ValueError('camera marker mismatch')
    header=_unpack(raw,16,'abi size sequence request_sequence','I')
    header.update(_unpack(raw,32,'calls unrouted_calls','Q'))
    if header['abi']!=1 or header['size']!=WIRE_BYTES:raise ValueError('unsupported camera ABI/size')
    if header['sequence']&1:raise ValueError('camera publication is incomplete')
    players=[]
    for player in range(2):
        at=48+2624*player
        out={name:_input(raw,at+offset) for name,offset in (('last_input',0),('last_nonzero_ry',160))}
        out.update({name:_decision(raw,at+offset) for name,offset in (('last_component',320),('last_success',832),('last_failure',1344),('last_post_desired',1856))})
        out['reasons']=dict(zip(REASONS,struct.unpack_from('<32Q',raw,at+2368)))
        players.append(out)
    return dict(header=header,players=players,last_unrouted=_decision(raw,5296))


def decode_snapshot(snapshot: dict) -> dict:
    """Decode a camera_probe envelope; unavailable statuses stay explicit."""
    if not isinstance(snapshot,dict):raise ValueError('snapshot must be an object')
    envelope=snapshot.get('camera_probe')
    if envelope is None:
        status=snapshot.get('camera_probe_status','missing')
        if not isinstance(status,str):raise ValueError('camera status must be a string')
        return {'status':status}
    if not isinstance(envelope,dict):raise ValueError('camera envelope must be an object')
    status=envelope.get('status')
    if not isinstance(status,str):raise ValueError('camera status must be a string')
    if status!='ok':return {'status':status}
    if envelope.get('encoding')!='base64' or type(envelope.get('abi')) is not int or envelope['abi']!=1 or type(envelope.get('bytes')) is not int or envelope['bytes']!=WIRE_BYTES:
        raise ValueError('unsupported camera envelope')
    data=envelope.get('data')
    if not isinstance(data,str) or len(data)!=7744:raise ValueError('incorrect camera base64 length')
    try:raw=base64.b64decode(data,validate=True)
    except (ValueError,binascii.Error) as exc:raise ValueError('invalid camera base64') from exc
    return decode_camera_probe(raw)


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input',nargs='?',type=Path,help='JSON/JSONL snapshot file; defaults to stdin')
    args=parser.parse_args(argv)
    source=None
    try:
        source=args.input.open(encoding='utf-8') if args.input else sys.stdin
        pending=''
        while True:
            line=source.readline(65537)
            if not line:break
            pending+=line
            if len(pending)>65536:raise ValueError('snapshot exceeds64KiB')
            if not pending.strip():pending='';continue
            try:row=json.loads(pending)
            except json.JSONDecodeError:continue
            pending=''
            if not isinstance(row,dict):raise ValueError('expected snapshot object')
            decoded=decode_snapshot(row.get('data',row))
            out={key:row[key] for key in ('sample','host_time_utc') if key in row}
            out['camera_probe']=decoded
            print(json.dumps(out,ensure_ascii=False,allow_nan=False))
        if pending.strip():raise ValueError('incomplete or invalid snapshot JSON')
    except (ValueError,OSError) as exc:
        print(f'camera decode: {exc}',file=sys.stderr);return 1
    finally:
        if args.input and source is not None:source.close()
    return 0


if __name__=='__main__':raise SystemExit(main())
