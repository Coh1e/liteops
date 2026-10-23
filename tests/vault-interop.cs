using System;
using System.IO;
namespace PuTTYHub {
 static class InteropFixture {
  static int Main(string[] args){try{var directory=Path.GetFullPath(args[1]);using(var v=new Vault(directory)){
   if(args[0]=="create"){v.Create("interop master password 2026");var credential=v.Put("","interop","C# fixture 秘密");var sessions=new SessionStore(directory);sessions.Document.Sessions.Add(new Session{Name="C# migration fixture",Host="example.test",User="fixture",Auth="password",CredentialId=credential});sessions.Save();File.WriteAllText(Path.Combine(directory,"created.txt"),"C# fixture generated");}
   else {v.Unlock("interop master password 2026");if(v.Entries.Length!=1||v.Entries[0].Password!="C++ updated 秘密")throw new Exception("C++ vault not readable from C#");File.WriteAllText(Path.Combine(directory,"reverse-result.txt"),"PASS: C# read C++ updated vault");}
  }return 0;}catch(Exception ex){File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"interop-error.txt"),ex.ToString());return 1;}}
 }
}
