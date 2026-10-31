#include "re6/capture_status.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <string>
#define CHECK(c) do {if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);}}while(0)
namespace fs=std::filesystem;
std::string contents(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
int main(){
 const auto directory=fs::temp_directory_path()/"re6-capture-status-tests";fs::remove_all(directory);fs::create_directory(directory);
 const auto path=(directory/"telemetry-status.csv").string();
 re6::CaptureStatus status{};status.target="RE6";status.target_title=0x123;status.actual_title=0x456;status.pid=9;status.expected_build[0]=0xAB;status.actual_build[31]=0xCD;
 status.phase=re6::CapturePhase::Metadata;status.ipc_result=0xCAFE;status.state=re6::RecorderState::Failed;status.error=re6::RecorderError::Attach;status.tick=42;
 char detail[]="wrong, \"build\"\r\nnext\x01";status.detail=detail;status.filename="capture.part";
 CHECK(re6::write_capture_status(path.c_str(),status));const auto first=contents(path);
 CHECK(first.find("schema,target,target_title,actual_title,pid,expected_build,actual_build,phase,ipc_result,recorder_state,recorder_error,records,dropped,errors,tick,detail,filename\n")==0);
 CHECK(first.find("\"wrong, \"\"build\"\"  next \"")!=std::string::npos);CHECK(first.find(",metadata,0000CAFE,failed,attach,")!=std::string::npos);
 CHECK(first.find("0000000000000123,0000000000000456,9,AB")!=std::string::npos);CHECK(first.find("CD,metadata")!=std::string::npos);
 detail[0]='X';CHECK(contents(path)==first);
 std::string huge(5000,'Q');status.detail=huge.c_str();status.records=7;status.dropped=2;status.phase=re6::CapturePhase::Read;
 CHECK(re6::write_capture_status(path.c_str(),status));const auto second=contents(path);CHECK(second!=first);CHECK(second.size()<re6::kCaptureStatusBytes);CHECK(second.find(",read,0000CAFE,failed,attach,7,2,")!=std::string::npos);
 CHECK(second.find(std::string(257,'Q'))==std::string::npos);CHECK(std::count(second.begin(),second.end(),'\n')==2);
 fs::create_directory(path+".tmp");CHECK(!re6::write_capture_status(path.c_str(),status));CHECK(contents(path)==second);fs::remove(path+".tmp");
 const auto invalid=(directory/"missing"/"status.csv").string();CHECK(!re6::write_capture_status(invalid.c_str(),status));CHECK(contents(path)==second);
 char tiny[10];CHECK(re6::format_capture_status(status,tiny,sizeof(tiny))==0);
 fs::remove_all(directory);std::puts("capture status tests passed");
}

