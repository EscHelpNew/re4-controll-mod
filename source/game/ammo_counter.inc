// Spare rounds in the attache case beside the native lower-right gauge.
// Native evidence: data/analysis/ammo_counter (reload 299320, CountItem
// 298310, gauge update 1EA30, sprite update 28FEA0, draw 290CE0).
struct AmmoCounterNative {
    void* (*sprite)(void*,unsigned,unsigned)=nullptr; // 28C8B0
    void (*classify)(unsigned,void*)=nullptr;         // 2970D0
    unsigned (*capacity)(unsigned,int)=nullptr;       // 293C90
} ammo_counter_native;
// A cached word/digit can survive a native root hide. Authorize both additions
// only after the real player-health arc7 reaches the existing290F00 callback.
struct AmmoCounterGaugeDraw {
    std::uint64_t frame=0;
    std::uintptr_t state=0,manager=0,arc=0;
    float alpha=0;
} ammo_counter_gauge_draw;
bool ammo_counter_gauge_sample(std::uintptr_t arc,std::uintptr_t state,std::uintptr_t manager,float& alpha){
    if(!arc||!ammo_counter_native.sprite||!state||!manager)return false;
    const auto root=reinterpret_cast<std::uintptr_t>(ammo_counter_native.sprite(reinterpret_cast<void*>(manager),0,0x21));
    if(!root||root==main_base+0x124D630
        ||arc!=reinterpret_cast<std::uintptr_t>(ammo_counter_native.sprite(reinterpret_cast<void*>(manager),7,0x21)))return false;
    std::array<std::uint8_t,0x170> sprite{};
    if(!modern_move_read(arc,sprite)||sprite[0]==0xFF||(sprite[0]&9)!=9
        ||sprite[2]!=0x21||sprite[3]!=7||sprite[0x88]!=0x20||sprite[0x9C]!=0x16||sprite[0x9E])return false;
    std::array<float,4> rgba{};std::memcpy(rgba.data(),sprite.data()+0x104,sizeof(rgba));
    for(float v:rgba)if(!std::isfinite(v)||v<0||v>255)return false;
    alpha=rgba[3];if(alpha<1)return false;
    std::uintptr_t parent=0;std::memcpy(&parent,sprite.data()+0x68,sizeof(parent));
    bool rooted=false;
    // The authenticated resource has55 records. A cycle cannot be a valid
    // gauge hierarchy; never follow an unbounded or foreign parent chain.
    for(unsigned depth=0;parent&&depth<55;++depth){
        std::array<std::uint8_t,0x170> node{};
        if(!modern_move_read(parent,node)||node[0]==0xFF||(node[0]&9)!=9
            ||node[2]!=0x21||node[4]!=1||node[0x9C]!=0x16)return false;
        float fade=0;std::memcpy(&fade,node.data()+0x110,sizeof(fade));
        if(!std::isfinite(fade)||fade<1||fade>255)return false;
        alpha=std::min(alpha,fade);
        std::uintptr_t next=0;std::memcpy(&next,node.data()+0x68,sizeof(next));
        if(parent==root){if(next)return false;rooted=true;break;}
        parent=next;
    }
    if(!rooted)return false;
    re4::ViewMatrix3x4 world,hud_view;re4::ViewMatrix4x4 projection;
    std::uintptr_t view=0;re4::WeaponPopupViewport viewport;
    if(!modern_move_read(state+0x10C,projection)||!modern_move_read(main_base+0xB275B8,view)
        ||!view||!modern_move_read(view,hud_view)||!weapon_popup_viewport_sample(viewport))return false;
    std::memcpy(&world,sprite.data()+8,sizeof(world));
    float corners[4][3]{};std::memcpy(corners,sprite.data()+0xBC,sizeof(corners));
    float left=512,right=0,top=448,bottom=0;
    for(const auto& corner:corners){
        re4::ViewVec3 point;re4::ViewHudPoint screen;
        if(!re4::view_transform_point(world,{corner[0],corner[1],corner[2]},point)
            ||!re4::project_view_point(hud_view,projection,point,screen))return false;
        const float x=float((viewport.sx*(2*double(screen.x)/512-1)+viewport.tx+1)*256);
        const float y=float((1-(viewport.sy*(1-2*double(screen.y)/448)+viewport.ty))*224);
        if(!std::isfinite(x)||!std::isfinite(y))return false;
        left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);
    }
    return right>left&&bottom>top&&right>0&&left<512&&bottom>0&&top<448;
}
void ammo_counter_gauge_observe(void* pointer){
    const auto arc=reinterpret_cast<std::uintptr_t>(pointer);std::array<std::uint8_t,4> tag{};
    if(!arc||!modern_move_read(arc,tag)||tag[2]!=0x21||tag[3]!=7)return;
    ammo_counter_gauge_draw={};std::uintptr_t state=0,manager=0;float alpha=0;
    if(!main_base||!native_hud_ready()||!modern_move_read(main_base+0xB1AE08,state)
        ||!modern_move_read(main_base+0xB25D98,manager)||!ammo_counter_gauge_sample(arc,state,manager,alpha))return;
    ammo_counter_gauge_draw={view_frame.load(std::memory_order_acquire),state,manager,arc,alpha};
}
bool ammo_counter_gauge_visible(float& alpha){
    const auto draw=ammo_counter_gauge_draw;std::uintptr_t state=0,manager=0;float current=0;
    if(!draw.frame||draw.frame!=view_frame.load(std::memory_order_acquire)
        ||!modern_move_read(main_base+0xB1AE08,state)||state!=draw.state
        ||!modern_move_read(main_base+0xB25D98,manager)||manager!=draw.manager
        ||!ammo_counter_gauge_sample(draw.arc,state,manager,current))return false;
    alpha=std::min(draw.alpha,current);return alpha>=1;
}
struct AmmoCounterSprite {
    std::uintptr_t address=0;
    float x=0,y=0,z=0;re4::ExtendedUpgradeQuad quad;
    std::array<float,4> rgba{};
};
bool ammo_counter_sprite(void* manager,unsigned id,AmmoCounterSprite& out){
    if(!ammo_counter_native.sprite)return false;
    const auto sprite=reinterpret_cast<std::uintptr_t>(ammo_counter_native.sprite(manager,id,0x21));
    std::uint8_t flags=0;float corners[4][3]{};
    // 28C8B0 returns the shared dummy record for a missing id.
    if(!sprite||sprite==main_base+0x124D630||!modern_move_read(sprite,flags)||flags==0xFF||(flags&9)!=9
        ||!modern_move_read(sprite+0x14,out.x)||!modern_move_read(sprite+0x24,out.y)||!modern_move_read(sprite+0x34,out.z)
        ||!modern_move_read(sprite+0xBC,corners)||!modern_move_read(sprite+0x104,out.rgba))return false;
    out.quad={corners[0][0],corners[0][1],corners[0][0],corners[0][1]};
    for(const auto& c:corners){
        out.quad.min_x=std::min(out.quad.min_x,c[0]);out.quad.max_x=std::max(out.quad.max_x,c[0]);
        out.quad.min_y=std::min(out.quad.min_y,c[1]);out.quad.max_y=std::max(out.quad.max_y,c[1]);
    }
    for(float v:{out.x,out.y,out.z,out.quad.min_x,out.quad.min_y,out.quad.max_x,out.quad.max_y})
        if(!std::isfinite(v)||std::fabs(v)>4000.f)return false;
    for(float v:out.rgba)if(!std::isfinite(v)||v<0||v>256.f)return false;
    out.address=sprite;return true;
}
// Sprite plane z=0 is a centered 640x480 screen; HUD view z=-240/tan(fovy/2)
// (28FF00..28FF5C). The late HUD is 512x448 with Y down.
bool ammo_counter_screen(const AmmoCounterSprite& s,float fovy,float x,float y,float& out_x,float& out_y){
    float k=1;
    if(std::isfinite(fovy)&&fovy>1&&fovy<179){
        const float z0=240.f/std::tan(fovy*3.14159265f/360.f);
        if(z0-s.z>1)k=z0/(z0-s.z);
    }
    out_x=256.f+.8f*(s.x+x)*k;out_y=224.f-(448.f/480.f)*(s.y+y)*k;
    return std::isfinite(out_x)&&std::isfinite(out_y);
}
bool ammo_counter_total(std::uint16_t& total){
    const auto inv=main_base+kExtendedInventory;
    std::uint16_t effective=0;std::uintptr_t equipped=0,slots=0;std::int32_t count=0;std::uint8_t owner=0;
    std::uint32_t packed=0;
    if(!modern_move_read(inv+0x18,effective)||!modern_move_read(inv+0x10,equipped)||!equipped
        ||!modern_move_read(inv+0x1B,owner)||owner==1||!modern_move_read(equipped+6,packed)
        ||!modern_move_read(inv+0x20,slots)||!modern_move_read(inv+0x30,count)||count<1||count>384
        ||!ammo_counter_native.classify||!ammo_counter_native.capacity||effective>0x10F)return false;
    std::array<std::uint8_t,6> type{};ammo_counter_native.classify(effective,type.data());
    if(type[2]!=1)return false;
    // Ammo id: first weapon-data row (A07CC6, 0x2B x 0x16) with this id and variant.
    std::uint16_t ammo=0xFFFF;const unsigned variant=(packed>>16)&7u;
    for(unsigned row=0;row<0x2B;++row){
        const auto base=main_base+0xA07CC6+row*0x16;std::uint16_t raw=0;std::uint8_t row_variant=0;
        if(!modern_move_read(base,raw)||!modern_move_read(base+2,row_variant))return false;
        if(raw==effective&&row_variant==variant){if(!modern_move_read(base+6,ammo))return false;break;}
    }
    const unsigned capacity=ammo_counter_native.capacity(effective,int((packed>>12)&15u)+1);
    if(!re4::ammo_counter_supported(effective,ammo,capacity))return false;
    re4::ModernMoveMemory memory;
    if(!modern_move_region(slots,std::size_t(count)*14,false,memory))return false;
    std::array<re4::AmmoCounterRecord,384> records{};
    for(std::int32_t i=0;i<count;++i){
        const auto r=slots+std::uintptr_t(i)*14;auto& out=records[std::size_t(i)];
        if(!memory.value(r,out.raw)||!memory.value(r+2,out.quantity)||!memory.value(r+4,out.flags)||!memory.value(r+5,out.owner))return false;
    }
    total=re4::ammo_counter_total(records.data(),std::size_t(count),ammo,owner);
    return true;
}
void draw_case_ammo_hud(){
    if(!settings.case_ammo_counter||!main_base||!native_hud_ready())return;
    float gauge_alpha=0;if(!ammo_counter_gauge_visible(gauge_alpha))return;
    std::uintptr_t state=0,manager=0,hud=0;std::uint32_t flags7c=0,flags78=0,groups=0;std::uint8_t gate=0,icon=0,character=0;
    float fovy=0;
    // 290D08 hides every native ID; 290D28..290D4C additionally skips
    // render layer0x16 on bit16. The gauge uses that layer (1E7A8..1E7D0),
    // even when its cached sprite flags and fade alpha still look visible.
    if(!modern_move_read(main_base+0xB1AE08,state)||!state||!modern_move_read(state+0x7C,flags7c)||(flags7c&((1u<<13)|(1u<<16)))
        ||!modern_move_read(state+0x78,flags78)||!modern_move_read(state+0x5092,gate)
        ||((flags78&(1u<<20))&&!(gate&4))||!modern_move_read(state+0x503C,character)
        ||!modern_move_read(state+0x168,fovy))return;
    // Gauge group 0x21 hidden by 28C650 sets mgr+30 bit30.
    if(!modern_move_read(main_base+0xB25D98,manager)||!manager||!modern_move_read(manager+0x30,groups)
        ||(groups&0x40000000u))return;
    // Cached icon id (1F5D0): 0x80 P.R.L. charge meter, 0xFF none.
    if(!modern_move_read(main_base+0xB25D80,hud)||!hud||!modern_move_read(hud+0x90,icon)||icon==0x80||icon==0xFF)return;
    constexpr std::uint8_t plates[]={0x40,0x41,0x42,0x45,0x43,0x44};
    if(character>=sizeof(plates))return;
    auto* m=reinterpret_cast<void*>(manager);
    AmmoCounterSprite root,ones,plate;
    if(!ammo_counter_sprite(m,0,root)||!ammo_counter_sprite(m,0x0B,ones)||!ammo_counter_sprite(m,plates[character],plate))return;
    const float alpha=std::min({root.rgba[3],ones.rgba[3],gauge_alpha});
    if(alpha<1)return;
    std::uint16_t total=0;if(!ammo_counter_total(total))return;
    float plate_left=0,plate_top=0,plate_right=0,plate_bottom=0,digit_top=0,digit_bottom=0,unused=0;
    if(!ammo_counter_screen(plate,fovy,plate.quad.min_x,plate.quad.max_y,plate_left,plate_top)
        ||!ammo_counter_screen(plate,fovy,plate.quad.max_x,plate.quad.min_y,plate_right,plate_bottom)
        ||!ammo_counter_screen(ones,fovy,0,ones.quad.max_y,unused,digit_top)
        ||!ammo_counter_screen(ones,fovy,0,ones.quad.min_y,unused,digit_bottom))return;
    // About 0.45 of the native magazine digits, centered in the original
    // three-digit field just above the name plate.
    const float height=std::clamp((digit_bottom-digit_top)*.45f,6.f,40.f);
    const float right=plate_right-(plate_right-plate_left)*.06f,bottom=plate_top-height*.25f;
    re4::AmmoCounterQuad quads[re4::kAmmoCounterMaxQuads];
    const unsigned count=re4::ammo_counter_quads(total,right,bottom,height,quads);
    if(!count)return;
    const bool empty=!total;
    const unsigned red=empty?194:unsigned(ones.rgba[0]),green=empty?80:unsigned(ones.rgba[1]),blue=empty?74:unsigned(ones.rgba[2]);
    const auto backend=native_hud_backend();auto* c=backend.context;
    backend.prepare_hud(c);backend.blend(c,1,4,5,0);backend.vertex_format(c,0,9,1,4,0);
    for(unsigned i=0;i<count;++i){
        backend.begin(c,0x98,0,4);
        for(const auto& p:quads[i].p){backend.position(c,p.x,p.y,0);backend.color(c,red,green,blue,unsigned(alpha));}
        backend.end(c,0x34);
    }
    backend.blend(c,1,1,0,0);backend.vertex_format(c,0,9,1,3,0);
}
void ammo_counter_install(){
    ammo_counter_native.sprite=reinterpret_cast<decltype(ammo_counter_native.sprite)>(main_base+0x28C8B0);
    ammo_counter_native.classify=reinterpret_cast<decltype(ammo_counter_native.classify)>(main_base+0x2970D0);
    ammo_counter_native.capacity=reinterpret_cast<decltype(ammo_counter_native.capacity)>(main_base+0x293C90);
}
