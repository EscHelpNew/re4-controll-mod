// Post-native attache hover-name capture, main4CFFCC. Registers are untouched.
// Native4CFA78..4CFAA4 resolves dragged/hovered node, then node+30 exact item.
// Native4DB6A8..4DB7AC passes (record+6 low nibble)+1 to293910, including
// native exclusive levels. Neither equipped-state upgrades nor base stats apply.
struct InventoryDamageNative {
    void* (*hovered)(void*,void*)=nullptr;
    void (*classify)(unsigned,void*)=nullptr;
    unsigned (*weapon)(unsigned)=nullptr;
    float (*power)(unsigned,int)=nullptr;
} inventory_damage_native;
struct InventoryDamageReadout {unsigned tenths=0;int x=0,y=0;};
std::atomic<bool> inventory_damage_enabled{false};
struct InventoryDamageCapture {
    InventoryDamageReadout value{};
    std::uintptr_t menu=0,slot=0;
    std::array<std::uint8_t,14> record{};
};
InventoryDamageCapture inventory_damage_capture;
std::atomic_flag inventory_damage_lock=ATOMIC_FLAG_INIT;
bool inventory_damage_region(std::uintptr_t address,std::size_t length){
    if(!address||!length||length>std::numeric_limits<std::uintptr_t>::max()-address)return false;
    MemoryInfo info{};std::uint32_t page=0;
    return !R_FAILED(svcQueryMemory(&info,&page,address))&&(info.perm&1)
        &&address>=info.addr&&address-info.addr<=info.size&&length<=info.size-(address-info.addr);
}
template<class T>bool inventory_damage_field(std::uintptr_t address,std::size_t offset,T& out){
    if(address>std::numeric_limits<std::uintptr_t>::max()-offset||!inventory_damage_region(address+offset,sizeof(T)))return false;
    std::memcpy(&out,reinterpret_cast<const void*>(address+offset),sizeof(T));return true;
}
void inventory_damage_clear(){
    if(inventory_damage_lock.test_and_set(std::memory_order_acquire))return;
    inventory_damage_capture={};inventory_damage_lock.clear(std::memory_order_release);
}
bool inventory_damage_slot(std::uintptr_t menu,std::uintptr_t& slot,std::array<std::uint8_t,14>& record){
    if(!inventory_damage_enabled.load(std::memory_order_acquire)||!main_base
        ||main_base>std::numeric_limits<std::uintptr_t>::max()-0x12712B0
        ||menu!=main_base+0x1270F70||!inventory_damage_native.hovered)return false;
    std::uintptr_t grid=0,node=0,cursor=0,slots=0;std::int32_t count=0;std::uint8_t owner=0;
    if(!inventory_damage_field(menu,0x338,grid)||!inventory_damage_region(grid,0x60)
        ||!inventory_damage_field(grid,0x20,node))return false;
    if(!node){
        if(!inventory_damage_field(grid,0x58,cursor)||!inventory_damage_region(cursor,0x50))return false;
        node=reinterpret_cast<std::uintptr_t>(inventory_damage_native.hovered(reinterpret_cast<void*>(grid),reinterpret_cast<void*>(cursor)));
    }
    const auto manager=main_base+0x124D918;
    if(!inventory_damage_field(node,0x30,slot)||!inventory_damage_field(manager,0x20,slots)
        ||!inventory_damage_field(manager,0x30,count)||count<1||count>384
        ||!inventory_damage_field(manager,0x1B,owner)||owner==1
        ||slot<slots||(slot-slots)%14||(slot-slots)/14>=std::size_t(count)
        ||!inventory_damage_region(slots,std::size_t(count)*14))return false;
    std::memcpy(record.data(),reinterpret_cast<const void*>(slot),record.size());
    std::uint16_t quantity=0;std::memcpy(&quantity,record.data()+2,2);
    return quantity&&(record[4]&1)&&record[5]==owner;
}
// Read at the UI draw boundary. Re-resolving the exact hover prevents cached
// damage surviving a move to empty cells, another duplicate, or a closed menu.
bool inventory_damage_snapshot(InventoryDamageReadout& out){
    if(inventory_damage_lock.test_and_set(std::memory_order_acquire))return false;
    const auto captured=inventory_damage_capture;inventory_damage_lock.clear(std::memory_order_release);
    std::uintptr_t slot=0;std::array<std::uint8_t,14> record{};std::uint32_t flags=0;
    //2B1750 activates text channel0 at engine+1C;2B1B50/2B0B80 clear bit0.
    //4CFAB8 closes that channel on an empty cell.4D019C nulls grid on exit.
    if(!captured.slot||!inventory_damage_field(main_base,0xB00648+0x1C,flags)||!(flags&1)
        ||!inventory_damage_slot(captured.menu,slot,record)
        ||slot!=captured.slot||record!=captured.record)return false;
    out=captured.value;return true;
}
HOOK_DEFINE_INLINE(InventoryDamageName){
    static void Callback(exl::hook::InlineFloatCtx* context){
        inventory_damage_clear();
        const auto& n=inventory_damage_native;
        if(!context||!n.classify||!n.weapon||!n.power)return;
        InventoryDamageCapture captured;captured.menu=re4::saved_x(context,20);
        if(!inventory_damage_slot(captured.menu,captured.slot,captured.record))return;
        std::uint16_t id=0;std::memcpy(&id,captured.record.data(),2);
        if(id>0x10F)return;
        std::array<std::uint8_t,6> type{};n.classify(id,type.data());
        if(type[2]!=1||n.weapon(id)>=255)return;
        const unsigned level=(captured.record[6]&15)+1;
        // Firepower table entries are seven floats; native special36/AB uses4.
        if(level>7||((id==0x36||id==0xAB)&&level>4))return;
        const float power=n.power(id,int(level))*extended_upgrade_display_ratio(captured.slot);
        if(!std::isfinite(power)||power<=0||power>6553.5f)return;
        captured.value.tenths=unsigned(power*10.f+.5f);
        captured.value.x=std::int32_t(std::uint32_t(re4::saved_x(context,22)));
        captured.value.y=std::int32_t(std::uint32_t(re4::saved_x(context,23)));
        if(captured.value.x<0||captured.value.x>2048||captured.value.y<0||captured.value.y>2048)return;
        std::uintptr_t slot=0;std::array<std::uint8_t,14> record{};
        if(!inventory_damage_slot(captured.menu,slot,record)||slot!=captured.slot||record!=captured.record)return;
        if(inventory_damage_lock.test_and_set(std::memory_order_acquire))return;
        inventory_damage_capture=captured;inventory_damage_lock.clear(std::memory_order_release);
    }
};
