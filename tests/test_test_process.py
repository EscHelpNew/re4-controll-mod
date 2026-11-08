"""Actual child lifecycles at cancellation and registration boundaries; no compiler."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

from tools import test_process as process


class ProcessLifecycleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.children = []
        self.workers = []
        self.addCleanup(self.join_workers)
        self.addCleanup(self.stop_children)
        self.addCleanup(process.cancel_owned, 3)

    def join_workers(self):
        for worker in self.workers:
            worker.join(5)
            self.assertFalse(worker.is_alive(), 'Cancelled worker remained blocked')

    def stop_children(self):
        for child in self.children:
            if child.poll() is None: process._terminate(child, 3)

    def run_child(self, name='child', code='import time;time.sleep(60)', timeout=10):
        return process.run_logged([sys.executable, '-c', code], cwd=self.root, env=os.environ.copy(),
            stdout_path=self.root / (name + '.stdout'), stderr_path=self.root / (name + '.stderr'),
            timeout=timeout, kill_timeout=3)

    def wait_file(self, path):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if path.exists(): return
            time.sleep(.005)
        self.fail('Child did not reach its synchronization point: ' + str(path))

    def test_interrupt_after_real_launch_terminates_child_and_records_cancellation(self):
        children = self.children
        class InterruptRegistration(set):
            def add(self, child):
                children.append(child)
                raise KeyboardInterrupt()
        with patch.object(process, '_ACTIVE', InterruptRegistration()):
            with self.assertRaises(KeyboardInterrupt): self.run_child()
        self.assertEqual(len(self.children), 1)
        self.assertIsNotNone(self.children[0].poll(), 'Registration interruption leaked the real child')
        result = json.loads((self.root / 'child.result.json').read_text())
        self.assertTrue(result['cancelled'])
        self.assertIsNotNone(result['returncode'])

    def test_cancelled_scope_blocks_later_commands_but_new_scope_can_run(self):
        self.assertTrue(hasattr(process, 'ProcessScope'), 'A pool needs its own cancellation latch')
        scope = process.ProcessScope(); scope.cancel(timeout=3)
        forbidden = self.root / 'forbidden.txt'
        code = 'from pathlib import Path;Path(' + repr(str(forbidden)) + ').write_text("ran")'
        result = scope.run(lambda: self.run_child('blocked', code))
        self.assertTrue(result['cancelled']); self.assertNotEqual(result['returncode'], 0)
        self.assertFalse(forbidden.exists()); self.assertNotIn('pid', result)
        result = process.ProcessScope().run(lambda: self.run_child('fresh', code))
        self.assertEqual(result['returncode'], 0); self.assertFalse(result['cancelled'])
        self.assertEqual(forbidden.read_text(), 'ran')

    def test_cancel_targets_only_its_registered_child_and_rejects_next_stage(self):
        self.assertTrue(hasattr(process, 'ProcessScope'), 'A pool needs its own cancellation latch')
        owned, unrelated = process.ProcessScope(), process.ProcessScope()
        results = {}; errors = []
        def work(scope, name):
            try:
                ready = self.root / (name + '.ready')
                code = 'from pathlib import Path;import time;Path(' + repr(str(ready)) + ').write_text("ready");time.sleep(60)'
                results[name] = scope.run(lambda: self.run_child(name, code))
            except BaseException as error: errors.append(error)
        for scope, name in ((owned, 'owned'), (unrelated, 'unrelated')):
            worker = threading.Thread(target=work, args=(scope, name), daemon=True)
            self.workers.append(worker); worker.start(); self.wait_file(self.root / (name + '.ready'))
        owned.cancel(timeout=3); self.workers[0].join(5)
        self.assertFalse(self.workers[0].is_alive()); self.assertTrue(self.workers[1].is_alive())
        self.assertEqual(errors, []); self.assertTrue(results['owned']['cancelled'])
        self.assertNotEqual(results['owned']['returncode'], 0)
        late = self.root / 'late-stage'
        result = owned.run(lambda: self.run_child('late', 'from pathlib import Path;Path(' + repr(str(late)) + ').touch()'))
        self.assertTrue(result['cancelled']); self.assertFalse(late.exists())
        unrelated.cancel(timeout=3); self.workers[1].join(5)
        self.assertEqual(errors, []); self.assertTrue(results['unrelated']['cancelled'])

    def test_cancellation_waits_for_inflight_launch_registration_then_reaps_child(self):
        scope = process.ProcessScope(); launched = threading.Event(); release = threading.Event()
        cancel_attempted = threading.Event(); cancel_done = threading.Event(); result = {}; errors = []
        original_popen, original_lock = subprocess.Popen, process._LOCK
        children = self.children
        class ObservedLock:
            def __enter__(self):
                if threading.current_thread().name == 'scope-canceller': cancel_attempted.set()
                original_lock.acquire(); return self
            def __exit__(self, *args): original_lock.release()
        def launch(*args, **kwargs):
            child = original_popen(*args, **kwargs)
            if args[0][0] == sys.executable:
                children.append(child); launched.set()
                if not release.wait(5): raise RuntimeError('Fixture did not release launch')
            return child
        def work():
            try: result.update(scope.run(lambda: self.run_child('inflight')))
            except BaseException as error: errors.append(error)
        def cancel():
            try: scope.cancel(timeout=3)
            except BaseException as error: errors.append(error)
            finally: cancel_done.set()
        with patch.object(process.subprocess, 'Popen', side_effect=launch), patch.object(process, '_LOCK', ObservedLock()):
            worker = threading.Thread(target=work, daemon=True)
            canceller = threading.Thread(target=cancel, name='scope-canceller', daemon=True)
            self.workers.extend((worker, canceller)); worker.start()
            try:
                self.assertTrue(launched.wait(5)); canceller.start()
                self.assertTrue(cancel_attempted.wait(5))
                self.assertFalse(cancel_done.wait(.1), 'Cancellation missed a child between launch and registration')
            finally:
                release.set(); worker.join(5)
                if canceller.ident is not None: canceller.join(5)
        self.assertEqual(errors, []); self.assertFalse(worker.is_alive()); self.assertTrue(cancel_done.is_set())
        self.assertTrue(result['cancelled']); self.assertNotEqual(result['returncode'], 0)
        self.assertIsNotNone(self.children[0].poll())
