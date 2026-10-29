#include "re4/update.hpp"
#include "nro_fixture.hpp"
#include <switch.h>
#include <curl/curl.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <cstdio>
void wait(re4::UpdateWorker& worker){
    for(unsigned i=0;i<2000 && worker.busy();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(!worker.busy());
}
int main(){
    using namespace re4;namespace fs=std::filesystem;
    fs::create_directories("build/update-worker-test");
    curl_test::nro=own_nro_fixture("9.0.0");auto& nro=curl_test::nro;
    unsigned char hash[32];sha256CalculateHash(hash,nro.data(),nro.size());char hex[65];
    for(unsigned i=0;i<32;++i)std::snprintf(hex+i*2,3,"%02x",hash[i]);
    curl_test::manifest=std::string("RE4-CONTROLS-UPDATE 1\nversion=9.0.0\nbytes=")+std::to_string(nro.size())+"\nsha256="+hex+"\n";
    {
        UpdateWorker worker;assert(!worker.install());
        curl_test::status=404;assert(worker.check());wait(worker);
        assert(worker.snapshot().stage==UpdateStage::Failed && worker.snapshot().detail.find("404")!=std::string::npos);
        curl_test::status=200;assert(worker.check());wait(worker);
        assert(worker.snapshot().stage==UpdateStage::Available && worker.snapshot().version=="9.0.0");
        assert(worker.install());wait(worker);assert(worker.snapshot().stage==UpdateStage::Installed);
        assert(fs::file_size("build/update-worker-test/game.nro")==nro.size());
        assert(fs::file_size("build/update-worker-test/bridge.nro")==nro.size());
        curl_test::slow=true;assert(worker.check());worker.cancel();wait(worker);
        assert(worker.snapshot().stage==UpdateStage::Cancelled);
        curl_test::slow=false;
    }
    assert(curl_test::opened==curl_test::closed);
    std::puts("actual OTA worker: verified HTTPS options, private404, check/download/install, cancellation and cleanup PASS");
}
