/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
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

/**************************************************
 *
 */
int main (int argc, char *argv[])
{
  int numNodes = 1;
  int staticDownlinkRate = 0;
  CommandLine cmd;
  cmd.AddValue ("numNodes", "Number of nodes/devices", numNodes);
  cmd.AddValue ("staticDownlinkRate", "Downlink data rate in kBps", staticDownlinkRate);
  cmd.Parse (argc,argv);
  std::cout << "NS3 NumNodes = " << numNodes << std::endl;
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));
  
  std::string TapBaseName = "sub";
  NodeContainer subscriberNodes;
  subscriberNodes.Create (numNodes);
  
  NodeContainer subscriberGatewayNodes;
  subscriberGatewayNodes.Create (numNodes+1);
  auto masterSubscriberGateway = subscriberGatewayNodes.Get(numNodes);
  
  NodeContainer nodes;
  nodes.Create(5);
  auto broker_gw1    = nodes.Get(0);
  auto broker        = nodes.Get(1);
  auto broker_gw2    = nodes.Get(2);
  auto publisher_gw  = nodes.Get(3);
  auto publisher     = nodes.Get(4);


  ////////////////////////////
  // Wifi
  ////////////////////////////
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  WifiMacHelper wifiMac;

  NetDeviceContainer subscriberNetDeviceContainer[numNodes];
  for (int i = 0; i < numNodes; i++) {
    std::string wifiName;
    wifiName = "wifi"+std::to_string(i+1);
    wifiPhy.SetChannel(wifiChannel.Create());
    //std::cout << wifiName << std::endl;
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(Ssid(wifiName)) );
    subscriberNetDeviceContainer[i] = wifi.Install(wifiPhy, wifiMac, NodeContainer(subscriberNodes.Get(i)));
    wifiMac.SetType ("ns3::StaWifiMac",
                    "Ssid", SsidValue (wifiName),
                    "ActiveProbing", BooleanValue (false));
    subscriberNetDeviceContainer[i].Add (wifi.Install (wifiPhy, wifiMac, NodeContainer (subscriberGatewayNodes.Get(i))));

  }
  
  MobilityHelper mobility;
  mobility.Install (NodeContainer(subscriberNodes,subscriberGatewayNodes));

  ////////////////////////////
  // Point-to-Point Links
  ////////////////////////////
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  p2p.SetChannelAttribute("Delay", StringValue("0ms"));
  NetDeviceContainer p2pLeft  = p2p.Install(NodeContainer(publisher_gw, broker_gw1));
  NetDeviceContainer p2pRight = p2p.Install(NodeContainer(masterSubscriberGateway, broker_gw2));

  NetDeviceContainer p2pSubscriberGatewayDevices[numNodes];
  std::string staticDownlinkRateKBps;
  for (int i = 0; i < numNodes; i++) {
    if (staticDownlinkRate != 0){
      staticDownlinkRateKBps = std::to_string(staticDownlinkRate)  + "KBps";
      //std::cout << staticDownlinkRate << std::endl;
      p2p.SetDeviceAttribute("DataRate", StringValue(staticDownlinkRateKBps));
    }
    p2pSubscriberGatewayDevices[i]  = p2p.Install(NodeContainer(subscriberGatewayNodes.Get(i),masterSubscriberGateway));
  }


  // wifiMac.SetType("ns3::StaWifiMac",
  //                 "Ssid", SsidValue(Ssid("left")),
  //                 "ActiveProbing", BooleanValue(false));
  // devicesLeft.Add(wifi.Install(wifiPhy, wifiMac, NodeContainer(subscriber,subscriber2)));

  ////////////////////////////
  // Left CSMA
  ////////////////////////////
  CsmaHelper csmaLeft;
  csmaLeft.SetChannelAttribute("DataRate", StringValue("10Mbps"));
  NetDeviceContainer devicesLeft =  csmaLeft.Install(NodeContainer(publisher,publisher_gw));
  // wifiMac.SetType("ns3::StaWifiMac",
  //                 "Ssid", SsidValue(Ssid("right")),
  //                 "ActiveProbing", BooleanValue(false));
  // devicesLeft.Add(wifi.Install(wifiPhy, wifiMac, publisher));

  // MobilityHelper mobility;
  // mobility.Install(NodeContainer(publisher,publisher_gw));

  ////////////////////////////
  // Middle CSMA
  ////////////////////////////
  CsmaHelper csmaMid;
  csmaMid.SetChannelAttribute("DataRate", StringValue("1Gbps"));
  NetDeviceContainer devicesMid = csmaMid.Install(NodeContainer(broker_gw1,broker_gw2,broker));

  ////////////////////////////
  // IP address assignment
  ////////////////////////////

  InternetStackHelper internet;
  internet.Install(NodeContainer(broker_gw1,broker_gw2,publisher_gw,subscriberGatewayNodes));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  ipv4.Assign(NetDeviceContainer(devicesMid.Get(0), devicesMid.Get(1)));
  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  ipv4.Assign(p2pRight);
  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  ipv4.Assign(NetDeviceContainer(devicesLeft.Get(1)));
  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  ipv4.Assign(p2pLeft);
      
  std::string subscriberAddress;
  std::string p2pSubscriberAddress;
  for (int i = 0; i < numNodes; i++) {
      // Set ip Subscriber-Gateways Network
      subscriberAddress = "10.3."+std::to_string(i)+".0";
      auto subscriberCharAddress = Ipv4Address(subscriberAddress.c_str());
      ipv4.SetBase(subscriberCharAddress, "255.255.255.0");
      ipv4.Assign(subscriberNetDeviceContainer[i].Get(1));

      // Set ip Subscriber Gateways-Master Network

      p2pSubscriberAddress = "10.2."+std::to_string(i)+".0";
      auto p2pSubscriberCharAddress = Ipv4Address(p2pSubscriberAddress.c_str());
      ipv4.SetBase(p2pSubscriberCharAddress, "255.255.255.0");
      ipv4.Assign(p2pSubscriberGatewayDevices[i]);
  }


  TapBridgeHelper tapBridge;
  tapBridge.SetAttribute ("Mode", StringValue("UseBridge"));
  tapBridge.SetAttribute ("DeviceName",StringValue("tap-pub" ));
  tapBridge.Install (publisher, devicesLeft.Get (0));
  tapBridge.SetAttribute ("DeviceName",StringValue("tap-mid" ));
  tapBridge.Install (broker, devicesMid.Get (2));

  for (int i = 0; i < numNodes; i++)
  {
      std::stringstream tapName;
      tapName << "tap-" << TapBaseName << (i+1) ;
      NS_LOG_UNCOND ("Tap bridge = " + tapName.str ());

      tapBridge.SetAttribute ("DeviceName", StringValue (tapName.str ()));
      tapBridge.Install (subscriberNodes.Get (i), subscriberNetDeviceContainer[i].Get (0));

  }


  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  std::cout << "*****check point *****" << std::endl;
  ListChannels();
  std::cout << std::endl;
  ListNodes();
  Simulator::Stop (Seconds (6000.));
  Simulator::Run ();
  

  Simulator::Destroy ();
}