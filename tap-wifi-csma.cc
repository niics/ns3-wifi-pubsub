/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Network topology
//
//  +----------+
//  | external |
//  |  Linux   |
//  |   Host   |
//  |          |
//  | "pubtap" |
//  +----------+
//       |           n0-1              n3                n4                 #n5             #n8-1
//       |       +--------+     +------------+     +------------+     +------------+     +--------+
//       +-------|  tap   |     |            |     |    tap     |     |            |     |        |
//               | bridge | ... |            |     |            |     |            | ... |        |
//               +--------+     +------------+     +------------+     +------------+     +--------+
//               |  Wifi  |     | Wifi |CSMA |-----|    CSMA    |-----|CSMA | Wifi |     |  Wifi  |
//               +--------+     +------+-----+     +------------+     +-----+------+     +--------+
//                   |              |     |              |               |     |              |     
//                 ((*))          ((*))   |              |               |   ((*))          ((*))
//                                        |              |               |
//                 ((*))          ((*))   |              |               |   ((*))          ((*))
//                   |              |     |              |               |     |              |
//                  n1             n2     ================================     n6            n7
//                     Wifi 10.1.2.x              CSMA LAN 10.1.1.x              Wifi 10.1.3.x
// 
// 
// *********************************************************************************************************************
// Network topology
//
//  +----------+
//  | external |
//  |  Linux   |
//  |   Host   |
//  |          |
//  | "mytap"  |
//  +----------+
//       |           n0-1              n3                n4                 #n5             #n6-1
//       |       +--------+     +------------+     +------------+     +------------+     +--------+
//       +-------|  tap   |     |            |     |            |     |            |     |        |
//               | bridge | ... |            |     |            |     |            | ... |        |
//               +--------+     +------------+     +------------+     +------------+     +--------+
//               |  Wifi  |     | Wifi | P2P |-----| P2P | CSMA |-----| P2P | Wifi |     |  Wifi  |
//               +--------+     +------+-----+     +-----+------+     +-----+------+     +--------+
//                   |              |           ^           |
//                 ((*))          ((*))         |           |
//                                          P2P 10.1.2      |
//                 ((*))          ((*))                     |    n5  n6   n7
//                   |              |                       |     |   |    |
//                  n1             n2                       ================
//                     Wifi 10.1.1                           CSMA LAN 10.1.3
//
// The Wifi device on node zero is:  10.1.1.1
// The Wifi device on node one is:   10.1.1.2
// The Wifi device on node two is:   10.1.1.3
// The Wifi device on node three is: 10.1.1.4
// The P2P device on node three is:  10.1.2.1
// The P2P device on node four is:   10.1.2.2
// The CSMA device on node four is:  10.1.3.1
// The CSMA device on node five is:  10.1.3.2
// The CSMA device on node six is:   10.1.3.3
// The CSMA device on node seven is: 10.1.3.4
//
// Some simple things to do:
//
// 1) Ping one of the simulated nodes on the left side of the topology.
//
//    ./waf --run tap-wifi-dumbbell&
//    ping 10.1.1.3
//
// 2) Configure a route in the linux host and ping once of the nodes on the 
//    right, across the point-to-point link.  You will see relatively large
//    delays due to CBR background traffic on the point-to-point (see next
//    item).
//
//    ./waf --run tap-wifi-dumbbell&
//    sudo route add -net 10.1.3.0 netmask 255.255.255.0 dev thetap gw 10.1.1.2
//    ping 10.1.3.4
//
//    Take a look at the pcap traces and note that the timing reflects the 
//    addition of the significant delay and low bandwidth configured on the 
//    point-to-point link along with the high traffic.
//
// 3) Fiddle with the background CBR traffic across the point-to-point 
//    link and watch the ping timing change.  The OnOffApplication "DataRate"
//    attribute defaults to 500kb/s and the "PacketSize" Attribute defaults
//    to 512.  The point-to-point "DataRate" is set to 512kb/s in the script,
//    so in the default case, the link is pretty full.  This should be 
//    reflected in large delays seen by ping.  You can crank down the CBR 
//    traffic data rate and watch the ping timing change dramatically.
//
//    ./waf --run "tap-wifi-dumbbell --ns3::OnOffApplication::DataRate=100kb/s"&
//    sudo route add -net 10.1.3.0 netmask 255.255.255.0 dev thetap gw 10.1.1.2
//    ping 10.1.3.4
//
// 4) Try to run this in UseBridge mode.  This allows you to bridge an ns-3
//    simulation to an existing pre-configured bridge.  This uses tap devices
//    just for illustration, you can create your own bridge if you want.
//
//    sudo tunctl -t mytap1
//    sudo ifconfig mytap1 0.0.0.0 promisc up
//    sudo tunctl -t mytap2
//    sudo ifconfig mytap2 0.0.0.0 promisc up
//    sudo brctl addbr mybridge
//    sudo brctl addif mybridge mytap1
//    sudo brctl addif mybridge mytap2
//    sudo ifconfig mybridge 10.1.1.5 netmask 255.255.255.0 up
//    ./waf --run "tap-wifi-dumbbell --mode=UseBridge --tapName=mytap2"&
//    ping 10.1.1.3

