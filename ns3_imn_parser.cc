
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
#include "ns3/bridge-module.h"

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

  // simulation locals
  NodeContainer nodes;
  ApplicationContainer apps;

  // read command-line parameters
  CommandLine cmd;
  cmd.AddValue("topo", "Path to intermediate topology file", topo_name);
  cmd.Parse (argc, argv);
  
  imnHelper imn_container(topo_name.c_str()); //holds entire imn file in a list of node and list link containers
  //imn_container.printAll();

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
      NodeContainer p2pNodes;
      NetDeviceContainer p2pDevices;
      PointToPointHelper p2p;

      peer = imn_container.imn_links.at(i).peer_list.at(0);
      peer2 = imn_container.imn_links.at(i).peer_list.at(1);

      int nNodes = nodes.GetN();
      bool pflag = false, p2flag = false;
      for(int x = 0; x < nNodes; x++){
        if(peer.compare(Names::FindName(nodes.Get(x))) == 0){
          pflag = true;
        }
        if(peer2.compare(Names::FindName(nodes.Get(x))) == 0){
          p2flag = true;
        }
      }

      if(!pflag && !p2flag){
        p2pNodes.Create(2);
        nodes.Add(p2pNodes);
        Names::Add(peer, p2pNodes.Get(0));
        Names::Add(peer2, p2pNodes.Get(1));
      }
      if(pflag && !p2flag){
        p2pNodes.Create(1);
        nodes.Add(p2pNodes);
        Names::Add(peer2, p2pNodes.Get(0));
        p2pNodes.Add(peer);
      }
      else if(!pflag && p2flag){
        p2pNodes.Create(1);
        nodes.Add(p2pNodes);
        Names::Add(peer, p2pNodes.Get(0));
        p2pNodes.Add(peer2);
      }

      if(imn_container.imn_links.at(i).delay.empty() == 0){
        p2p.SetChannelAttribute("Delay",TimeValue(MicroSeconds(stoi(imn_container.imn_links.at(i).delay))));
      }
      if(imn_container.imn_links.at(i).bandwidth.empty() == 0){
        p2p.SetDeviceAttribute("DataRate", DataRateValue(stoi(imn_container.imn_links.at(i).bandwidth)));
      }

      p2pDevices.Add(p2p.Install(peer, peer2));

      InternetStackHelper internetP2P;

      if(!pflag && !p2flag){
        internetP2P.Install(p2pNodes);
      }
      else if(pflag && !p2flag){
        internetP2P.Install(peer2);
      }
      else if(!pflag && p2flag){
        internetP2P.Install(peer);
      }

      //Ipv4AddressHelper address;
      //address.SetBase(temp, "255.255.255.0");
      address.NewNetwork();
      Ipv4InterfaceContainer p2pInterface = address.Assign(p2pDevices);

      cout << "Creating point-to-point connection with " << peer << " and " << peer2 << endl;
    }//=============Wifi===============
    else if(type.compare("wlan") == 0){
      int total_peers = imn_container.imn_links.at(i).peer_list.size();
      peer = imn_container.imn_links.at(i).name;
      NodeContainer wifiNodes;
      NetDeviceContainer wifiDevices;

      WifiHelper wifi;
      YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
      wifiPhy.Set("RxGain", DoubleValue(0.0));
      wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

      YansWifiChannelHelper wifiChannel;
      wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
      wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
      wifiPhy.SetChannel(wifiChannel.Create());

      string phyMode("DsssRate1Mbps");
      WifiMacHelper wifiMac;
      wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
      wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

      for(int j = 0; j < total_peers; j++){
        peer2 = imn_container.imn_links.at(i).peer_list.at(j);

        int nNodes = nodes.GetN();
        bool p2flag = false;
        for(int x = 0; x < nNodes; x++){
          if(peer2.compare(Names::FindName(nodes.Get(x))) == 0){
            p2flag = true;
            break;
          }
        }

        if(!p2flag){
          wifiNodes.Create(1);
          Names::Add(peer2, wifiNodes.Get(wifiNodes.GetN() - 1));
        }

        wifiMac.SetType("ns3::AdhocWifiMac");
        wifiDevices.Add(wifi.Install(wifiPhy, wifiMac, peer2));
        cout << "Adding node " << peer2 << " to WLAN " << peer << endl;

        MobilityHelper mobility;
        mobility.Install(peer2);
      }

      InternetStackHelper wifiInternet;
      wifiInternet.Install(wifiNodes);

      nodes.Add(wifiNodes);

      //Ipv4AddressHelper address;
      //address.SetBase(temp, "255.255.255.0");
      address.NewNetwork();
      Ipv4InterfaceContainer wifiInterface = address.Assign(wifiDevices);
    }//=============Hub/Switch===============
    else if(type.compare("hub") == 0 || type.compare("lanswitch") == 0){
      int total_peers = imn_container.imn_links.at(i).peer_list.size();
      int total_asso_links = imn_container.imn_links.at(i).extra_links.size();
      NodeContainer csmaNodes;
      NetDeviceContainer csmaDevices;
      NetDeviceContainer bridgeDevice;

      peer = imn_container.imn_links.at(i).name;

      int nNodes = nodes.GetN();
      bool pflag = false;
      for(int x = 0; x < nNodes; x++){
        if(peer.compare(Names::FindName(nodes.Get(x))) == 0){
          pflag = true;
          break;
        }
      }

      if(!pflag){
        csmaNodes.Create(1);
        Names::Add(peer, csmaNodes.Get(csmaNodes.GetN() - 1));
      }

      Ptr<Node> bridge = csmaNodes.Get(csmaNodes.GetN() - 1);

      cout << "Creating new hub network named " << peer << endl;
      //for all peers using this hub
      for(int j = 0; j < total_peers; j++){
        peer2 = imn_container.imn_links.at(i).peer_list.at(j);
        NodeContainer csmaNodeSegment;
        NetDeviceContainer link;
        CsmaHelper csma;
        csmaNodeSegment.Add(bridge);

        bool p2flag = false;
        for(int x = 0; x < nNodes; x++){
          if(peer2.compare(Names::FindName(nodes.Get(x))) == 0){
            p2flag = true;
            break;
          }
        }

        if(!p2flag){
          csmaNodes.Create(1);
          Names::Add(peer2, csmaNodes.Get(csmaNodes.GetN() - 1));
        }

        csmaNodeSegment.Add(peer2);
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

        link.Add(csma.Install(csmaNodeSegment));
        csmaDevices.Add(link.Get(1));
        bridgeDevice.Add(link.Get(0));

        cout << "Adding node " << peer2 << " to a csma(hub) " << peer << endl;
      }

      BridgeHelper bridgeHelp;
      bridgeHelp.Install(bridge, bridgeDevice);
      InternetStackHelper internetCsma;
      internetCsma.Install(csmaNodes);

      nodes.Add(csmaNodes);

      //Ipv4AddressHelper address;
      //address.SetBase(temp, "255.255.255.0");
      address.NewNetwork();
      Ipv4InterfaceContainer csmaInterface = address.Assign(csmaDevices);
    }
  }//end of for loop

  cout << "\nCORE topology imported...\n\nSetting NetAnim coordinates..." << endl;

  //
  //set router/pc/p2p/other coordinates for NetAnim
  //


