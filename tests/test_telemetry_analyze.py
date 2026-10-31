import io
import math
import unittest
import os
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory
from tools.telemetry_analyze import analyze, read_journal

RE6 = '01002A000CD48000'
BID = bytes([0x06,0x1D,0x2A,0x06,0xCD,0x70,0x07,0x85,0xE9,0x87,0xC4,0x2C,0xE2,0xD7,0x22,0x31]+[0]*16).hex().upper()

def row(frame, yaw=0, x=0, rx=32767, valid=15):
    # Canonical column-vector world/view matrices; camera looking down -Z.
    c, s = math.cos(yaw), math.sin(yaw)
    return dict(session=4, sequence=frame, dropped_total=0, frame=frame,
                tick=frame*640000, input_tick=frame*640000-100,
                sample_id=frame, controller_id=0, controller_style=1,
                buttons=0, validity=valid, actor=4096, camera=8192,
                lx=0, ly=0, rx=rx, ry=0,
                **{f'player_m{i//4}{i%4}':v for i,v in enumerate([1,0,0,x,0,1,0,0,0,0,1,0])},
                **{f'view_m{i//4}{i%4}':v for i,v in enumerate([c,0,-s,0,0,1,0,0,s,0,c,0])},
                **{f'projection_m{i//4}{i%4}':1 if i in (0,5,10,15) else 0 for i in range(16)})

def stream(rows, title=RE6, bid=BID):
    import csv
    out=io.StringIO()
    out.write(f'#re6_journal,abi=1,session=4,title_id={title},tick_hz=19200000,build_id={bid}\n')
    writer=csv.DictWriter(out, fieldnames=list(rows[0]))
    writer.writeheader()
    out.write('#capture,start_after_sequence=0,capacity=256\n')
    writer.writerows(rows);out.seek(0)
    out.seek(0,2)
    out.write(f"#end,records={len(rows)},dropped={max(r['dropped_total'] for r in rows)},status=complete\n")
    out.seek(0)
    return out

class TelemetryAnalysisTests(unittest.TestCase):
    def test_actual_native_recorder_csv_is_accepted_without_format_translation(self):
        root=Path(__file__).resolve().parents[1]
        compiler=os.environ.get('CXX','C:/msys64/mingw64/bin/g++.exe')
        with TemporaryDirectory() as temp:
            image=Path(temp)/'journal.exe';csv_path=Path(temp)/'journal.csv'
            subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
                            '-static','-I'+str(root/'include'),str(root/'tests/native_telemetry_csv.cpp'),
                            '-o',str(image)],capture_output=True,check=True,timeout=60)
            subprocess.run([str(image),str(csv_path)],capture_output=True,check=True,timeout=20)
            with csv_path.open(encoding='utf-8',newline='') as f:report=analyze(read_journal(f))
            self.assertEqual(len(report['intervals']),1)
            self.assertAlmostEqual(report['intervals'][0]['yaw_deg_s'],60,places=4)
            self.assertAlmostEqual(report['intervals'][0]['player_speed_native_s'],60)

    def test_actual_ticks_and_wrap_determine_angular_speed(self):
        rows=[row(1, math.radians(179)),row(2,math.radians(-179),x=2)]
        report=analyze(read_journal(stream(rows)))
        sample=report['intervals'][0]
        self.assertAlmostEqual(sample['dt_s'],1/30)
        self.assertAlmostEqual(sample['yaw_deg_s'],60,places=5)
        self.assertAlmostEqual(sample['player_speed_native_s'],60)
        self.assertEqual(sample['rx'],1)
        self.assertEqual(report['rejected_intervals'],0)

    def test_missing_or_stale_input_and_lost_frames_never_become_measurements(self):
        rows=[row(i) for i in range(1,6)]
        rows[1]['input_tick']=1
        rows[1]['tick']=3000000  # stale and nonuniform native time
        rows[2]['tick']=3640000
        rows[2]['validity']=14
        rows[3]['dropped_total']=2
        rows[4]['dropped_total']=2
        rows[3]['sequence']=6
        rows[4]['sequence']=7
        report=analyze(read_journal(stream(rows)))
        self.assertEqual(len(report['intervals']),1)
        self.assertEqual(report['rejected_intervals'],3)
        self.assertEqual(report['dropped_total'],2)

    def test_identity_and_nonfinite_matrices_are_rejected(self):
        with self.assertRaises(ValueError):read_journal(stream([row(1)],title='0000000000000000'))
        with self.assertRaises(ValueError):read_journal(stream([row(1)],bid='0'*64))
        rows=[row(1),row(2)];rows[1]['view_m00']='nan'
        self.assertEqual(analyze(read_journal(stream(rows)))['rejected_intervals'],1)
        rows=[row(1),row(2)];rows[1]['session']=5
        with self.assertRaises(ValueError):read_journal(stream(rows))

    def test_fps_not_used_as_assumed_clock_and_controller_change_breaks_interval(self):
        rows=[row(1),row(2),row(3)]
        rows[1]['tick']=rows[0]['tick']+960000
        rows[2]['tick']=rows[1]['tick']+320000
        for r in rows:r['input_tick']=r['tick']-100
        rows[2]['controller_style']=4
        report=analyze(read_journal(stream(rows)))
        self.assertEqual(len(report['intervals']),1)
        self.assertAlmostEqual(report['intervals'][0]['dt_s'],.05)

    def test_unfinished_or_malformed_footer_is_not_a_completed_measurement(self):
        text=stream([row(1),row(2)]).getvalue()
        for bad in (text.rsplit('#end',1)[0],text.replace('records=2','records=3'),
                    text.replace('status=complete','status=incomplete'),text+'garbage\n',
                    text.replace('start_after_sequence=0','start_after_sequence=100')):
            with self.subTest(bad=bad[-70:]):
                with self.assertRaises(ValueError):read_journal(io.StringIO(bad))

if __name__=='__main__':unittest.main()
