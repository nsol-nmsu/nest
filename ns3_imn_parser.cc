
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

      interface peerAddr = imn_container.get_interface_info(peer, peer2);
      regex_search(peerAddr.ipv4_addr, r_match, addr);
      string temp = r_match.str() + ".0";

      address.SetBase(temp.c_str(), "255.255.255.0");
      Ipv4InterfaceContainer p2pInterface = address.Assign(p2pDevices);

      cout << "Creating point-to-point connection with " << peer << " and " << peer2 << endl;
    }//=============Wifi===============
    else if(type.compare("wlan") == 0){
      int total_peers = imn_container.imn_links.at(i).peer_list.size();
      peer = imn_container.imn_links.at(i).name;
      NodeContainer wifiNodes;
      NetDeviceContainer wifiDevices;
      InternetStackHelper wifiInternet;

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
          wifiInternet.Install(peer2);
        }

        wifiMac.SetType("ns3::AdhocWifiMac");
        wifiDevices.Add(wifi.Install(wifiPhy, wifiMac, peer2));
        cout << "Adding node " << peer2 << " to WLAN " << peer << endl;

        MobilityHelper mobility;
        mobility.Install(peer2);
      }

      //wifiInternet.Install(wifiNodes);

      nodes.Add(wifiNodes);

      interface peerAddr = imn_container.get_interface_info(peer2, peer);
      regex_search(peerAddr.ipv4_addr, r_match, addr);
      string temp = r_match.str() + ".0";

      address.SetBase(temp.c_str(), "255.255.255.0");
      Ipv4InterfaceContainer wifiInterface = address.Assign(wifiDevices);
    }//=============Hub/Switch===============
    else if(type.compare("hub") == 0 || type.compare("lanswitch") == 0){
      int total_peers = imn_container.imn_links.at(i).peer_list.size();
      int total_asso_links = imn_container.imn_links.at(i).extra_links.size();
      NodeContainer csmaNodes;
      NodeContainer bridgeNode;
      NetDeviceContainer csmaDevices;
      NetDeviceContainer bridgeDevice;
      InternetStackHelper internetCsma;

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
        bridgeNode.Create(1);
        Names::Add(peer, bridgeNode.Get(0));
        nodes.Add(peer);
      }

      cout << "Creating new hub network named " << peer << endl;
      //for all peers using this hub
      for(int j = 0; j < total_peers; j++){
        peer2 = imn_container.imn_links.at(i).peer_list.at(j);
        CsmaHelper csma;

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
          nodes.Add(peer2);
          internetCsma.Install(peer2);
        }

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

        NetDeviceContainer link = csma.Install(NodeContainer(peer2, peer));
        csmaDevices.Add(link.Get(0));
        bridgeDevice.Add(link.Get(1));

        cout << "Adding node " << peer2 << " to a csma(hub) " << peer << endl;
      }

      BridgeHelper bridgeHelp;
      bridgeHelp.Install(peer, bridgeDevice);

      interface peerAddr = imn_container.get_interface_info(peer2, peer);
      regex_search(peerAddr.ipv4_addr, r_match, addr);
      string temp = r_match.str() + ".0";

      address.SetBase(temp.c_str(), "255.255.255.0");
      Ipv4InterfaceContainer csmaInterface = address.Assign(csmaDevices);
    }
  }//end of for loop

  cout << "\nCORE topology imported...\n\nSetting NetAnim coordinates for " << nodes.GetN() << " connected nodes..." << endl;

  //
  //set router/pc/p2p/other coordinates for NetAnim
  //


/////////////////////////////////////////////////////
// need to deal with rouge nodes somehow...
/////////////////////////////////////////////////////
  int nNodes = nodes.GetN(), extra = 0, tNodes = imn_container.imn_nodes.size();

/////////////////////////////////////////////////////
  string nodeName;

  for(int i = 0; i < imn_container.imn_nodes.size() ; i++){
    nodeName = imn_container.imn_nodes.at(i).name;

        bool nflag = false;
        for(int x = 0; x < nNodes; x++){
          if(nodeName.compare(Names::FindName(nodes.Get(x))) == 0){
            nflag = true;
            break;
          }
        }

        if(!nflag){
          nodes.Create(1);
          Names::Add(nodeName, nodes.Get(nodes.GetN() - 1));
          InternetStackHelper rStack;
          rStack.Install(nodeName);
          nNodes++;
          extra++;
        }

    int n = Names::Find<Node>(nodeName)->GetId();
    cout << nodeName << " id " << n;

    AnimationInterface::SetConstantPosition(Names::Find<Node>(nodeName), imn_container.imn_nodes.at(i).coordinates.x, imn_container.imn_nodes.at(i).coordinates.y);

    cout << " set..." << endl;
  }

  if(extra > 0){
    cout << extra << " rouge (unconnected) node(s) detected!" << endl;
  }

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

    nodeName = imn_container.imn_links.at(i).name;

    //
    // Create OnOff applications to send UDP to the bridge, on the first pair.
    //
    uint16_t port = 50000;
    peer = imn_container.imn_links.at(i).peer_list.at(0);
    peer2 = imn_container.imn_links.at(i).peer_list.at(1);
    Ptr<Ipv4> ipv4 = Names::Find<Node>(peer)->GetObject<Ipv4>();
    Ipv4Address addri = ipv4->GetAddress(1,0).GetLocal();

    OnOffHelper onOffHelper ("ns3::UdpSocketFactory", Address (InetSocketAddress(addri)));
    ApplicationContainer spokeApps = onOffHelper.Install(Names::Find<Node>(peer2));
    spokeApps.Start (Seconds (1.0));
    spokeApps.Stop (Seconds (10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress (Ipv4Address::GetAny(), port)));
    ApplicationContainer sink1 = sink.Install(Names::Find<Node>(peer));
    sink1.Start(Seconds(1.0));
    sink1.Stop(Seconds(10.0));

    //place nodes into NetAnim
    AnimationInterface::SetConstantPosition(Names::Find<Node>(nodeName), imn_container.imn_links.at(i).coordinates.x, imn_container.imn_links.at(i).coordinates.y);
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




