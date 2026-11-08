"""Bounded subprocess trees with direct, durable host-test logs."""
import json
import os
from pathlib import Path
import signal
import shutil
import subprocess
import threading
import time

_ACTIVE = set()
_LOCK = threading.Lock()
_OWNERS = {}
_LOCAL = threading.local()


class ProcessScope:
    """One pool's cancellation latch; worker calls adopt it explicitly."""
    def __init__(self):
        self._cancelled = False

    def run(self, call, *args, **kwargs):
        previous = getattr(_LOCAL, 'scope', None)
        _LOCAL.scope = self
        try:
            return call(*args, **kwargs)
        finally:
            _LOCAL.scope = previous

    def cancel(self, timeout=5):
        with _LOCK:
            self._cancelled = True
            active = [child for child in _ACTIVE if _OWNERS.get(child) is self]
        for child in active:
            _terminate(child, timeout)


def tool_environment(environment, tool):
    """Keep nested tool executables on their own DLL/toolchain search path."""
    env = environment.copy()
    resolved = Path(tool) if Path(tool).is_file() else Path(shutil.which(str(tool), path=env.get('PATH')) or '')
    if resolved.is_file():
        env['PATH'] = str(resolved.resolve().parent) + os.pathsep + env.get('PATH', '')
    return env


if os.name == 'nt':
    import ctypes
    from ctypes import wintypes
    _WIN = ctypes.WinDLL('kernel32', use_last_error=True)
    _WIN.SetErrorMode(0x8003)  # Critical/file errors and WER dialogs cannot block unattended children.

    class _ProcessEntry(ctypes.Structure):
        _fields_ = [('size', wintypes.DWORD), ('usage', wintypes.DWORD), ('pid', wintypes.DWORD),
                    ('heap', ctypes.c_size_t), ('module', wintypes.DWORD), ('threads', wintypes.DWORD),
                    ('parent', wintypes.DWORD), ('priority', wintypes.LONG), ('flags', wintypes.DWORD),
                    ('executable', wintypes.WCHAR * 260)]

    _WIN.CreateToolhelp32Snapshot.argtypes = (wintypes.DWORD, wintypes.DWORD)
    _WIN.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    _WIN.Process32FirstW.argtypes = (wintypes.HANDLE, ctypes.POINTER(_ProcessEntry))
    _WIN.Process32NextW.argtypes = (wintypes.HANDLE, ctypes.POINTER(_ProcessEntry))
    _WIN.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    _WIN.OpenProcess.restype = wintypes.HANDLE
    _WIN.WaitForSingleObject.argtypes = (wintypes.HANDLE, wintypes.DWORD)
    _WIN.TerminateProcess.argtypes = (wintypes.HANDLE, wintypes.UINT)
    _WIN.CloseHandle.argtypes = (wintypes.HANDLE,)


def _windows_children(pid):
    """Hold handles only for descendants of our still-owned Popen process."""
    snapshot = _WIN.CreateToolhelp32Snapshot(2, 0)
    if snapshot == ctypes.c_void_p(-1).value:
        raise OSError(ctypes.get_last_error(), 'Cannot enumerate owned process descendants')
    try:
        entry = _ProcessEntry()
        entry.size = ctypes.sizeof(entry)
        parents = {}
        more = _WIN.Process32FirstW(snapshot, ctypes.byref(entry))
        while more:
            parents[entry.pid] = entry.parent
            more = _WIN.Process32NextW(snapshot, ctypes.byref(entry))
    finally:
        _WIN.CloseHandle(snapshot)
    owned = {pid}
    while True:
        found = {child for child, parent in parents.items() if parent in owned}
        if found <= owned:
            break
        owned.update(found)
    handles = []
    for child in owned - {pid}:
        handle = _WIN.OpenProcess(0x100001, False, child)  # SYNCHRONIZE | PROCESS_TERMINATE
        if handle:
            handles.append((child, handle))
    return handles


