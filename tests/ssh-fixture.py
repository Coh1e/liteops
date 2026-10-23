"""Loopback-only SSH/SFTP fixture. No shell commands or user files are exposed."""
import sys, pathlib, socket, threading, time, json, base64, hashlib, struct
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests' / 'deps'))
import paramiko

OUT = ROOT / 'tests' / 'ssh-fixture'
OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'stop').unlink(missing_ok=True)
(OUT / 'drop-transports').unlink(missing_ok=True)
SERVER_LOSS_TEST = '--server-loss' in sys.argv
AUTH_PENDING_TEST = '--auth-pending' in sys.argv
BLACKHOLE_TEST = '--blackhole' in sys.argv
blackhole = threading.Event()
for name in ('enable-blackhole', 'disable-blackhole'):
    (OUT / name).unlink(missing_ok=True)
auth_hold_claimed = False
(OUT / 'release-auth').unlink(missing_ok=True)
transport_lock = threading.Lock()
active_transports = set()
(OUT / 'events.jsonl').write_text('', encoding='utf-8')
key = paramiko.RSAKey.generate(2048)
fingerprint = 'SHA256:' + base64.b64encode(hashlib.sha256(key.asbytes()).digest()).decode().rstrip('=')
lock = threading.Lock()
def record(event, **fields):
    with lock:
        with (OUT / 'events.jsonl').open('a', encoding='utf-8') as f:
            f.write(json.dumps({'event':event, **fields}, ensure_ascii=False) + '\n')

class Server(paramiko.ServerInterface):
    def __init__(self):
        self.sizes = {}
        self.programs = {}
    def get_allowed_auths(self, username): return 'publickey' if 'AGENT_IDENTITY' in globals() else 'password'
    def check_auth_publickey(self, username, offered):
        ok = 'AGENT_IDENTITY' in globals() and username == 'fixture' and offered.asbytes() == AGENT_IDENTITY.asbytes()
        record('publickey_auth', accepted=ok)
        return paramiko.AUTH_SUCCESSFUL if ok else paramiko.AUTH_FAILED
    def check_auth_password(self, username, password):
        global auth_hold_claimed
        hold = False
        if AUTH_PENDING_TEST:
            with lock:
                if not auth_hold_claimed:
                    auth_hold_claimed = True
                    hold = True
        if hold:
            record('auth_pending')
            until = time.monotonic() + 60
            while time.monotonic() < until and not (OUT / 'release-auth').exists() and not (OUT / 'stop').exists():
                time.sleep(0.02)
            record('auth_pending_released')
            return paramiko.AUTH_FAILED
        ok = 'AGENT_IDENTITY' not in globals() and username == 'fixture' and password == 'probe-假密码-2026'
        record('password_auth', accepted=ok)
        return paramiko.AUTH_SUCCESSFUL if ok else paramiko.AUTH_FAILED
    def check_channel_request(self, kind, chanid):
        return paramiko.OPEN_SUCCEEDED if kind == 'session' else paramiko.OPEN_FAILED_ADMINISTRATIVELY_PROHIBITED
    def check_channel_pty_request(self, channel, term, width, height, *args):
        self.sizes[channel.chanid] = (width,height)
        record('pty_size', width=width, height=height); return True
    def check_channel_window_change_request(self, channel, width, height, *args):
        self.sizes[channel.chanid] = (width,height)
        if channel.chanid in self.programs: self.programs[channel.chanid].resize(width,height)
        record('pty_resize', width=width, height=height); return True
    def check_channel_shell_request(self, channel):
        record('shell_open')
        if 'VIM_FACTORY' in globals():
            width,height = self.sizes.get(channel.chanid,(80,24))
            self.programs[channel.chanid] = VIM_FACTORY(channel,width,height,record)
            return True
        channel.send(b'PuTTY Hub local integration fixture\r\nNo real commands are executed.\r\nfixture> ')
        return True

class SFTP(paramiko.SFTPServerInterface):
    def canonicalize(self, path): return '/'
    def list_folder(self, path):
        record('sftp_list')
        item=paramiko.SFTPAttributes(); item.filename='integration-ok.txt'; item.st_mode=0o100644; item.st_size=0; item.st_uid=0;item.st_gid=0;item.st_atime=item.st_mtime=int(time.time())
        return [item]
    def stat(self, path):
        item=paramiko.SFTPAttributes(); item.st_mode=0o040755;item.st_size=0;item.st_uid=0;item.st_gid=0;item.st_atime=item.st_mtime=int(time.time());return item
    lstat=stat

