// Included in game_link.cpp's private namespace. Invoked only by explicit Monitor snapshots.
static_assert(re5::kCameraProbeAbi==1&&re5::kCameraProbeBytes==5808);
static_assert(re5::kCameraProbeOffset==sizeof(re5::SharedControl)+sizeof(re5::ImageIdentity)+sizeof(re5::RuntimeProbe));
struct Re5CameraHeader {
    char marker[16];u32 abi,size,sequence,request_sequence;u64 calls,unrouted_calls;
};
static_assert(sizeof(Re5CameraHeader)==offsetof(re5::CameraProbe,players));
struct Re5CameraSnapshot {const re5::CameraProbe* value=nullptr;const char* status="unavailable";};
Re5CameraSnapshot re5_camera_capture(Service* service,u64 control,u64 control_bytes,Re5Build build,Re5Extents main,u64 pid,re5::CameraProbe& buffer){
    Re5CameraSnapshot snapshot;
    constexpr auto offset=re5::kCameraProbeOffset,bytes=re5::kCameraProbeBytes;
    if(!control||build==Re5Build::None||main.base<0x1000||(main.base&0xfff)
        ||main.size!=re5::profile::main_size||main.size>UINT64_MAX-main.base
        ||control>UINT64_MAX-offset||control_bytes<offset+sizeof(Re5CameraHeader))return snapshot;
    const auto address=control+offset;
    if(address>UINT64_MAX-sizeof(Re5CameraHeader))return snapshot;
    auto identity=[&](){
        if(re5_image_identity(service,control,build,main,pid))return true;
        snapshot.status=re5_same_process(pid)?"identity_unverified":"identity_changed";return false;
    };
    auto read=[&](void* out,std::size_t size){
        if(!re5_same_process(pid)){snapshot.status="identity_changed";return false;}
        const auto result=re5_read(service,address,out,size);
        if(!re5_same_process(pid)){snapshot.status="identity_changed";return false;}
        if(R_FAILED(result)){snapshot.status="read_failed";return false;}return true;
    };
    if(!identity())return snapshot;
    Re5CameraHeader first{},last{};
    if(!read(&first,sizeof(first)))return snapshot;
    if(std::memcmp(first.marker,re5::kCameraProbeMarker,sizeof(first.marker)))return snapshot;
    if(first.abi!=re5::kCameraProbeAbi||first.size!=bytes){snapshot.status="invalid";return snapshot;}
    if(first.sequence&1){snapshot.status="busy";return snapshot;}
    if(bytes>control_bytes-offset||address>UINT64_MAX-bytes){snapshot.status="bounds";return snapshot;}
    if(!read(&buffer,sizeof(buffer)))return snapshot;
    if(std::memcmp(&first,&buffer,sizeof(first))){snapshot.status="changed";return snapshot;}
    if(!read(&last,sizeof(last)))return snapshot;
    if(std::memcmp(&first,&last,sizeof(first))){snapshot.status="changed";return snapshot;}
    if(!identity())return snapshot;
    // Image validation may span another native publication; compare once more.
    if(!read(&last,sizeof(last)))return snapshot;
    if(std::memcmp(&first,&last,sizeof(first))){snapshot.status="changed";return snapshot;}
    snapshot.value=&buffer;snapshot.status="ok";return snapshot;
}
bool re5_camera_json(const Re5CameraSnapshot& snapshot,char* out,std::size_t capacity){
    if(!out||capacity<5)return false;
    std::memcpy(out,"null",5);if(!snapshot.value)return true;
    static_assert(re5::kCameraProbeBytes%3==0);
    constexpr std::size_t encoded=4*(re5::kCameraProbeBytes/3);
    constexpr char prefix[]=R"({"status":"ok","encoding":"base64","abi":1,"bytes":5808,"data":")";
    static_assert(sizeof(prefix)-1+encoded+3<8192);
    if(sizeof(prefix)-1+encoded+3>capacity)return false;
    std::memcpy(out,prefix,sizeof(prefix)-1);std::size_t used=sizeof(prefix)-1;
    constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const auto* source=reinterpret_cast<const u8*>(snapshot.value);
    for(std::size_t i=0;i<sizeof(re5::CameraProbe);i+=3){
        const u32 value=(u32(source[i])<<16)|(u32(source[i+1])<<8)|source[i+2];
        out[used++]=alphabet[(value>>18)&63];out[used++]=alphabet[(value>>12)&63];
        out[used++]=alphabet[(value>>6)&63];out[used++]=alphabet[value&63];
    }
    out[used++]='"';out[used++]='}';out[used]=0;return true;
}
