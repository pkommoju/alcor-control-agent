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

#include "aca_log.h"
#include "aca_util.h"
#include "aca_config.h"
#include "aca_net_config.h"
#include <errno.h>

using namespace std;

extern std::atomic_ulong g_total_execute_system_time;
extern bool g_demo_mode;

namespace aca_net_config
{
Aca_Net_Config &Aca_Net_Config::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static Aca_Net_Config instance;
  return instance;
}

int Aca_Net_Config::create_namespace(string ns_name, ulong &culminative_time)
{
  int rc;

  if (ns_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty namespace, rc: %d\n", rc);
    return rc;
  }

  string cmd_string = IP_NETNS_PREFIX + "add " + ns_name;

  return execute_system_command(cmd_string, culminative_time);
}

// caller needs to ensure the device name is 15 characters or less
// due to linux limit
int Aca_Net_Config::create_veth_pair(string veth_name, string peer_name, ulong &culminative_time)
{
  int rc;

  if (veth_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty veth_name, rc: %d\n", rc);
    return rc;
  }

  if (peer_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty peer_name, rc: %d\n", rc);
    return rc;
  }

  string cmd_string = "ip link add " + veth_name + " type veth peer name " + peer_name;

  return execute_system_command(cmd_string, culminative_time);
}

int Aca_Net_Config::delete_veth_pair(string peer_name, ulong &culminative_time)
{
  int rc;

  if (peer_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty peer_name, rc: %d\n", rc);
    return rc;
  }

  string cmd_string = "ip link delete " + peer_name;

  return execute_system_command(cmd_string, culminative_time);
}

int Aca_Net_Config::setup_peer_device(string peer_name, ulong &culminative_time)
{
  int rc;

  if (peer_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty peer_name, rc: %d\n", rc);
    return rc;
  }

  string cmd_string =
          "ip link set dev " + peer_name + " up mtu " + std::to_string(DEFAULT_MTU);

  return execute_system_command(cmd_string, culminative_time);
}

int Aca_Net_Config::move_to_namespace(string veth_name, string ns_name, ulong &culminative_time)
{
  int rc;

  if (veth_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty veth_name, rc: %d\n", rc);
    return rc;
  }

  if (ns_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty ns_name, rc: %d\n", rc);
    return rc;
  }

  string cmd_string = "ip link set " + veth_name + " netns " + ns_name;

  return execute_system_command(cmd_string, culminative_time);
}

int Aca_Net_Config::setup_veth_device(string ns_name, veth_config new_veth_config,
                                      ulong &culminative_time)
{
  int overall_rc = EXIT_SUCCESS;
  int command_rc;
  string cmd_string;

  if (ns_name.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty ns_name, rc: %d\n", overall_rc);
    return overall_rc;
  }

  if (new_veth_config.veth_name.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty new_veth_config.veth_name, rc: %d\n", overall_rc);
    return overall_rc;
  }

  if (new_veth_config.ip.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty new_veth_config.ip, rc: %d\n", overall_rc);
    return overall_rc;
  }

  if (new_veth_config.prefix_len.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty new_veth_config.prefix_len, rc: %d\n", overall_rc);
    return overall_rc;
  }

  if (new_veth_config.mac.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty new_veth_config.mac, rc: %d\n", overall_rc);
    return overall_rc;
  }

  if (new_veth_config.gateway_ip.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty new_veth_config.gateway_ip, rc: %d\n", overall_rc);
    return overall_rc;
  }

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ip addr add " +
               new_veth_config.ip + "/" + new_veth_config.prefix_len + " dev " +
               new_veth_config.veth_name;
  command_rc = execute_system_command(cmd_string, culminative_time);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ip link set dev " +
               new_veth_config.veth_name + " up";
  command_rc = execute_system_command(cmd_string, culminative_time);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " route add default gw " +
               new_veth_config.gateway_ip;
  command_rc = execute_system_command(cmd_string, culminative_time);
  // it is okay if the gateway is already setup
  //   if (command_rc != EXIT_SUCCESS)
  //     overall_rc = command_rc;

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ifconfig " +
               new_veth_config.veth_name + " hw ether " + new_veth_config.mac;
  command_rc = execute_system_command(cmd_string, culminative_time);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  if (g_demo_mode) {
    cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " sysctl -w net.ipv4.tcp_mtu_probing=2";
    command_rc = execute_system_command(cmd_string, culminative_time);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;

    cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ethtool -K " +
                 new_veth_config.veth_name + " tso off gso off ufo off";
    command_rc = execute_system_command(cmd_string, culminative_time);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;

    cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ethtool --offload " +
                 new_veth_config.veth_name + " rx off tx off";
    command_rc = execute_system_command(cmd_string, culminative_time);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;

    cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ifconfig lo up";
    command_rc = execute_system_command(cmd_string, culminative_time);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;
  }

  return overall_rc;
}

