"""Loopback SSH fixture with a restricted, disposable real Vim session."""
import atexit, hashlib, json, pathlib, runpy, subprocess, sys, threading, uuid

ROOT = pathlib.Path(__file__).resolve().parents[1]
VIM = pathlib.Path(sys.argv[1]).resolve()
if VIM.name.lower() != 'vim.exe' or not VIM.is_file():
    raise RuntimeError('Provide an existing Vim executable')
owned = set()
owned_lock = threading.Lock()

class VimSession:
    def __init__(self, channel, width, height, record):
        self.channel, self.record = channel, record
        self.file = ROOT / 'tests/ssh-fixture' / ('vim-fixture-' + uuid.uuid4().hex + '.txt')
        self.file.write_text('fullscreen-start-marker\n', encoding='utf-8')
        self.size = pathlib.Path(str(self.file) + '.size.json')
        self.resize(width, height)
        self.process = subprocess.Popen([str(ROOT / 'bin/tui-host.exe'), str(VIM), str(self.file), str(self.size)],
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        with owned_lock:
            owned.add(self)
        record('vim_start', programSha256=hashlib.sha256(VIM.read_bytes()).hexdigest())
        threading.Thread(target=self.output, daemon=True).start()

    def resize(self, width, height):
        pending = pathlib.Path(str(self.size) + '.tmp')
        pending.write_text(json.dumps({'columns': width, 'rows': height}), encoding='utf-8')
        pending.replace(self.size)

    def input(self, data):
        self.process.stdin.write(data)
        self.process.stdin.flush()

    def output(self):
        try:
            while True:
                data = self.process.stdout.read1(16384)
                if not data:
                    break
                self.channel.sendall(data)
            code = self.process.wait(timeout=5)
            saved = 'fullscreen-edit-marker' in self.file.read_text(encoding='utf-8')
            self.record('vim_exit', exitCode=code, saved=saved)
            self.channel.send_exit_status(code)
            self.channel.close()
        except Exception as error:
            self.record('vim_bridge_error', type=type(error).__name__)
        finally:
            self.close()

    def close(self):
        with owned_lock:
            if self not in owned:
                return
            owned.remove(self)
        if self.process.poll() is None:
            self.process.kill()  # Closing the helper's job also terminates its own Vim.
            self.process.wait(timeout=5)

def close_all():
    with owned_lock:
        sessions = list(owned)
    for session in sessions:
        session.close()

atexit.register(close_all)
try:
    runpy.run_path(str(ROOT / 'tests/ssh-fixture.py'), run_name='__main__',
                   init_globals={'VIM_FACTORY': VimSession, 'FIXTURE_MODE': 'vim'})
finally:
    close_all()