/////////////////////////////////////////////////////
// need to deal with rouge nodes somehow...
/////////////////////////////////////////////////////
  int nNodes = nodes.GetN(), extra = 0, tNodes = imn_container.imn_nodes.size();
  NodeContainer rouges;
//  NetDeviceContainer rDevice;

//cout << tNodes << " " <<  imn_container.imn_nodes.size() << " " << nNodes << endl;

  if(nNodes < tNodes){
    int rNodes = tNodes - nNodes;
    rouges.Create(rNodes);
    nodes.Add(rouges);
    InternetStackHelper rStack;
    rStack.Install(rouges);
    nNodes = imn_container.total;

    cout << rNodes << " rouge (unconnected) node detected!" << endl;
  }
/////////////////////////////////////////////////////
  string nodeName;

  for(int i = 0; i < imn_container.imn_nodes.size() ; i++){
    int n, nNodes = nodes.GetN();
    //regex_search(imn_container.imn_nodes.at(i).name,r_match,number);
    //n = stoi(r_match[0]) - 1;
    nodeName = imn_container.imn_nodes.at(i).name;

    for(n = 0; n < nNodes; n++){
      if(nodeName.compare(Names::FindName(nodes.Get(n))) == 0){
        break;
      }
    }

    if(n == nNodes){
      n = i;
      Names::Add(nodeName, rouges.Get(extra++));
    }

    cout << nodeName << " id " << n;

    AnimationInterface::SetConstantPosition(nodes.Get(n), imn_container.imn_nodes.at(i).coordinates.x, imn_container.imn_nodes.at(i).coordinates.y);

    cout << " set..." << endl;
  }

/*  //
  //Create a packet sink on the hubs to recieve packets
  //
  uint16_t port = 50000;
  Address hubLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", hubLocalAddress);
*/
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

/*    ApplicationContainer hubApp = packetSinkHelper.Install (nodes.Get(n));
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
*/
    //place nodes into NetAnim
    AnimationInterface::SetConstantPosition(nodes.Get(n), imn_container.imn_links.at(i).coordinates.x, imn_container.imn_links.at(i).coordinates.y);
  }

  //
  //set wlan node coordinates
  //
  AnimationInterface anim("test.xml");
/*  for(int i = 0; i < imn_container.imn_links.size() ; i++){
    int n = 0;
    if(imn_container.imn_links.at(i).type.compare("wlan") == 0){
      regex_search(imn_container.imn_links.at(i).name,r_match,number);
      n = stoi(r_match[0]) - 1;
      anim.SetConstantPosition(nodes.Get(n), imn_container.imn_links.at(i).coordinates.x, imn_container.imn_links.at(i).coordinates.y);
    }
  }*/

  cout << "Node coordinates set... \n\nSetting simulation time..." << endl;

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