// this functions bring the linux device down for the rename,
// and then bring it back up
int Aca_Net_Config::rename_veth_device(string ns_name, string org_veth_name,
                                       string new_veth_name, ulong &culminative_time)
{
  int overall_rc = EXIT_SUCCESS;
  int command_rc;
  string cmd_string;

  if (ns_name.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty ns_name, rc: %d\n", overall_rc);
    return overall_rc;
  }

  if (org_veth_name.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty org_veth_name, rc: %d\n", overall_rc);
    return overall_rc;
  }

  if (new_veth_name.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty new_veth_name, rc: %d\n", overall_rc);
    return overall_rc;
  }

  // bring the link down
  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ip link set dev " +
               org_veth_name + " down";
  command_rc = execute_system_command(cmd_string, culminative_time);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ip link set " +
               org_veth_name + " name " + new_veth_name;
  command_rc = execute_system_command(cmd_string, culminative_time);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  // bring the device back up
  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ip link set dev " +
               new_veth_name + " up";
  command_rc = execute_system_command(cmd_string, culminative_time);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  return overall_rc;
}

// workaround to add the gateway information based on the current CNI contract
// to be removed with the final contract/design
int Aca_Net_Config::add_gw(string ns_name, string gateway_ip, ulong &culminative_time)
{
  int rc;
  string cmd_string;

  if (ns_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty ns_name, rc: %d\n", rc);
    return rc;
  }

  if (gateway_ip.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty gateway_ip, rc: %d\n", rc);
    return rc;
  }

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " route add default gw " + gateway_ip;
  rc = execute_system_command(cmd_string, culminative_time);

  return rc;
}

int Aca_Net_Config::execute_system_command(string cmd_string)
{
  ulong not_care;

  return execute_system_command(cmd_string, not_care);
}

int Aca_Net_Config::execute_system_command(string cmd_string, ulong &culminative_time)
{
  int rc;

  if (cmd_string.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty cmd_string, rc: %d\n", rc);
    return rc;
  }

  ACA_LOG_INFO("Executing command: %s\n", cmd_string.c_str());

  auto execute_system_time_start = chrono::steady_clock::now();

  rc = system(cmd_string.c_str());

  auto execute_system_time_end = chrono::steady_clock::now();

  auto execute_system_elapse_time =
          cast_to_microseconds(execute_system_time_end - execute_system_time_start)
                  .count();

  culminative_time += execute_system_elapse_time;

  g_total_execute_system_time += execute_system_elapse_time;

  if (rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Command succeeded!\n");
  } else {
    ACA_LOG_DEBUG("Command failed!!! rc: %d\n", rc);
  }

  ACA_LOG_DEBUG(" Elapsed time for system command [%s] took: %ld microseconds or %ld milliseconds.\n",
                cmd_string.c_str(), execute_system_elapse_time,
                us_to_ms(execute_system_elapse_time));

  return rc;
}

} // namespace aca_net_config
