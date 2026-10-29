#pragma once
#include <cassert>
#include <chrono>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
using curl_off_t=long long;using CURLcode=int;
using CURLoption=int;
enum {CURLE_OK=0,CURL_GLOBAL_DEFAULT=0,CURLPROTO_HTTPS=2,CURL_SSLVERSION_TLSv1_2=6,CURL_SSLVERSION_MAX_TLSv1_2=0x60000};
enum {CURLOPT_URL,CURLOPT_USERAGENT,CURLOPT_FOLLOWLOCATION,CURLOPT_MAXREDIRS,CURLOPT_PROTOCOLS,CURLOPT_REDIR_PROTOCOLS,
      CURLOPT_SSL_VERIFYPEER,CURLOPT_SSL_VERIFYHOST,CURLOPT_SSLVERSION,CURLOPT_CONNECTTIMEOUT,CURLOPT_TIMEOUT,CURLOPT_LOW_SPEED_LIMIT,
      CURLOPT_LOW_SPEED_TIME,CURLOPT_NOSIGNAL,CURLOPT_FAILONERROR,CURLOPT_WRITEFUNCTION,CURLOPT_WRITEDATA,CURLOPT_NOPROGRESS,
      CURLOPT_XFERINFOFUNCTION,CURLOPT_XFERINFODATA,CURLINFO_RESPONSE_CODE};
struct CURL {
    std::string url;std::map<int,long> options;void* data=nullptr;void* progress_data=nullptr;
    std::size_t(*write)(char*,std::size_t,std::size_t,void*)=nullptr;
    int(*progress)(void*,curl_off_t,curl_off_t,curl_off_t,curl_off_t)=nullptr;
    long status=200;
};
namespace curl_test {inline std::string manifest;inline std::vector<unsigned char> nro;inline long status=200;inline bool slow=false;inline unsigned opened=0,closed=0;}
inline int curl_global_init(long){++curl_test::opened;return 0;}inline void curl_global_cleanup(){++curl_test::closed;}
inline CURL* curl_easy_init(){return new CURL;}inline void curl_easy_cleanup(CURL* curl){delete curl;}
template<class T> int curl_easy_setopt(CURL* curl,int option,T value){
    if constexpr(std::is_integral_v<T>)curl->options[option]=long(value);
    else if constexpr(std::is_convertible_v<T,const char*>){if(option==CURLOPT_URL)curl->url=value;}
    else if constexpr(std::is_convertible_v<T,decltype(curl->write)>){if(option==CURLOPT_WRITEFUNCTION)curl->write=value;}
    else if constexpr(std::is_convertible_v<T,decltype(curl->progress)>){if(option==CURLOPT_XFERINFOFUNCTION)curl->progress=value;}
    else if constexpr(std::is_convertible_v<T,void*>){if(option==CURLOPT_WRITEDATA)curl->data=value;if(option==CURLOPT_XFERINFODATA)curl->progress_data=value;}
    return 0;
}
inline int curl_easy_perform(CURL* curl){
    assert(curl->url.rfind("https://github.com/EscHelpNew/re4-controll-mod/releases/latest/download/",0)==0);
    assert(curl->options[CURLOPT_PROTOCOLS]==CURLPROTO_HTTPS && curl->options[CURLOPT_REDIR_PROTOCOLS]==CURLPROTO_HTTPS);
    assert(curl->options[CURLOPT_SSL_VERIFYPEER]==1 && curl->options[CURLOPT_SSL_VERIFYHOST]==2);
    assert(curl->options[CURLOPT_FAILONERROR]==1 && curl->options[CURLOPT_MAXREDIRS]<=5);
    curl->status=curl_test::status;if(curl->status!=200)return 22;
    const bool manifest=curl->url.find(".txt")!=curl->url.npos;
    std::vector<unsigned char> data=manifest?std::vector<unsigned char>(curl_test::manifest.begin(),curl_test::manifest.end()):curl_test::nro;
    for(std::size_t offset=0;offset<data.size();){
        if(curl->progress(curl->progress_data,0,0,0,0))return 42;
        if(curl_test::slow)std::this_thread::sleep_for(std::chrono::milliseconds(5));
        const auto bytes=std::min<std::size_t>(128,data.size()-offset);
        if(curl->write(reinterpret_cast<char*>(data.data()+offset),1,bytes,curl->data)!=bytes)return 23;
        offset+=bytes;
    }
    return 0;
}
inline int curl_easy_getinfo(CURL* curl,int,long* status){*status=curl->status;return 0;}
