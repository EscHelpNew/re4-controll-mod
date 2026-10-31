"""Derive observed motion from synchronized RE4/RE6 journals, never assumed FPS."""
import argparse
import csv
import json
import math
from pathlib import Path

BUILDS = {
    '01002A000CD48000': '061D2A06CD700785E987C42CE2D72231' + '0'*32,
    '010099A00BC1E000': '82C2A04A21D3E0B850F273D4149C9003' + '0'*32,
}
INTS = ('session','sequence','dropped_total','frame','tick','sample_id',
        'controller_id','controller_style','buttons','validity','actor','camera',
        'lx','ly','rx','ry','input_tick')
MATRICES = tuple(f'{p}_m{i//4}{i%4}' for p,n in
                 (('player',12),('view',12),('projection',16)) for i in range(n))

def read_journal(stream):
    first=stream.readline().strip().split(',')
    if first[0]!='#re6_journal':raise ValueError('Not a supported controls journal')
    try:meta=dict(item.split('=',1) for item in first[1:])
    except ValueError as e:raise ValueError('Malformed journal identity') from e
    if (meta.get('abi')!='1' or meta.get('tick_hz')!='19200000'
            or BUILDS.get(meta.get('title_id'))!=meta.get('build_id')):
        raise ValueError('Unsupported journal build or timing ABI')
    try:session=int(meta['session'])
    except (ValueError,KeyError) as e:raise ValueError('Missing session identity') from e
    if session<=0:raise ValueError('Invalid session identity')
    reader=csv.reader(stream)
    fields=next(reader,[])
    if len(set(fields))!=len(fields):
        raise ValueError('Duplicate CSV columns')
    if not set(INTS+MATRICES).issubset(fields):
        raise ValueError('Incomplete journal columns')
    rows=[];footer=None;capture=None
    for values in reader:
        if footer is not None:raise ValueError('Data after capture footer')
        if values and values[0]=='#capture':
            if capture is not None or rows:raise ValueError('Unexpected capture boundary')
            try:
                capture=dict(item.split('=',1) for item in values[1:])
                capture={k:int(capture[k]) for k in ('start_after_sequence','capacity')}
            except (ValueError,KeyError) as e:raise ValueError('Invalid capture boundary') from e
            if capture['start_after_sequence']<0 or capture['capacity'] not in (256,1024):
                raise ValueError('Invalid capture boundary')
            continue
        if values and values[0]=='#end':
            try:
                footer=dict(item.split('=',1) for item in values[1:])
                if (footer.get('status')!='complete' or int(footer['records'])!=len(rows)
                        or int(footer['dropped'])<max((r['dropped_total'] for r in rows),default=0)):
                    raise ValueError('Invalid capture footer')
            except (ValueError,KeyError) as e:raise ValueError('Incomplete capture') from e
            continue
        if capture is None or len(values)!=len(fields):raise ValueError('Missing capture boundary or truncated CSV row')
        source=dict(zip(fields,values))
        try:
            row={k:int(source[k]) if source[k] else None for k in INTS}
            row.update({k:float(source[k]) if source[k] else None for k in MATRICES})
        except ValueError as e:raise ValueError('Malformed journal value') from e
        if row['session']!=session:raise ValueError('Mixed journal sessions')
        if any(row[k] is None or row[k]<0 for k in
               ('sequence','dropped_total','frame','tick','validity','actor','camera')):
            raise ValueError('Missing or negative frame identity')
        if (row['sequence']-capture['start_after_sequence']-len(rows)-1!=row['dropped_total']
                or (rows and (row['sequence']<=rows[-1]['sequence'] or row['dropped_total']<rows[-1]['dropped_total']))):
            raise ValueError('Inconsistent sequence/loss accounting')
        rows.append(row)
    if footer is None or capture is None:raise ValueError('Missing completed capture footer or boundary')
    return {'metadata':meta,'capture':capture,'rows':rows,'dropped_total':int(footer['dropped'])}

def _pose(row):
    if row['validity']&7!=7 or not row['actor'] or not row['camera']:return None
    required=INTS[5:]+MATRICES[:24]
    if any(row[k] is None or not math.isfinite(row[k]) for k in required):return None
    if any(not -32768<=row[k]<=32767 for k in ('lx','ly','rx','ry')):return None
    if not 0<=row['tick']-row['input_tick']<=1920000:return None
    # Third row of the rigid view matrix is camera-back. Preserve native handedness.
    back=[row[f'view_m2{i}'] for i in range(3)]
    length=math.sqrt(sum(v*v for v in back))
    if not .9<=length<=1.1:return None
    yaw=math.atan2(back[0],back[2])
    pitch=math.asin(max(-1,min(1,-back[1]/length)))
    position=[row[f'player_m{i}3'] for i in range(3)]
    return yaw,pitch,position

def analyze(journal):
    rows=journal['rows'];out=[];rejected=0
    for a,b in zip(rows,rows[1:]):
        p,q=_pose(a),_pose(b)
        dt=(b['tick']-a['tick'])/19200000
        identity=('controller_id','controller_style','actor','camera')
        if (p is None or q is None or not 0<dt<=.1
                or b['sequence']!=a['sequence']+1 or b['frame']!=a['frame']+1
                or b['dropped_total']!=a['dropped_total']
                or any(a[k]!=b[k] for k in identity)):
            rejected+=1;continue
        dyaw=math.atan2(math.sin(q[0]-p[0]),math.cos(q[0]-p[0]))
        sample={'frame':b['frame'],'tick':b['tick'],'dt_s':dt,
                'yaw_deg_s':math.degrees(dyaw)/dt,
                'pitch_deg_s':math.degrees(q[1]-p[1])/dt,
                'player_speed_native_s':math.dist(p[2],q[2])/dt}
        for key in ('lx','ly','rx','ry'):
            sample[key]=(a[key]+b[key])/(2*32767)
            sample[key]=max(-1,min(1,sample[key]))
        out.append(sample)
    return {'metadata':journal['metadata'],'frames':len(rows),
            'dropped_total':journal['dropped_total'],
            'rejected_intervals':rejected,'intervals':out,
            'units':'camera degrees/s; player native units/s; sticks normalized raw',
            'notice':'Observed intervals only. Compare repeated trials; not a fitted control profile.'}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('journal',type=Path);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    with args.journal.open(encoding='utf-8',newline='') as f:report=analyze(read_journal(f))
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    print(f"Frames={report['frames']} valid_intervals={len(report['intervals'])} "
          f"rejected={report['rejected_intervals']} dropped={report['dropped_total']}")
if __name__=='__main__':main()
