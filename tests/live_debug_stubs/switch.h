#pragma once
#include "../overlay_stubs/switch.h"
enum BsdServiceType {BsdServiceType_User=1};
struct SocketInitConfig {
    u32 tcp_tx_buf_size=0,tcp_rx_buf_size=0,tcp_tx_buf_max_size=0,tcp_rx_buf_max_size=0;
    u32 udp_tx_buf_size=0,udp_rx_buf_size=0,sb_efficiency=0,num_bsd_sessions=0;
    BsdServiceType bsd_service_type=BsdServiceType_User;
};
Result smInitialize();
void smExit();
Result socketInitialize(const SocketInitConfig*);
void socketExit();
