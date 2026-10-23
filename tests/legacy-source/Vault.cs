using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.Serialization;
using System.Security.Cryptography;
using System.Text;

namespace PuTTYHub {
 [DataContract] public sealed class SecretEntry {
  [DataMember] public string Id;
  [DataMember] public string Name;
  [DataMember] public string Password;
 }
 [DataContract] public sealed class VaultPayload { [DataMember] public List<SecretEntry> Entries=new List<SecretEntry>(); }
 sealed class Vault : IDisposable {
  readonly string path;byte[] key;byte[] salt;VaultPayload payload;
  public bool Exists {get{return File.Exists(path);}}
  public bool Unlocked {get{return key!=null;}}
  public Vault(string dir){Files.PrivateDirectory(dir);path=Path.Combine(dir,"vault.bin");}
  public void Create(string pass){if(Exists)throw new Exception("密码库已存在。");if(pass.Length<10)throw new Exception("主密码至少需要 10 个字符。");salt=Random(32);key=Derive(pass,salt);payload=new VaultPayload();try{Save();}catch{Lock();throw;}}
  static byte[] Random(int n){var b=new byte[n];using(var r=RandomNumberGenerator.Create())r.GetBytes(b);return b;}
  static byte[] Derive(string pass,byte[] salt){using(var k=new Rfc2898DeriveBytes(pass,salt,600000,HashAlgorithmName.SHA256))return k.GetBytes(64);}
  static byte[] Part(byte[] b,int offset,int n){var v=new byte[n];Buffer.BlockCopy(b,offset,v,0,n);return v;}
  public void Unlock(string pass){
   Lock();byte[] candidate=null,plain=null,outer=null;
   try{
    outer=ProtectedData.Unprotect(File.ReadAllBytes(path),null,DataProtectionScope.CurrentUser);
    if(outer.Length<100||Encoding.ASCII.GetString(outer,0,4)!="PHV1")throw new CryptographicException();
    salt=Part(outer,4,32);candidate=Derive(pass,salt);
    using(var h=new HMACSHA256(Part(candidate,32,32))){var tag=h.ComputeHash(outer,0,outer.Length-32);int diff=0;for(int i=0;i<32;i++)diff|=tag[i]^outer[outer.Length-32+i];if(diff!=0)throw new CryptographicException();}
    using(var aes=Aes.Create()){aes.Key=Part(candidate,0,32);aes.IV=Part(outer,36,16);using(var t=aes.CreateDecryptor())plain=t.TransformFinalBlock(outer,52,outer.Length-84);}
    payload=Files.Deserialize<VaultPayload>(plain);key=candidate;candidate=null;
   }catch{Lock();throw new Exception("无法解锁密码库：主密码不正确、当前 Windows 用户不匹配，或文件已损坏。");}
   finally{if(candidate!=null)Array.Clear(candidate,0,candidate.Length);if(plain!=null)Array.Clear(plain,0,plain.Length);if(outer!=null)Array.Clear(outer,0,outer.Length);}
  }
  public SecretEntry[] Entries {get{Require();return payload.Entries.ToArray();}}
  void Require(){if(!Unlocked)throw new Exception("请先解锁密码库。");}
  public string Get(string id){Require();var entry=payload.Entries.Find(e=>e.Id==id);if(entry==null)throw new Exception("会话引用的密码不存在，请编辑会话。");return entry.Password;}
  public string Put(string id,string name,string password){Require();if(password.IndexOfAny(new[]{'\r','\n','\0'})>=0)throw new Exception("密码不能包含换行或 NUL。");var e=payload.Entries.Find(x=>x.Id==id);if(e==null){e=new SecretEntry{Id=Guid.NewGuid().ToString("N")};payload.Entries.Add(e);}e.Name=name;e.Password=password;Save();return e.Id;}
  public void Delete(string id){Require();payload.Entries.RemoveAll(e=>e.Id==id);Save();}
  void Save(){
   Require();byte[] plain=Files.Serialize(payload);byte[] raw=null;
   try{using(var aes=Aes.Create()){aes.Key=Part(key,0,32);aes.GenerateIV();byte[] cipher;using(var t=aes.CreateEncryptor())cipher=t.TransformFinalBlock(plain,0,plain.Length);raw=new byte[52+cipher.Length+32];Encoding.ASCII.GetBytes("PHV1").CopyTo(raw,0);salt.CopyTo(raw,4);aes.IV.CopyTo(raw,36);cipher.CopyTo(raw,52);using(var h=new HMACSHA256(Part(key,32,32)))h.ComputeHash(raw,0,raw.Length-32).CopyTo(raw,raw.Length-32);Files.Atomic(path,ProtectedData.Protect(raw,null,DataProtectionScope.CurrentUser));}}
   finally{Array.Clear(plain,0,plain.Length);if(raw!=null)Array.Clear(raw,0,raw.Length);}
  }
  public void Lock(){if(key!=null)Array.Clear(key,0,key.Length);key=null;salt=null;if(payload!=null){foreach(var e in payload.Entries)e.Password=null;payload.Entries.Clear();}payload=null;}
  public void Dispose(){Lock();}
 }
}
