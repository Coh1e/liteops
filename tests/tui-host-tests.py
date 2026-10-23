"""Synthetic pipe/ConPTY fixture, never desktop input or a user file."""
import argparse, hashlib, json, pathlib, subprocess, threading, time, uuid

parser = argparse.ArgumentParser()
parser.add_argument('--vim', required=True)
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
file = root / 'tests/ssh-fixture' / ('vim-fixture-' + uuid.uuid4().hex + '.txt')
file.parent.mkdir(parents=True, exist_ok=True)
file.write_text('fullscreen-start-marker\n', encoding='utf-8')
size = pathlib.Path(str(file) + '.size.json')
size.write_text('{"columns":80,"rows":24}', encoding='utf-8')
process = subprocess.Popen([str(root / 'bin/tui-host.exe'), args.vim, str(file), str(size)],
                           stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
output, errors = [], []

def collect(stream, parts):
    while True:
        data = stream.read1(16384)
        if not data:
            break
        parts.append(data)

threads = [threading.Thread(target=collect, args=(process.stdout, output), daemon=True),
           threading.Thread(target=collect, args=(process.stderr, errors), daemon=True)]
for thread in threads:
    thread.start()

startup_acknowledged = False

def observed(marker, seconds):
    global startup_acknowledged
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        data = b''.join(output)
        if marker in data:
            return True
        if not startup_acknowledged and b'Press ENTER or type command to continue' in data:
            process.stdin.write(b'\r')
            process.stdin.flush()
            startup_acknowledged = True
        if process.poll() is not None:
            break
        time.sleep(.05)
    return marker in b''.join(output)

report = {'status': 'failed', 'scope': 'Real Vim through test-only ConPTY pipes; not SSH or HwndTerminal visual acceptance',
          'vim': args.vim, 'vimSha256': hashlib.sha256(pathlib.Path(args.vim).read_bytes()).hexdigest(), 'checks': []}
try:
    if not observed(b'fullscreen-start-marker', 12):
        raise RuntimeError('Disposable file marker did not appear')
    report['checks'].append('Real Vim displayed disposable file')
    size.write_text('{"columns":96,"rows":30}', encoding='utf-8')
    time.sleep(.3)
    process.stdin.write(b":echo &columns . 'x' . &lines\r")
    process.stdin.flush()
    if not observed(b'96x30', 5):
        raise RuntimeError('Vim did not observe requested 96x30 grid')
    report['checks'].append('Real Vim observed resized grid')
    process.stdin.write(b'Gofullscreen-edit-marker')
    process.stdin.flush()
    time.sleep(.2)
    process.stdin.write(b'\x1b')
    process.stdin.flush()
    time.sleep(.5)
    process.stdin.write(b':wq\r')
    process.stdin.flush()
    code = process.wait(timeout=10)
    if code != 0 or 'fullscreen-edit-marker' not in file.read_text(encoding='utf-8'):
        raise RuntimeError('Vim edit/exit did not complete')
    report['checks'] += ['Vim persisted fixture edit', 'Vim and host exited']
    report['status'] = 'passed'
except Exception as error:
    report['error'] = str(error)
finally:
    if process.poll() is None:
        process.kill()
        process.wait(timeout=5)
    for thread in threads:
        thread.join(timeout=2)
    report['outputBytes'] = sum(map(len, output))
    report['stderr'] = b''.join(errors).decode('utf-8', errors='replace')[:500]
    file.with_suffix('.vt-output').write_bytes(b''.join(output))
    (root / 'evidence/tui-host-local-results.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report))
raise SystemExit(0 if report['status'] == 'passed' else 1)
