
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <regex>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

#include "ns3/netanim-module.h"

#include "ns3/imnHelper.h" //custom created file to parse imn

//thinds from namespace std
using std::cout;
using std::endl;
using std::cerr;
using std::string;
using std::vector;

// settings
#define SIMULATION_RUNTIME 1600
#define MINIMUM_FREQUENCY  "1"
#define CHUNK_SIZE         1436
#define WIFI_RSS_DBM       -80
#define WIFI_PHY_MODE      "DsssRate11Mbps"
#define DEFAULT_P2P_RATE   "1000Mbps"
#define DEFAULT_P2P_DELAY  "1ms"
#define DEBUG_OUTPUT       0


using namespace ns3;
//trying to parse an imn file and create an ns3 scenario file from it



int main (int argc, char *argv[]) {

	// config locals
  string topo_name = "/dev/null", 
              topo_cmd = "";
              //trace_prefix = "/dev/null",
              //content_size = "0",
              
  std::ifstream topo_stream;		//imn file


  //struct stat st;
  //bool did_num = false, did_sta = false, did_con = false, did_pro = false;

  // simulation locals
  NodeContainer nodes;
  ApplicationContainer apps;
  //PointToPointHelper p2p;
  //NetDeviceContainer p2pDevices;
  WifiHelper wifi = WifiHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  //InternetStackHelper InternetHelper;
  //GlobalRoutingHelper routingHelper;
  //AppHelper consumerHelper ("ns3::ndn::ConsumerTest");
  //AppHelper producerHelper ("ns3::ndn::Producer");
  //NetDeviceContainer wifi_devices;

  // setting default parameters for links and channels
  //Config::SetDefault ("ns3::PointToPointNetDevice::DataRate", StringValue (DEFAULT_P2P_RATE));
  //Config::SetDefault ("ns3::PointToPointNetDevice::Mtu", UintegerValue(1500));
  //Config::SetDefault ("ns3::PointToPointChannel::Delay", StringValue (DEFAULT_P2P_DELAY));
  //Config::SetDefault ("ns3::DropTailQueue::MaxPackets", StringValue ("64"));
  //Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  //Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  //Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (WIFI_PHY_MODE));
  
  // set up wifi stuff
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
  wifiPhy.Set ("RxGain", DoubleValue (0) ); 
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue (WIFI_RSS_DBM));
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
  							   "DataMode",    StringValue(WIFI_PHY_MODE),
  							   "ControlMode", StringValue(WIFI_PHY_MODE));

  // read command-line parameters
  CommandLine cmd;
  cmd.AddValue("topo", "Path to intermediate topology file", topo_name);
  //cmd.AddValue("trace","Directory in which to store trace files", trace_prefix);
  //cmd.AddValue("csize","Size per content in MB", content_size);
  //cmd.AddValue("cache","Size of NDN caches in MB", cache_size);
  cmd.Parse (argc, argv);
  
  imnHelper imn_container(topo_name.c_str()); //holds entire imn file in a list of node and list link containers

  nodes.Create(imn_container.total);

