// MIT License
// Copyright(c) 2020 Futurewei Cloud
//
//     Permission is hereby granted,
//     free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction,
//     including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons
//     to whom the Software is furnished to do so, subject to the following conditions:
//
//     The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
//     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//     FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//     WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef ACA_ON_DEMAND_ENGINE_H
#define ACA_ON_DEMAND_ENGINE_H

#define IPTOS_PREC_INTERNETCONTROL 0xc0
#define DSCP_DEFAULT (IPTOS_PREC_INTERNETCONTROL >> 2)
#define STDOUT_FILENO 1 /* Standard output.  */

#include "common.pb.h"
#include <openvswitch/ofp-errors.h>
#include <openvswitch/ofp-packet.h>
#include <string>
#include <thread>
#include "hashmap/HashMap.h"
#include <grpcpp/grpcpp.h>
#include <unordered_map>
#include "aca_log.h"
#include "goalstateprovisioner.grpc.pb.h"
#include "ctpl/ctpl_stl.h"

using namespace alcor::schema;
using namespace std;
using namespace ctpl;

extern int thread_pools_size;

// using namespace grpc;
struct on_demand_payload {
  std::chrono::_V2::steady_clock::time_point insert_time;
  string uuid;
  uint32_t in_port;
  void *packet;
  int packet_size;
  alcor::schema::Protocol protocol;
};
// ACA on-demand engine implementation class
namespace aca_on_demand_engine
{
class ACA_On_Demand_Engine {
  public:
  /* This thread is responsible for processing hostOperationReplies from NCM */
  std::thread *on_demand_reply_processing_thread;
  /* 
  This thread checks request_uuid_on_demand_payload_map periodically and removes any 
  entry that has been staying in the map for more than ON_DEMAND_ENTRY_CLEANUP_FREQUENCY_IN_MICROSECONDS
  */
  std::thread *on_demand_payload_cleaning_thread;
  grpc::CompletionQueue _cq;

  /*
      If there is a good way to iterate thru the CTSL::HashMap,
      we should change this request_uuid_on_demand_payload_map 
      back to this HashMap. Using unordered_map and locking it 
      with a mutex will prabably bring performance issues in the future.
  */
  unordered_map<std::string, on_demand_payload *, std::hash<std::string> > request_uuid_on_demand_payload_map;
  std::mutex _payload_map_mutex;
  /* This records when clean_remaining_payload() ran last time, 
  its initial value should be the time  when clean_remaining_payload() was first called*/
  std::chrono::_V2::steady_clock::time_point last_time_cleaned_remaining_payload;

  ctpl::thread_pool thread_pool_;

  static ACA_On_Demand_Engine &get_instance();

  /*
   * parse a received packet.
   * Input:
   *    uint32 in_port: the port received the packet
   *    void *packet: packet data.
   * example:
   *    ACA_ON_Demand_Engine::get_instance().parse_packet(1, packet) 
   */
  void parse_packet(uint32_t in_port, void *packet);

  void clean_remaining_payload();
  /*
   * print out the contents of packet payload data.
   * Input:
   *    const u_char *payload: payload data
   *    int len: payload size.
   * example:
   *    Aca_On_Demand_Engine::get_instance().print_payload(packet, len)
   */
  void print_payload(const u_char *payload, int len);
  void print_hex_ascii_line(const u_char *payload, int len, int offset);
  void on_demand(string uuid_for_call, OperationStatus status, uint32_t in_port,
                 void *packet, int packet_size, Protocol protocol,
                 std::chrono::_V2::steady_clock::time_point insert_time);
  void unknown_recv(uint16_t vlan_id, string ip_src, string ip_dest, int port_src,
                    int port_dest, Protocol protocol, char *uuid_str);
  void process_async_grpc_replies();
  void process_async_replies_asyncly(string request_id, OperationStatus replyStatus,
                                     std::chrono::_V2::high_resolution_clock::time_point received_ncm_reply_time);
/* ethernet headers are always exactly 14 bytes [1] */
#define SIZE_ETHERNET 14

  /* IP header */
  struct sniff_ip {
    u_char ip_vhl; /* version << 4 | header length >> 2 */
    u_char ip_tos; /* type of service */
    u_short ip_len; /* total length */
    u_short ip_id; /* identification */
    u_short ip_off; /* fragment offset field */
#define IP_RF 0x8000 /* reserved fragment flag */
#define IP_DF 0x4000 /* dont fragment flag */
#define IP_MF 0x2000 /* more fragments flag */
#define IP_OFFMASK 0x1fff /* mask for fragmenting bits */
    u_char ip_ttl; /* time to live */
    u_char ip_p; /* protocol */
    u_short ip_sum; /* checksum */
    struct in_addr ip_src, ip_dst; /* source and dest address */
  };
/* The second-half of the first byte in ip_header contains the IP header length (IHL). */
#define IP_HL(ip) (((ip)->ip_vhl) & 0x0f)
/* The IHL is number of 32-bit segments. Multiply by four to get a byte count for pointer arithmetic */
#define IP_V(ip) (((ip)->ip_vhl) >> 4)

  /* TCP header */
  typedef u_int tcp_seq;

  struct sniff_tcp {
    u_short th_sport; /* source port */
    u_short th_dport; /* destination port */
    tcp_seq th_seq; /* sequence number */
    tcp_seq th_ack; /* acknowledgement number */
    u_char th_offx2; /* data offset, rsvd */
#define TH_OFF(th) (((th)->th_offx2 & 0xf0) >> 4)
    u_char th_flags;
#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20
#define TH_ECE 0x40
#define TH_CWR 0x80
#define TH_FLAGS (TH_FIN | TH_SYN | TH_RST | TH_ACK | TH_URG | TH_ECE | TH_CWR)
    u_short th_win; /* window */
    u_short th_sum; /* checksum */
    u_short th_urp; /* urgent pointer */
  };

  /* UDP protocol header. */
  struct sniff_udp {
    u_short uh_sport; /* source port */
    u_short uh_dport; /* destination port */
    u_short uh_ulen; /* udp length */
    u_short uh_sum; /* udp checksum */
  };

  // compiler will flag the error when below is called.
  ACA_On_Demand_Engine(ACA_On_Demand_Engine const &) = delete;
  void operator=(ACA_On_Demand_Engine const &) = delete;

  private:
  ACA_On_Demand_Engine()
  {
    ACA_LOG_DEBUG("%s\n", "Constructor of a new on demand engine, need to create a new thread to process the grpc replies");
    int cores = std::thread::hardware_concurrency();
    ACA_LOG_DEBUG("This host has %ld cores, setting the size of the thread pools to be %ld\n",
                  cores, thread_pools_size);
    on_demand_reply_processing_thread = new std::thread(
            std::bind(&ACA_On_Demand_Engine::process_async_grpc_replies, this));

    on_demand_reply_processing_thread->detach();
    on_demand_payload_cleaning_thread = new std::thread(
            std::bind(&ACA_On_Demand_Engine::clean_remaining_payload, this));
    on_demand_payload_cleaning_thread->detach();
    thread_pool_.resize(thread_pools_size);
  };
  ~ACA_On_Demand_Engine()
  {
    _cq.Shutdown();
    request_uuid_on_demand_payload_map.clear();
    delete on_demand_reply_processing_thread;
    delete on_demand_payload_cleaning_thread;
    thread_pool_.stop();
  };
};
} // namespace aca_on_demand_engine
#endif // #ifndef ACA_ON_DEMAND_ENGINE_H