#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tap-bridge-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PubsTap-SubsTap");

int 
main (int argc, char *argv[])
{
  std::string mode = "ConfigureLocal";
  std::string tapName = "pubsubtap";
  
  std::string tapPubName = "tap-pub";
  std::string tapSubName = "tap-sub";

  CommandLine cmd;
  cmd.AddValue ("mode", "Mode setting of TapBridge", mode);
  cmd.AddValue ("tapName", "Name of the OS tap device", tapName);
  cmd.Parse (argc, argv);

  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  //
  // The topology has a Wifi network of four nodes on the pub side.  We'll make
  // node zero the AP and have the other three will be the STAs.
  //
  NodeContainer nodesPub;
  nodesPub.Create (4);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  Ssid ssid = Ssid ("pub");
  WifiHelper wifi;
  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));
  NetDeviceContainer devicesPub = wifi.Install (wifiPhy, wifiMac, nodesPub.Get (0));


  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));
  devicesPub.Add (wifi.Install (wifiPhy, wifiMac, NodeContainer (nodesPub.Get (1), nodesPub.Get (2), nodesPub.Get (3))));
  
  //
  // Set up Subscribe wifi
  //
  NodeContainer nodesSub;
  nodesSub.Create (4);

  // YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  // YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  // wifiPhy.SetChannel (wifiChannel.Create ());

  ssid = Ssid ("sub");

  // Ssid ssid = Ssid ("pub");
  // WifiHelper wifi;
  // WifiMacHelper wifiMac;
  // wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));
  NetDeviceContainer devicesPub = wifi.Install (wifiPhy, wifiMac, nodesSub.Get (0));


  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));
  devicesPub.Add (wifi.Install (wifiPhy, wifiMac, NodeContainer (nodesSub.Get (1), nodesSub.Get (2), nodesSub.Get (3))));

  //
  // Set IP for publisher side
  //

  MobilityHelper mobility;
  mobility.Install (nodesPub);

  InternetStackHelper internetPub;
  internetPub.Install (nodesPub);

  Ipv4AddressHelper ipv4Pub;
  ipv4Pub.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesPub = ipv4Pub.Assign (devicesPub);

  TapBridgeHelper tapBridge (interfacesPub.GetAddress (1));
  tapBridge.SetAttribute ("Mode", StringValue (mode));
  tapBridge.SetAttribute ("DeviceName", StringValue (tapPubName));
  tapBridge.Install (nodesPub.Get (0), devicesPub.Get (0));

  //
  // Set IP for subscriber side
  //

  // MobilityHelper mobility;
  mobility.Install (nodesSub);

  InternetStackHelper internetSub;
  internetSub.Install (nodesSub);

  Ipv4AddressHelper ipv4Sub;
  ipv4Sub.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesSub = ipv4Sub.Assign (devicesSub);

  TapBridgeHelper tapBridge (interfacesSub.GetAddress (1));
  tapBridge.SetAttribute ("Mode", StringValue (mode));
  tapBridge.SetAttribute ("DeviceName", StringValue (tapSubName));
  tapBridge.Install (nodesSub.Get (0), devicesSub.Get (0));

  //
  // Create mid node
  //
  NodeContainer nodesMid;
  nodesSub.Create (1);
  
  //
  // Now, create the csma node.
  //
  // NodeContainer nodesCsma;
  // nodesCsma.Create (4);
  
  NodeContainer nodesCsma = NodeContainer (nodesPub.Get (0), nodesMid.Get (0), nodesSub.Get (0));;

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (1000000));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer devicesCsma = csma.Install (nodesCsma);

  InternetStackHelper internetCsma;
  internetCsma.Install (nodesCsma);

  Ipv4AddressHelper ipv4Csma;
  ipv4Right.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesRight = ipv4Right.Assign (devicesCsma);

  //
  // Stick in the point-to-point line between the sides.
  //
  // PointToPointHelper p2p;
  // p2p.SetDeviceAttribute ("DataRate", StringValue ("512kbps"));
  // p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));
  // 
  // NodeContainer nodes = NodeContainer (nodesPub.Get (3), nodesRight.Get (0));
  // NetDeviceContainer devices = p2p.Install (nodes);
  // 
  // Ipv4AddressHelper ipv4;
  // ipv4.SetBase ("10.1.2.0", "255.255.255.192");
  // Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  //
  // Simulate some CBR traffic over the point-to-point link
  //
  uint16_t port = 9;   // Discard port (RFC 863)
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  onoff.SetConstantRate (DataRate ("500kb/s"));

  ApplicationContainer apps = onoff.Install (nodesPub.Get (3));
  apps.Start (Seconds (1.0));

  // Create a packet sink to receive these packets
  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));

  apps = sink.Install (nodesRight.Get (0));
  apps.Start (Seconds (1.0));

  wifiPhy.EnablePcapAll ("tap-wifi-dumbbell");

  csmaRight.EnablePcapAll ("tap-wifi-dumbbell", false);
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (60.));
  Simulator::Run ();
  Simulator::Destroy ();
}