cout << "\nCreating " << imn_container.total << " nodes" << endl;

  //CsmaHelper csma;
  //NetDeviceContainer csmaDevices;
  //NetDeviceContainer switchDevices;
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");

  regex number("[0-9]+");
  regex addr("[0-9]+[.]{0,1}[0-9]+[.]{0,1}[0-9]+");
  smatch r_match;
  int last_ap_id = -1;
  int n1 = 0, n2 = 0;
  Ssid ssid = Ssid("no-network");
  string peer, peer2, type, addr_str;

  for(int i = 0; i < imn_container.imn_links.size(); i++){
    type = imn_container.imn_links.at(i).type;
    cout << endl;
    //=============P2P===============
    if(type.compare("p2p") == 0){
      n1 = 0;
      n2 = 0;
      NodeContainer p2pNodes;
      NetDeviceContainer p2pDevices;
      PointToPointHelper p2p;

      peer = imn_container.imn_links.at(i).peer_list.at(0);
      peer2 = imn_container.imn_links.at(i).peer_list.at(1);
      regex_search(peer, r_match, number);
      n1 = stoi(r_match[0]) - 1;
      regex_search(peer2, r_match, number);
      n2 = stoi(r_match[0]) - 1;

      p2pNodes.Add(nodes.Get(n1));
      p2pNodes.Add(nodes.Get(n2));


      if(imn_container.imn_links.at(i).delay.empty() == 0){
        p2p.SetChannelAttribute("Delay",TimeValue(MicroSeconds(stoi(imn_container.imn_links.at(i).delay))));
      }
      if(imn_container.imn_links.at(i).bandwidth.empty() == 0){
        p2p.SetDeviceAttribute("DataRate", DataRateValue(stoi(imn_container.imn_links.at(i).bandwidth)));
      }

      p2pDevices.Add(p2p.Install(nodes.Get(n1), nodes.Get(n2)));

      //InternetStackHelper internetP2P;
      //internetP2P.Install(p2pNodes);

      regex_search(imn_container.imn_nodes.at(n1 + 1).interface_list.at(0).ipv4_addr, r_match, addr);
      addr_str.assign(r_match.str());
      addr_str.append(".0");
      char temp[14];
      strncpy(temp, addr_str.c_str(), sizeof(temp));
      temp[sizeof(temp) - 1] = 0;

      //Ipv4AddressHelper address;
      //address.SetBase(temp, "255.255.255.0");
      address.NewNetwork();
      Ipv4InterfaceContainer p2pInterface = address.Assign(p2pDevices);

      cout << "Creating point-to-point connection with n" << n1 + 1 << " and n" << n2 + 1 << endl;

      if(n1 == 0 || n2 == 0){
        cout << "P2P link could not be established with " << peer << " and " << peer2 << endl;
        return -1;
      }
    }//=============Wifi===============
    else if(type.compare("wlan") == 0){
      n1 = 0;
      n2 = 0;
      int total_peers = imn_container.imn_links.at(i).peer_list.size();
      peer = imn_container.imn_links.at(i).name;
      regex_search(peer,r_match,number);
      n1 = stoi(r_match[0]) - 1;
      NodeContainer wifiNodes;
      NetDeviceContainer wifiDevices;

      if(n1 != last_ap_id){
        stringstream ssid_creator;
        ssid_creator << "wifi-" << n1;
        ssid = Ssid(ssid_creator.str().c_str());

        wifiPhy.SetChannel(wifiChannel.Create());
        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        wifiDevices.Add(wifi.Install(wifiPhy, wifiMac, nodes.Get(n1)));
        mobility.Install(nodes.Get(n1));
        wifiNodes.Add(nodes.Get(n1));
        last_ap_id = n1;
        cout << "Creating new wifi network " << ssid << " at n" << n1 + 1 << endl;
      }

      for(int j = 0; j < total_peers; j++){
        peer2 = imn_container.imn_links.at(i).peer_list.at(j);
        regex_search(peer2,r_match,number);
        n2 = stoi(r_match[0]) - 1;

        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        wifiDevices.Add(wifi.Install(wifiPhy, wifiMac, nodes.Get(n2)));
        cout << "Adding node n" << n2 + 1 << " to " << ssid << " at n" << n1 + 1 << endl;
        mobility.Install(nodes.Get(n2));
        wifiNodes.Add(nodes.Get(n2));
      }

      //InternetStackHelper wifiInternet;
      //wifiInternet.Install(wifiNodes);

      regex_search(imn_container.imn_nodes.at(n1 + 1).interface_list.at(0).ipv4_addr, r_match, addr);
      addr_str.assign(r_match.str());
      addr_str.append(".0");
      char temp[14];
      strncpy(temp, addr_str.c_str(), sizeof(temp));
      temp[sizeof(temp) - 1] = 0;

      //Ipv4AddressHelper address;
      //address.SetBase(temp, "255.255.255.0");
      address.NewNetwork();
      Ipv4InterfaceContainer wifiInterface = address.Assign(wifiDevices);
    }//=============Hub===============
    else if(type.compare("hub") == 0){
      n1 = 0;
      n2 = 0;
      int total_peers = imn_container.imn_links.at(i).peer_list.size();
      int total_asso_links = imn_container.imn_links.at(i).extra_links.size();
      NodeContainer csmaNodes;
      NetDeviceContainer csmaDevices;

      peer = imn_container.imn_links.at(i).name;
      regex_search(peer,r_match,number);
      n1 = stoi(r_match[0]) - 1;
      csmaNodes.Add(nodes.Get(n1));
      //csma.Install(nodes.Get(n1));

      cout << "Creating new hub network named n" << n1 + 1 << endl;
      //for all peers using this hub
      for(int j = 0; j < total_peers; j++){
        peer2 = imn_container.imn_links.at(i).peer_list.at(j);
        regex_search(peer2,r_match,number);
        n2 = stoi(r_match[0]) - 1;
        NodeContainer csmaNodeSegment;
        CsmaHelper csma;
        csmaNodeSegment.Add(nodes.Get(n1));
        csmaNodeSegment.Add(nodes.Get(n2));
        csmaNodes.Add(nodes.Get(n2));
        //iterate through links to correctly match corresponding data 
        for(int k = 0; k < total_asso_links; k++){
          string peer2_check = imn_container.imn_links.at(i).extra_links.at(k).name;
          //if link info doesn't belong to current node, skip to next
          if(peer2.compare(peer2_check) != 0){
            continue;
          }
          if(imn_container.imn_links.at(i).extra_links.at(k).delay.empty() == 0){
            csma.SetChannelAttribute("Delay",TimeValue(MicroSeconds(stoi(imn_container.imn_links.at(i).extra_links.at(k).delay))));
          }
          if(imn_container.imn_links.at(i).extra_links.at(k).bandwidth.empty() == 0){
            csma.SetDeviceAttribute("DataRate", DataRateValue(stoi(imn_container.imn_links.at(i).extra_links.at(k).bandwidth)));
          }
        }

        csmaDevices.Add(csma.Install(csmaNodeSegment));

        cout << "Adding node n" << n2 + 1 << " to a csma(hub) n" << n1 + 1 << endl;
      }
      int x;
      for(x = 0; x < imn_container.imn_nodes.size(); x++){
        peer2 = imn_container.imn_nodes.at(x).name;
        regex_search(peer2,r_match,number);
        int compare_node = stoi(r_match[0]) - 1;
        if(n2 == compare_node){
          break;
        }
      }
   
      //InternetStackHelper internetCsma;
      //internetCsma.Install(csmaNodes);

      regex_search(imn_container.imn_nodes.at(x).interface_list.at(0).ipv4_addr, r_match, addr);
      addr_str.assign(r_match.str());
      addr_str.append(".0");
      char temp[14];
      strncpy(temp, addr_str.c_str(), sizeof(temp));
      temp[sizeof(temp) - 1] = 0;

      //Ipv4AddressHelper address;
      //address.SetBase(temp, "255.255.255.0");
      address.NewNetwork();
      Ipv4InterfaceContainer csmaInterface = address.Assign(csmaDevices);
    }//=============Switch===============
    else if(type.compare("landswitch") == 0){
     // add different setup for landswitches
      n1 = 0;
      n2 = 0;
      int total_peers = imn_container.imn_links.at(i).peer_list.size();
      int total_asso_links = imn_container.imn_links.at(i).extra_links.size();
      NetDeviceContainer csmaDevices;

      peer = imn_container.imn_links.at(i).name;
      regex_search(peer,r_match,number);
      n1 = stoi(r_match[0]) - 1;

      cout << "Creating new switch network " << " named n" << n1 + 1 << endl;

      for(int j = 0; j < total_peers; j++){
        int flag1 = 0, flag2 = 0;
        peer2 = imn_container.imn_links.at(i).peer_list.at(j);
        regex_search(peer2,r_match,number);
        n2 = stoi(r_match[0]) - 1;
        NodeContainer switchNodes;
        CsmaHelper csma;
        switchNodes.Add(nodes.Get(n1));
        switchNodes.Add(nodes.Get(n2));

        for(int k = 0; k < total_asso_links; k++){
          string peer2_check = imn_container.imn_links.at(i).extra_links.at(k).name;
          if(peer2.compare(peer2_check) != 0){
            continue;
          }
          if(imn_container.imn_links.at(i).extra_links.at(k).delay.empty() == 0){
            csma.SetChannelAttribute("Delay",TimeValue(MicroSeconds(stoi(imn_container.imn_links.at(i).extra_links.at(k).delay))));
          }
          if(imn_container.imn_links.at(i).extra_links.at(k).bandwidth.empty() == 0){
            csma.SetDeviceAttribute("DataRate", DataRateValue(stoi(imn_container.imn_links.at(i).extra_links.at(k).bandwidth)));
          }
        }
        csmaDevices.Add(csma.Install(switchNodes));

        //InternetStackHelper internetCsma;
        //internetCsma.Install(switchNodes);

        cout << "Adding node n" << n2 + 1 << " to a csma(switch) n" << n1 + 1 << endl;
      }

      regex_search(imn_container.imn_nodes.at(n1 + 1).interface_list.at(0).ipv4_addr, r_match, addr);
      addr_str.assign(r_match.str());
      addr_str.append(".0");
      char temp[14];
      strncpy(temp, addr_str.c_str(), sizeof(temp));
      temp[sizeof(temp) - 1] = 0;

      //Ipv4AddressHelper address;
      //address.SetBase(temp, "255.255.255.0");
      address.NewNetwork();
      Ipv4InterfaceContainer csmaInterface = address.Assign(csmaDevices);

    }
  }//end of for loop

  /*InternetHelper.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.1.0", "255.255.255.0");
  address.Assign(p2pDevices);

  address.SetBase("10.1.1.0", "255.255.255.0");
  address.Assign(wifi_devices);

  address.SetBase("10.2.1.0", "255.255.255.0");
  address.Assign(csmaDevices);
  */
  //
  //set router/pc/p2p/other coordinates for NetAnim
  //
  for(int i = 0; i < imn_container.imn_nodes.size() ; i++){
    int n = 0;
    regex_search(imn_container.imn_nodes.at(i).name,r_match,number);
    n = stoi(r_match[0]) - 1;

    AnimationInterface::SetConstantPosition(nodes.Get(n), imn_container.imn_nodes.at(i).coordinates.x, imn_container.imn_nodes.at(i).coordinates.y);
  }

  //
  //Create a packet sink on the hubs to recieve packets
  //
  uint16_t port = 50000;
  Address hubLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", hubLocalAddress);

  //
  //set hub/switch nodes coordinates
  //
  for(int i = 0; i < imn_container.imn_links.size() ; i++){
    int n = 0;
    if(imn_container.imn_links.at(i).type.compare("p2p") == 0){
      continue;
    }
    if(imn_container.imn_links.at(i).type.compare("wlan") == 0){
      continue;
    }
    regex_search(imn_container.imn_links.at(i).name,r_match,number);
    n = stoi(r_match[0]) - 1;

    ApplicationContainer hubApp = packetSinkHelper.Install (nodes.Get(n));
    hubApp.Start (Seconds (1.0));
    hubApp.Stop (Seconds (10.0));

    //
    // Create OnOff applications to send TCP to the hub, one on each spoke node.
    //
    OnOffHelper onOffHelper ("ns3::TcpSocketFactory", Address ());
    onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer spokeApps;
    int total_peers = imn_container.imn_links.at(i).peer_list.size();

    //ptr to hubs ipv4 address
    Ptr<Ipv4> ipv4 = nodes.Get(n)->GetObject<Ipv4>();
cout << n << endl;
    for (uint32_t j = 0; j < total_peers; j++){
      string temp_str;
      string peer2 = imn_container.imn_links.at(i).peer_list.at(j);
      regex_search(peer2, r_match, number);
      int n2 = stoi(r_match[0]) - 1;

      //get interface address of the xth interface
      Ipv4Address addri = ipv4->GetAddress((j+1),0).GetLocal();
cout << addri << endl;
      //finish setting node's application target
      AddressValue remoteAddress (InetSocketAddress (addri, port));
      onOffHelper.SetAttribute ("Remote", remoteAddress);
      spokeApps.Add (onOffHelper.Install (nodes.Get(n2)));
    }
    spokeApps.Start (Seconds (1.0));
    spokeApps.Stop (Seconds (10.0));

    //place nodes into NetAnim
    AnimationInterface::SetConstantPosition(nodes.Get(n), imn_container.imn_links.at(i).coordinates.x, imn_container.imn_links.at(i).coordinates.y);
  }

  //
  //set wlan node coordinates
  //
  AnimationInterface anim("test.xml");
  for(int i = 0; i < imn_container.imn_links.size() ; i++){
    int n = 0;
    if(imn_container.imn_links.at(i).type.compare("wlan") == 0){
      regex_search(imn_container.imn_links.at(i).name,r_match,number);
      n = stoi(r_match[0]) - 1;
      anim.SetConstantPosition(nodes.Get(n), imn_container.imn_links.at(i).coordinates.x, imn_container.imn_links.at(i).coordinates.y);
    }
  }

  // Turn on global static routing so we can actually be routed across the network.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();


  Simulator::Stop (Seconds (9 + 1));

  // traces
  //ndn::L3RateTracer::InstallAll ((trace_prefix + "/rate-trace.txt").c_str(), Seconds (SIMULATION_RUNTIME + 0.9999));
  //L2RateTracer::InstallAll ((trace_prefix + "/drop-trace.txt").c_str(), Seconds (SIMULATION_RUNTIME + 0.999));
  //ndn::CsTracer::InstallAll ((trace_prefix + "/cs-trace.txt").c_str(), Seconds (SIMULATION_RUNTIME + 0.9999));
  //ndn::AppDelayTracer::InstallAll ((trace_prefix + "/app-delays-trace.txt").c_str());
  //wifiPhy.EnablePcap ((trace_prefix + "/wifi.pcap").c_str(), wifi_devices);

  Simulator::Run ();
  Simulator::Destroy ();
  
  cout << "Done." << endl;

  return 0;

}