def handle(client):
    t = None
    server = None
    try:
        t=paramiko.Transport(client);t.add_server_key(key);t.set_subsystem_handler('sftp',paramiko.SFTPServer,SFTP)
        with transport_lock: active_transports.add((t, client))
        server = Server()
        t.start_server(server=server)
        channels=[]
        received={}
        unicode_seen=set()
        while t.is_active():
            channel=t.accept(1)
            if channel: channels.append(channel)
            for active in channels:
                if active.recv_ready():
                    data=active.recv(16384)
                    if active.chanid in server.programs:
                        server.programs[active.chanid].input(data)
                        continue
                    if b'probe-input' in data: record('input_seen', probe_hex=data[:128].hex())
                    received[active.chanid]=(received.get(active.chanid,b'')+data)[-65536:]
                    if active.chanid not in unicode_seen and b'probe-input\t'+ '中文😀'.encode('utf-8') in received[active.chanid]:
                        unicode_seen.add(active.chanid)
                        record('unicode_input_seen')
                    active.send(b'echo: '+data+b'\r\nfixture> ')
        t.close()
    except Exception as e: record('connection_closed', type=type(e).__name__)
    finally:
        if server is not None:
            for program in list(server.programs.values()): program.close()
        if t is not None:
            with transport_lock: active_transports.discard((t, client))
            t.close()
        client.close()

def proxy_pump(source, target, direction):
    source.settimeout(0.2)
    try:
        while not (OUT / 'stop').exists():
            try: data = source.recv(65536)
            except socket.timeout: continue
            if not data: break
            if blackhole.is_set(): record('blackhole_drop', direction=direction, bytes=len(data))
            else: target.sendall(data)
    except OSError: pass
    finally:
        for connection in (source, target):
            try: connection.shutdown(socket.SHUT_RDWR)
            except OSError: pass
            connection.close()

def proxy_accept(listener, target_port):
    listener.settimeout(0.2)
    try:
        while not (OUT / 'stop').exists():
            try: source, _ = listener.accept()
            except socket.timeout: continue
            target = socket.create_connection(('127.0.0.1', target_port), timeout=2)
            threading.Thread(target=proxy_pump, args=(source,target,'client_to_server'), daemon=True).start()
            threading.Thread(target=proxy_pump, args=(target,source,'server_to_client'), daemon=True).start()
    finally: listener.close()

with socket.socket() as listener:
    listener.bind(('127.0.0.1',0));listener.listen(10);listener.settimeout(1)
    advertised_port = listener.getsockname()[1]
    if BLACKHOLE_TEST:
        relay = socket.socket(); relay.bind(('127.0.0.1',0)); relay.listen(10)
        advertised_port = relay.getsockname()[1]
        threading.Thread(target=proxy_accept,args=(relay,listener.getsockname()[1]),daemon=True).start()
    (OUT / 'connection.json').write_text(json.dumps({'port':advertised_port, 'fingerprint':fingerprint, 'faultMode':globals().get('FIXTURE_MODE', 'blackhole' if BLACKHOLE_TEST else 'auth_pending' if AUTH_PENDING_TEST else 'server_transport' if SERVER_LOSS_TEST else 'upstream_process')}), encoding='utf-8')
    record('ready')
    deadline=time.time()+180
    while time.time()<deadline and not (OUT / 'stop').exists():
        if BLACKHOLE_TEST:
            for name, enabled in (('enable-blackhole', True), ('disable-blackhole', False)):
                if (OUT / name).exists():
                    (OUT / name).unlink()
                    if enabled: blackhole.set()
                    else: blackhole.clear()
                    record('blackhole_enabled' if enabled else 'blackhole_disabled')
        if SERVER_LOSS_TEST and (OUT / 'drop-transports').exists():
            (OUT / 'drop-transports').unlink()
            with transport_lock: closing = list(active_transports)
            for transport, client in closing:
                try: client.shutdown(socket.SHUT_RDWR)
                except OSError: pass
                client.close()
                transport.close()
            record('transport_drop', closed=len(closing))
        try: client,_=listener.accept()
        except socket.timeout: continue
        threading.Thread(target=handle,args=(client,),daemon=True).start()
