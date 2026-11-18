#pragma once
#include <cstring>
namespace re4 {
struct StartupOptions {bool recovery=false;};
inline StartupOptions startup_options(int argc,const char* const* argv){
 StartupOptions options;
 if(!argv)return options;
 for(int i=1;i<argc;++i)if(argv[i]&&!std::strcmp(argv[i],"--recovery"))options.recovery=true;
 return options;
}
}
