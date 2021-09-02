#pragma once

#include "of_message.h"

#undef OFP_ASSERT
#undef CONTAINER_OF
#undef ARRAY_SIZE
#undef ROUND_UP

#include "libfluid-base/OFServer.hh"
#include "libfluid-msg/of10msg.hh"
#include "libfluid-msg/of13msg.hh"

#include <arpa/inet.h>
#include <atomic>
#include <vector>
#include <chrono>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <pthread.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <set>
#include <stdlib.h>
#include <unordered_map>

using namespace fluid_base;
using namespace fluid_msg;

class OFController : public OFServer {
public:
    OFController(const std::unordered_map<uint64_t, std::string> switch_dpid_map,
                 const std::unordered_map<std::string, std::string> port_id_map,
                 const char* address = "0.0.0.0",
                 const int port = 1234,
                 const int nthreads = 4,
                 bool secure = false) :
            xid(0),
            switch_dpid_map(switch_dpid_map),
            port_id_map(port_id_map),
            OFServer(address, port, nthreads, secure,
                     OFServerSettings().supported_version(4) // OF version 0x04 is OF 1.3
                         .echo_interval(30)) { }

    ~OFController() = default;

    void stop() override;

    void message_callback(OFConnection* ofconn, uint8_t type, void* data, size_t len) override;

    void connection_callback(OFConnection* ofconn, OFConnection::Event type) override;

    OFConnection* get_instance(std::string bridge);

    void add_switch_to_conn_map(std::string bridge, int ofconn_id, OFConnection* ofconn);

    void remove_switch_from_conn_map(std::string bridge);

    void remove_switch_from_conn_map(int ofconn_id);

    void setup_default_br_int_flows();

    void setup_default_br_tun_flows();

    void execute_flow(const std::string br, const std::string flow_str, const std::string action = "add");

    void packet_out(const char* br, const char* opt);

private:
    // tracking xid (ovs transaction id)
    std::atomic<uint32_t> xid;

    // k is bridge name like 'br-int', v is OFConnection* obj
    std::unordered_map<std::string, OFConnection*> switch_conn_map;

    // k is ofconnection id like '0', v is bridge name associated with it
    std::unordered_map<int, std::string> switch_id_map;

    // k is dpid (query from ovs), v is bridge name associated with it
    std::unordered_map<uint64_t, std::string> switch_dpid_map;

    // k is port name like (patch-int/tun and vxlan-generic), v is ofport id of it from ovsdb
    std::unordered_map<std::string, std::string> port_id_map;

    std::mutex switch_map_mutex;

    void send_flow(OFConnection *ofconn, ofmsg_ptr_t &&p);

    void send_packet_out(OFConnection *ofconn, ofbuf_ptr_t &&po);

    void send_bundle_flow_mods(OFConnection *ofconn, std::vector<ofmsg_ptr_t> flow_mods);
};
