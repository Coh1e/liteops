using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Text;

namespace PuTTYHub {
 [DataContract] public sealed class Session {
  [DataMember] public string Id=Guid.NewGuid().ToString("N");
  [DataMember] public string Name="";
  [DataMember] public string Group="默认";
  [DataMember] public string Host="";
  [DataMember] public int Port=22;
  [DataMember] public string User="";
  [DataMember] public string Auth="agent";
  [DataMember] public string CredentialId="";
  [DataMember] public string KeyFile="";
  [DataMember] public string RemoteDirectory="";
  [DataMember] public List<string> Bookmarks=new List<string>();
  [DataMember] public string PuttyProfile="";
  [DataMember] public string Source="";
  public Session Copy(){var s=(Session)MemberwiseClone();s.Id=Guid.NewGuid().ToString("N");s.Name+=" 副本";s.Bookmarks=new List<string>(Bookmarks??new List<string>());return s;}
  public void Validate(){
   if(string.IsNullOrWhiteSpace(Name)||Name.Length>150)throw new Exception("请填写会话名称（最多 150 字）。");
   if(string.IsNullOrWhiteSpace(Host)||Host.StartsWith("-")||Host.Any(c=>char.IsWhiteSpace(c)||char.IsControl(c)||"\"/\\@".IndexOf(c)>=0))throw new Exception("请填写主机名或 IP，不要包含协议、空格、路径或账号。");
   if(Port<1||Port>65535)throw new Exception("端口必须为 1–65535。");
   if((User??"").Any(char.IsControl))throw new Exception("用户名不能包含控制字符。");
   if(Auth!="agent"&&Auth!="password"&&Auth!="ask")throw new Exception("未知认证方式。");
  }
 }
 [DataContract] public sealed class SessionDocument {
  [DataMember] public int Version=1;
  [DataMember] public List<Session> Sessions=new List<Session>();
  [DataMember] public List<string> Groups=new List<string>{"默认"};
 }
 static class Files {
  public static readonly string Root=Path.GetFullPath(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,".."));
  public static readonly string Data=Path.Combine(Root,"data");
  public static readonly string Components=Path.Combine(Root,"components");
  public static void PrivateDirectory(string path){
   Directory.CreateDirectory(path);var sec=new DirectorySecurity();sec.SetAccessRuleProtection(true,false);
   sec.AddAccessRule(new FileSystemAccessRule(WindowsIdentity.GetCurrent().User,FileSystemRights.FullControl,InheritanceFlags.ContainerInherit|InheritanceFlags.ObjectInherit,PropagationFlags.None,AccessControlType.Allow));
   Directory.SetAccessControl(path,sec);
  }
  public static byte[] Serialize<T>(T obj){using(var ms=new MemoryStream()){new DataContractJsonSerializer(typeof(T)).WriteObject(ms,obj);return ms.ToArray();}}
  public static T Deserialize<T>(byte[] b){using(var ms=new MemoryStream(b))return (T)new DataContractJsonSerializer(typeof(T)).ReadObject(ms);}
  public static void Atomic(string path,byte[] b){
   var temp=path+"."+Guid.NewGuid().ToString("N")+".tmp";
   try{using(var f=new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None)){f.Write(b,0,b.Length);f.Flush(true);}if(File.Exists(path))File.Replace(temp,path,null);else File.Move(temp,path);}
   finally{if(File.Exists(temp))File.Delete(temp);}
  }
 }
 sealed class SessionStore {
  public SessionDocument Document;
  readonly string path;
  public SessionStore(string directory){Files.PrivateDirectory(directory);path=Path.Combine(directory,"sessions.json");Document=File.Exists(path)?Files.Deserialize<SessionDocument>(File.ReadAllBytes(path)):new SessionDocument();if(Document.Version!=1||Document.Sessions==null||Document.Groups==null)throw new Exception("会话库格式不支持。");}
  public void Save(){Files.Atomic(path,Files.Serialize(Document));}
  public void Export(string file){var d=new SessionDocument();d.Groups=new List<string>(Document.Groups);foreach(var s in Document.Sessions){var clone=Files.Deserialize<Session>(Files.Serialize(s));clone.CredentialId="";if(clone.Auth=="password")clone.Auth="ask";d.Sessions.Add(clone);}Files.Atomic(file,Files.Serialize(d));}
  public int Import(string file){var d=Files.Deserialize<SessionDocument>(File.ReadAllBytes(file));if(d.Version!=1||d.Sessions==null)throw new Exception("不是支持的会话文件。");foreach(var s in d.Sessions)s.Validate();foreach(var s in d.Sessions){s.Id=Guid.NewGuid().ToString("N");s.CredentialId="";if(s.Auth=="password")s.Auth="ask";Document.Sessions.Add(s);if(!Document.Groups.Contains(s.Group))Document.Groups.Add(s.Group);}Save();return d.Sessions.Count;}
 }
}
