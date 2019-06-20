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
//  +----------+                                                                                    +----------+
//  | external |                                                                                    | external |
//  |  Linux   |                                                                                    |  Linux   |
//  |   Host   |                                                                                    |   Host   |
//  |          |                                                                                    |          |
//  |"tap-pub" |                                                                                    |"tap-sub1"|
//  +----------+                                                                                    +----------+
//       |           n2-1            n2-0               n1                 n3-0             n3-1          |
//       |       +--------+     +------------+     +------------+     +------------+     +--------+       |
//       +-------|  tap   |     |            |     |    tap     |     |            |     |        |-------+
//               | bridge | ... |            |     |            |     |            | ... |        |
//               +--------+     +------------+     +------------+     +------------+     +--------+
//               |  Wifi  |     | Wifi |CSMA |-----|    CSMA    |-----|CSMA | Wifi |     |  Wifi  |
//               +--------+     +------+-----+     +------------+     +-----+------+     +--------+
//                   |              |     |              |               |     |              |     
//                 ((*))          ((*))   |              |               |   ((*))          ((*))
//                                        |              |               |
//                 ((*))          ((*))   |              |               |   ((*))          ((*))
//                   |              |     |              |               |     |              |
//                  n2-2           n2-3   ================================   n3-2            n3-3
//                     Wifi 10.1.2.x              CSMA LAN 10.1.1.x              Wifi 10.1.3.x
// 
// The Wifi device on node 2-1 is:  10.1.2.1
// The Wifi device on node 2-2 is:  10.1.2.2
// The Wifi device on node 2-3 is:  10.1.2.3
//
// The Wifi device on node 2-0 is:  10.1.2.4
// The CSMA device on node 2-0 is:  10.1.1.1
//
// The CSMA device on node 1   is:  10.1.1.2
//
// The CSMA device on node 3-0 is:  10.1.1.3
// The Wifi device on node 3-0 is:  10.1.3.4   
//
// The Wifi device on node 3-1 is:  10.1.3.1
// The Wifi device on node 3-2 is:  10.1.3.2
// The Wifi device on node 3-3 is:  10.1.3.3
// 
// *********************************************************************************************************************

#include <iostream>
#include <fstream>

#include <iomanip>
#include <sstream>
#include <string>

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

NS_LOG_COMPONENT_DEFINE ("PubSubNetwork");

/**************************************************
 *
 */
std::string
ChannelType(Ptr<Channel> channel) 
{
  if (channel->GetObject<PointToPointChannel>())
    return "P2P"; 
  if (channel->GetObject<CsmaChannel>())
    return "CSMA"; 
  if (channel->GetObject<YansWifiChannel>())
    return "WiFi"; 
  return "Unknown";
}

/**************************************************
 *
 */
std::string
DeviceIpv4Info(Ptr<NetDevice> device) 
{
  std::ostringstream os;
  Ptr<Node> node = device->GetNode();
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();

  if (ipv4 != nullptr) 
  {
    int32_t ifno = ipv4->GetInterfaceForDevice(device);
    int32_t num_addr = ipv4->GetNAddresses(ifno);
    if (num_addr > 0) 
    {
      Ipv4InterfaceAddress addr = ipv4->GetAddress(ifno,0);
      os << "#IP:" << num_addr
         << " | 1st-IP:" << addr.GetLocal();
    }
    else 
      os << "#IP:0";
  }
  else 
    os << "(IP STACK NOT INSTALLED)";
  return os.str();
}

/**************************************************
 *
 */
void
ListChannels() 
{
  std::cout << "Channel List" << std::endl
            << "============"
            << std::endl;
  for (uint16_t i = 0; i < ChannelList::GetNChannels(); i++) 
  {
    Ptr<Channel> channel = ChannelList::GetChannel(i);
    std::cout << "Channel " << i
              << " (" << ChannelType(channel) << ")"
              << " has " << channel->GetNDevices() 
              << " device(s) attached" << std::endl;
    for (uint16_t j = 0; j < channel->GetNDevices(); j++) 
    {
      Ptr<NetDevice> device = channel->GetDevice(j);
      Ptr<Node> node = device->GetNode();
      std::cout << "- node:" << node->GetId()
                << " | device:" << device->GetIfIndex()
                << " | " << DeviceIpv4Info(device)
                << std::endl;
    }
  }
}