def _terminate(process, timeout):
    if process.poll() is not None:
        return dict(tree_terminated=True, tree_pids=[])
    deadline = time.monotonic() + timeout
    enumeration_error = None
    try:
        children = _windows_children(process.pid) if os.name == 'nt' else []
    except OSError as error:
        children = []
        enumeration_error = str(error)
    if os.name == 'nt':
        # Kill descendants while the parent still exists. Killing only g++
        # first can orphan collect2/ld and lose the process-tree relationship.
        try:
            result = subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'],
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                    timeout=timeout, creationflags=subprocess.CREATE_NO_WINDOW)
            tree_terminated = result.returncode == 0
        except (OSError, subprocess.TimeoutExpired):
            tree_terminated = False
        # taskkill is asynchronous with respect to final process teardown.
        # Keep owned handles alive until exit, including inherited log handles.
        try:
            for _, handle in children:
                remaining = max(0, int((deadline - time.monotonic()) * 1000))
                if _WIN.WaitForSingleObject(handle, remaining) == 258:
                    _WIN.TerminateProcess(handle, 1)
                    remaining = max(0, int((deadline - time.monotonic()) * 1000))
                    tree_terminated &= _WIN.WaitForSingleObject(handle, remaining) == 0
        finally:
            for _, handle in children:
                _WIN.CloseHandle(handle)
    else:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        tree_terminated = True
    try:
        process.wait(timeout=max(.01, deadline - time.monotonic()))
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=timeout)
        tree_terminated = False
    result = dict(tree_terminated=tree_terminated and enumeration_error is None,
                  tree_pids=[pid for pid, _ in children])
    if enumeration_error:
        result['termination_error'] = enumeration_error
    return result


def cancel_owned(timeout=5):
    """Cancel only children registered by this helper, never by process name."""
    scope = getattr(_LOCAL, 'scope', None)
    if scope is not None:
        scope.cancel(timeout)
        return
    with _LOCK:
        active = list(_ACTIVE)
    for process in active:
        _terminate(process, timeout)


def _write(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')
    temporary.replace(path)


def run_logged(argv, *, cwd, env, stdout_path, stderr_path, timeout, kill_timeout=5):
    """Run one owned tree; command metadata exists before any child starts.

    Files, rather than PIPE handles, prevent descendants from keeping a
    timed-out parent's communicate() blocked indefinitely.
    """
    stdout_path, stderr_path = Path(stdout_path), Path(stderr_path)
    stdout_path.parent.mkdir(parents=True, exist_ok=True)
    stderr_path.parent.mkdir(parents=True, exist_ok=True)
    command_file = stdout_path.with_suffix('.command.json')
    result_file = stdout_path.with_suffix('.result.json')
    step = dict(command=[str(a) for a in argv], cwd=str(cwd), timeout=timeout,
                stdout=str(stdout_path), stderr=str(stderr_path), command_file=str(command_file),
                result_file=str(result_file), returncode=None, timed_out=False, cancelled=False)
    _write(command_file, step)
    start = time.monotonic()
    process = None
    scope = getattr(_LOCAL, 'scope', None)
    try:
        with stdout_path.open('wb') as stdout, stderr_path.open('wb') as stderr:
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {'start_new_session': True}
            try:
                # Cancellation and launch+registration are one transaction.
                # A cancelling pool must see every child already launched;
                # later stages must see the latched cancellation before Popen.
                with _LOCK:
                    if scope is not None and scope._cancelled:
                        step.update(cancelled=True, returncode=-1, tree_terminated=True, tree_pids=[])
                    else:
                        process = subprocess.Popen(step['command'], cwd=cwd, env=env,
                                                   stdout=stdout, stderr=stderr, **options)
                        step['pid'] = process.pid
                        _ACTIVE.add(process)
                        _OWNERS[process] = scope
                if process is not None:
                    step['returncode'] = process.wait(timeout=timeout)
                    with _LOCK:
                        if scope is not None and scope._cancelled:
                            step['cancelled'] = True
            except subprocess.TimeoutExpired:
                step['timed_out'] = True
                step.update(_terminate(process, kill_timeout))
                step['returncode'] = process.returncode
                state = 'confirmed' if step['tree_terminated'] else 'unconfirmed'
                stderr.write(f'\nProcess exceeded timeout; owned-tree termination {state}\n'.encode())
            except OSError as error:
                if process is not None:
                    step.update(_terminate(process, kill_timeout))
                    step['returncode'] = process.returncode
                step['error'] = str(error)
                stderr.write((str(error) + '\n').encode('utf-8', errors='replace'))
            except BaseException:
                step['cancelled'] = True
                if process is not None:
                    step.update(_terminate(process, kill_timeout))
                    step['returncode'] = process.returncode
                raise
            finally:
                if process is not None:
                    with _LOCK:
                        _ACTIVE.discard(process)
                        _OWNERS.pop(process, None)
    finally:
        step['seconds'] = time.monotonic() - start
        _write(result_file, step)
    return step
