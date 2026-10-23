"""Create disposable PPK v2 fixtures, then serve loopback public-key-only SSH."""
import sys, pathlib, base64, struct, hashlib, hmac, runpy
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'/'deps'))
import paramiko
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

out=ROOT/'tests'/'ssh-fixture'
out.mkdir(parents=True,exist_ok=True)
identity=paramiko.RSAKey.generate(2048)
numbers=identity.key.private_numbers()
def string(value):return struct.pack('>I',len(value))+value
def mpint(value):
 data=value.to_bytes((value.bit_length()+7)//8,'big')
 if data and data[0]&128:data=b'\0'+data
 return string(data)
private=b''.join(mpint(value) for value in (numbers.d,numbers.p,numbers.q,numbers.iqmp))
public=identity.asbytes()
def ppk(password):
 encryption=b'aes256-cbc' if password else b'none'
 clear=private
 if password:clear+=hashlib.sha1(private).digest()[:(-len(private))%16]
 comment=b'PuTTY Hub disposable test key'
 mac_key=hashlib.sha1(b'putty-private-key-file-mac-key'+password).digest()
 mac=hmac.new(mac_key,b''.join(string(value) for value in (b'ssh-rsa',encryption,comment,public,clear)),hashlib.sha1).hexdigest()
 if password:
  key=b''.join(hashlib.sha1(struct.pack('>I',index)+password).digest() for index in (0,1))[:32]
  encryptor=Cipher(algorithms.AES(key),modes.CBC(bytes(16))).encryptor()
  clear=encryptor.update(clear)+encryptor.finalize()
 def lines(value):
  encoded=base64.b64encode(value).decode()
  return [encoded[index:index+64] for index in range(0,len(encoded),64)]
 pub,priv=lines(public),lines(clear)
 return '\n'.join(['PuTTY-User-Key-File-2: ssh-rsa','Encryption: '+encryption.decode(),'Comment: '+comment.decode(),'Public-Lines: '+str(len(pub)),*pub,'Private-Lines: '+str(len(priv)),*priv,'Private-MAC: '+mac,''])
(out/'agent-test.ppk').write_text(ppk(b''),encoding='ascii')
(out/'agent-encrypted-test.ppk').write_text(ppk(b'fixture passphrase 2026'),encoding='ascii')
runpy.run_path(str(ROOT/'tests'/'ssh-fixture.py'),init_globals={'AGENT_IDENTITY':identity},run_name='__main__')
