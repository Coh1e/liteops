#include "tools.hpp"
#include <iostream>

using namespace hub;
int wmain(){try{
 const auto directory=root()/L"tests"/wide("components-"+uuid());private_directory(directory);
 Components components(directory);std::atomic<bool>cancel=false;std::string previous;
 components.install([&](std::string progress){auto category=progress.substr(0,progress.find(" · "));if(category!=previous){std::cout<<progress<<"\n";previous=std::move(category);}},cancel);
 Json checks=Json::array();for(auto&spec:Components::catalog()){check(components.ready(spec.name),"Freshly downloaded component hash");checks.push_back(spec.name+" verified "+spec.sha256);}
 check(fs::exists(directory/L"putty-0.85/LICENCE")&&fs::exists(directory/L"terminal-1.24.12741.0/LICENSE")&&fs::exists(directory/L"terminal-1.24.12741.0/NOTICE.html")&&fs::exists(directory/L"winscp-6.7/license.txt"),"Redistribution license files present");checks.push_back("Component license files present");
 auto bytes=read_bytes(components.path("terminal"));bytes[0]^=1;write_bytes(components.path("terminal"),bytes);check(!components.ready("terminal"),"Reject tampered component");checks.push_back("Corrupt terminal component rejected");
 // Restore the fixture and ensure an already-cancelled install performs no downloads.
 bytes[0]^=1;write_bytes(components.path("terminal"),bytes);cancel=true;bool cancelled=false;try{components.install([](std::string){},cancel);}catch(const std::exception&){cancelled=true;}check(cancelled,"Component cancellation reported");for(auto&entry:fs::directory_iterator(directory))check(!entry.path().filename().wstring().starts_with(L"download-"),"Staging directory cleaned");checks.push_back("Cancelled install cleans staging directory");
 write_text(root()/L"evidence/component-results.json",Json{{"status","passed"},{"directory",utf8(directory.wstring())},{"checks",checks}}.dump(2));std::cout<<"Fresh component installation passed.\n";return 0;
 }catch(const std::exception&e){write_text(root()/L"evidence/component-error.txt",e.what());std::cerr<<e.what()<<"\n";return 1;}}