/**************************************************
 *
 */
void
ListNodes() 
{
  std::cout << "Node List" << std::endl
            << "========="
            << std::endl;
  for (uint16_t i = 0; i < NodeList::GetNNodes(); i++) 
  {
    Ptr<Node> node = NodeList::GetNode(i);
    int32_t id = node->GetId();
    std::cout << "Node " << id << std::endl;
    for (uint16_t j = 0; j < node->GetNDevices(); j++) 
    {
      Ptr<NetDevice> device = node->GetDevice(j);
      Ptr<Channel> channel = device->GetChannel();
      std::ostringstream os;
      if (channel)
        os << channel->GetId() << " (" << ChannelType(channel) << ")";
      else
        os << "none";

      std::cout 
        << "- device:" << device->GetIfIndex()
        << " | channel:" << std::setw(10) << std::left << os.str()
        << " | " << DeviceIpv4Info(device)
        << std::endl;
    }
  }
}

int 
main (int argc, char *argv[])
{
  std::string mode = "ConfigureLocal";
  std::string tapName = "pubsubtap";
  
  std::string tapPubName = "tap-pub";
  std::string tapSubName = "tap-sub";
  std::string tapMidName = "tap-mid";

  CommandLine cmd;
  cmd.AddValue ("mode", "Mode setting of TapBridge", mode);
  cmd.AddValue ("tapName", "Name of the OS tap device", tapName);
  cmd.Parse (argc, argv);

  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  //  Define numbers of nodes
  int noOfPub = 4;
  int noOfSub = 4;

  //
  //  Define node container
  //

  NodeContainer nodesMid;
  nodesMid.Create (3);
  auto midLeft = nodesMid.Get (0);
  auto midDev = nodesMid.Get (1);
  auto midRight = nodesMid.Get (2);

  NodeContainer nodesPub;
  nodesPub.Create (noOfPub);
  auto nodePubAP = nodesPub.Get (0);
  auto nodePubTap = nodesPub.Get (1);

  NodeContainer nodesSub;
  nodesSub.Create (noOfSub);
  auto nodeSubAP = nodesSub.Get (0);
  auto nodeSubTap = nodesSub.Get (1);

  //
  // Set up Publisher wifi
  //
  YansWifiPhyHelper wifiPubPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiPubChannel = YansWifiChannelHelper::Default ();
  wifiPubPhy.SetChannel (wifiPubChannel.Create ());

  Ssid ssidPub = Ssid ("pub-wifi");
  WifiHelper wifiPub;
  WifiMacHelper wifiPubMac;
  wifiPub.SetRemoteStationManager ("ns3::ArfWifiManager");

  // std::cout << ssidPub << std::endl;
  wifiPubMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue (ssidPub));
  NetDeviceContainer pubNetContainer = wifiPub.Install (wifiPubPhy, wifiPubMac, nodePubAP);

  wifiPubMac.SetType ("ns3::StaWifiMac",
                    "Ssid", SsidValue (ssidPub),
                    "ActiveProbing", BooleanValue (false));
  for (int i=1; i<noOfPub; i++) {
    pubNetContainer.Add (wifiPub.Install (wifiPubPhy, wifiPubMac, NodeContainer (nodesPub.Get(i))));
  }

  //
  // Set up Subscriber wifi
  //
  // NetDeviceContainer subNetContainer;

  YansWifiPhyHelper wifiSubPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiSubChannel = YansWifiChannelHelper::Default ();
  wifiSubPhy.SetChannel (wifiSubChannel.Create ());

  Ssid ssidSub = Ssid ("sub-wifi");
  WifiHelper wifiSub;
  WifiMacHelper wifiSubMac;
  wifiSub.SetRemoteStationManager ("ns3::ArfWifiManager");

  // std::cout << ssidPub << std::endl;
  wifiSubMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue (ssidSub));
  NetDeviceContainer subNetContainer = wifiSub.Install (wifiSubPhy, wifiSubMac, nodeSubAP);

  wifiPubMac.SetType ("ns3::StaWifiMac",
                  "Ssid", SsidValue (ssidPub),
                  "ActiveProbing", BooleanValue (false));
  for (int i=1; i<noOfSub; i++) {
    subNetContainer.Add (wifiSub.Install (wifiSubPhy, wifiSubMac, NodeContainer (nodesSub.Get(i))));
  }
  
  //
  // Middle CSMA
  //
  CsmaHelper csmaMid;
  csmaMid.SetChannelAttribute ("DataRate", StringValue("1Gbps"));
  csmaMid.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer devicesMid = csmaMid.Install(nodesMid);

  //
  // Mobility
  //

  MobilityHelper mobility;

  mobility.Install (nodesPub);
  mobility.Install (nodesSub);

  InternetStackHelper internetMid;
  internetMid.Install (nodesMid);

  InternetStackHelper internetPub;
  internetPub.Install (nodesPub);

  InternetStackHelper internetSub;
  internetSub.Install (nodesSub);

  // 
  // Wifi's IP assigns
  // 
  Ipv4AddressHelper ipv4Pub;
  ipv4Pub.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesPub = ipv4Pub.Assign (pubNetContainer);

  Ipv4AddressHelper ipv4Sub;
  ipv4Sub.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesSub = ipv4Sub.Assign (subNetContainer);

  // 
  // Point to Point links
  // 
  PointToPointHelper p2pLeft;
  p2pLeft.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  p2pLeft.SetChannelAttribute("Delay", StringValue("0ms"));
  NetDeviceContainer devicesLeft  = p2pLeft.Install(NodeContainer(nodePubAP, midLeft));

  PointToPointHelper p2pRight;
  p2pRight.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  p2pRight.SetChannelAttribute("Delay", StringValue("0ms"));
  NetDeviceContainer devicesRight = p2pRight.Install(NodeContainer(nodeSubAP, midRight));

  // 
  // P2P's IP assigns
  // 
  Ipv4AddressHelper ipv4Left;
  ipv4Left.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesLeft = ipv4Left.Assign (devicesLeft);

  Ipv4AddressHelper ipv4Right;
  ipv4Right.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesRight = ipv4Right.Assign (devicesRight);

  // 
  // CSMA(mid)'s IP assigns
  // 
  Ipv4AddressHelper ipv4Mid;
  ipv4Mid.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesMid = ipv4Mid.Assign (devicesMid);

  //
  //  Set-up tap bridge
  //
  TapBridgeHelper tapBridge (interfacesPub.GetAddress (1));
  tapBridge.SetAttribute ("Mode", StringValue (mode));
  tapBridge.SetAttribute ("DeviceName", StringValue (tapPubName));
  tapBridge.Install (nodePubTap, pubNetContainer.Get (1));

  TapBridgeHelper tapBridgeMid (interfacesMid.GetAddress (1));
  tapBridgeMid.SetAttribute ("Mode", StringValue (mode));
  tapBridgeMid.SetAttribute ("DeviceName",StringValue(tapMidName ));
  tapBridgeMid.Install (nodesMid.Get (1), devicesMid.Get (1));

  // TapBridgeHelper tapBridge;
  // tapBridge.SetAttribute ("Mode", StringValue("UseBridge"));
  // tapBridge.SetAttribute ("DeviceName",StringValue(tapPubName ));
  // tapBridge.Install (nodePubTap, pubNetContainer.Get (1));
  // tapBridge.SetAttribute ("DeviceName",StringValue(tapSubName ));
  // tapBridge.Install (nodeSubTap, subNetContainer.Get (1));
  // tapBridge.SetAttribute ("DeviceName",StringValue(tapMidName ));
  // tapBridge.Install (nodesMid.Get (1), devicesMid.Get (1));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  std::cout << "*****check point *****" << std::endl;
  ListChannels();
  std::cout << std::endl;
  // ListNodes();

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (6000.));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
