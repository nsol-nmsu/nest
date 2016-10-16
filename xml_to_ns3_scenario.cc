
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/ns2-mobility-helper.h"

#include "ns3/netanim-module.h"

//#include <string>
//#include <iostream>
//#include <sstream>
//#include <fstream>
//#include <sys/stat.h>
#include <regex>

//#include "ns3/imnHelper.h"
//#include "ns3/xmlGenerator.h"

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <exception>
#include <set>

//things from namespace std

using std::cout;
using std::endl;
using std::cerr;
using std::string;
using std::vector;
using std::ostream;
using std::regex;
using std::smatch;

using boost::property_tree::ptree;

using namespace ns3;

//trying to parse an imn file and create an ns3 scenario file from it
int main (int argc, char *argv[]) {
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1024));

  // config locals
  double duration = 10.0;
  regex cartesian("[0-9]+");
  string peer, peer2, type;
  string topo_name = "",
         traceFile = "/dev/null";

  // simulation locals
  NodeContainer nodes;
  NodeContainer bridges;

  // read command-line parameters
  CommandLine cmd;
  cmd.AddValue("topo", "Path to intermediate topology file", topo_name);
  cmd.AddValue("traceFile","Ns2 movement trace file", traceFile);
  cmd.AddValue("duration","Duration of Simulation",duration);
  //cmd.AddValue ("logFile", "Log file", logFile);
  cmd.Parse (argc, argv);

  // Check command line arguments
  if (topo_name.empty ()){
    std::cout << "Usage of " << argv[0] << " :\n\n"
    "./waf --run \"scratch/xml_to_ns3_scenario"
    " --topo=imn2ns3/imn_sample_files/sample1.xml"
    " --traceFile=imn2ns3/imn_sample_files/sample1.ns_movements"
    //" --logFile=ns2-mob.log"
    " --duration=27.0\" \n\n";

    return 0;
  }

  //holds entire list of nodes and list links containers  
  ptree pt;
  read_xml(topo_name, pt); 

  // Create Ns2MobilityHelper with the specified trace log file as parameter
  Ns2MobilityHelper ns2 = Ns2MobilityHelper (traceFile);

  // open log file for output
  //ofstream os;
  //os.open(logFile.c_str());

  //for ipv4 and ipv6, ipv6 will find mask, therefore use prefix for address
  regex addr("[0-9]+[.]{0,1}[0-9]+[.]{0,1}[0-9]+[.]{0,1}[0-9]+");
  regex addrIpv6("[/]{1}[0-9]+");
  smatch r_match;

  BOOST_FOREACH(ptree::value_type const& nod, pt.get_child("ScenarioScript.NetworkPlan")){
    const ptree& child = nod.second;
    type = child.get<string>("interface.<xmlattr>.type", "router");

//===================================
//=================P2P===============
//===================================
    if(type.compare("p2p") == 0){
      NodeContainer p2pNodes;
      NetDeviceContainer p2pDevices;
      PointToPointHelper p2p;
      string ipv4_addr, ipv6_addr;
      string ipv4_addr2, ipv6_addr2;
      bool fst = true;
      int band = 0;
      int dela = 0;

      BOOST_FOREACH(ptree::value_type const& p, child.get_child("interface.channel")){
        if(p.first == "peer"){
          if(fst){
            peer = p.second.get<string>("<xmlattr>.name");
            fst = false;
          }
          else{
            peer2 = p.second.get<string>("<xmlattr>.name");
          }
        }
      }

      // Check if nodes already exists, 
      // if so set flag and only reference by name, do not recreate
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

      if(child.get<int>("interface.channel.delay", -1) != 0){
        p2p.SetChannelAttribute("Delay",TimeValue(MicroSeconds(child.get<int>("interface.channel.delay"))));
      }
      if(child.get<int>("interface.channel.bandwidth", -1) != 0){
        p2p.SetDeviceAttribute("DataRate", DataRateValue(child.get<int>("interface.channel.bandwidth")));
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
      // get addresses ipv4 and ipv6
      BOOST_FOREACH(ptree::value_type const& p, pt.get_child("ScenarioScript.NetworkPlan")){
        if(p.second.get<string>("<xmlattr>.name") == peer){
          BOOST_FOREACH(ptree::value_type const& p2, p.second){
            if(p2.first == "interface" && p2.second.get<string>("peer.<xmlattr>.name") == peer2){
              BOOST_FOREACH(ptree::value_type const& p3, p2.second){
                if(p3.first == "address" && p3.second.get<string>("<xmlattr>.type") == "ipv4"){
                  ipv4_addr = p3.second.data();
                }
                else if(p3.first == "address" && p3.second.get<string>("<xmlattr>.type") == "ipv6"){
                  ipv6_addr = p3.second.data();
                }
              }
            }
          }
        }
        else if(p.second.get<string>("<xmlattr>.name") == peer2){
          BOOST_FOREACH(ptree::value_type const& p2, p.second){
            if(p2.first == "interface" && p2.second.get<string>("peer.<xmlattr>.name") == peer){
              BOOST_FOREACH(ptree::value_type const& p3, p2.second){
                if(p3.first == "address" && p3.second.get<string>("<xmlattr>.type") == "ipv4"){
                  ipv4_addr2 = p3.second.data();
                }
                else if(p3.first == "address" && p3.second.get<string>("<xmlattr>.type") == "ipv6"){
                  ipv6_addr2 = p3.second.data();
                }
              }
            }
          }
        }
      }

      //set addresses, starting with peer
      regex_search(ipv4_addr, r_match, addr);
      string tempIpv4 = r_match.str();
      string tempMask = r_match.suffix().str();

      regex_search(ipv6_addr, r_match, addrIpv6);
      string tempIpv6 = r_match.prefix().str();
      string tempIpv6Mask = r_match.str();

      Ptr<NetDevice> device = p2pDevices.Get (0);

      Ptr<Node> node = device->GetNode ();

      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
      Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
      int32_t deviceInterface = ipv4->GetInterfaceForDevice (device);
      if (deviceInterface == -1)
        {
          deviceInterface = ipv4->AddInterface (device);
        }

      Ipv4InterfaceAddress ipv4Addr = Ipv4InterfaceAddress (tempIpv4.c_str(), tempMask.c_str());
      ipv4->AddAddress (deviceInterface, ipv4Addr);
      ipv4->SetMetric (deviceInterface, 1);
      ipv4->SetUp (deviceInterface);

      // Install the default traffic control configuration if the traffic
      // control layer has been aggregated, if this is not 
      // a loopback interface, and there is no queue disc installed already
      Ptr<TrafficControlLayer> tc = node->GetObject<TrafficControlLayer> ();
      if (tc && DynamicCast<LoopbackNetDevice> (device) == 0 && tc->GetRootQueueDiscOnDevice (device) == 0)
        {
          //NS_LOG_LOGIC ("Installing default traffic control configuration");
          TrafficControlHelper tcHelper = TrafficControlHelper::Default ();
          tcHelper.Install (device);
        }

      deviceInterface = ipv6->GetInterfaceForDevice (device);
      if (deviceInterface == -1)
        {
          deviceInterface = ipv6->AddInterface (device);
        }
      NS_ASSERT_MSG (deviceInterface >= 0, "Ipv6AddressHelper::Allocate (): "
                     "Interface index not found");

      Ipv6InterfaceAddress ipv6Addr = Ipv6InterfaceAddress (tempIpv6.c_str(), tempIpv6Mask.c_str());
      ipv6->SetMetric (deviceInterface, 1);
      ipv6->AddAddress (deviceInterface, ipv6Addr);
      ipv6->SetUp (deviceInterface);

      tc = node->GetObject<TrafficControlLayer> ();
      if (tc && DynamicCast<LoopbackNetDevice> (device) == 0 && tc->GetRootQueueDiscOnDevice (device) == 0)
        {
          //NS_LOG_LOGIC ("Installing default traffic control configuration");
          TrafficControlHelper tcHelper = TrafficControlHelper::Default ();
          tcHelper.Install (device);
        }

      // Set address for peer2
      regex_search(ipv4_addr2, r_match, addr);
      tempIpv4 = r_match.str();
      tempMask = r_match.suffix().str();

      regex_search(ipv6_addr2, r_match, addrIpv6);
      tempIpv6 = r_match.prefix().str();
      tempIpv6Mask = r_match.str();

      device = p2pDevices.Get (1);
      node = device->GetNode ();

      ipv4 = node->GetObject<Ipv4>();
      ipv6 = node->GetObject<Ipv6>();
      deviceInterface = ipv4->GetInterfaceForDevice (device);
      if (deviceInterface == -1)
        {
          deviceInterface = ipv4->AddInterface (device);
        }

      ipv4Addr = Ipv4InterfaceAddress (tempIpv4.c_str(), tempMask.c_str());
      ipv4->AddAddress (deviceInterface, ipv4Addr);
      ipv4->SetMetric (deviceInterface, 1);
      ipv4->SetUp (deviceInterface);

      tc = node->GetObject<TrafficControlLayer> ();
      if (tc && DynamicCast<LoopbackNetDevice> (device) == 0 && tc->GetRootQueueDiscOnDevice (device) == 0)
        {
          //NS_LOG_LOGIC ("Installing default traffic control configuration");
          TrafficControlHelper tcHelper = TrafficControlHelper::Default ();
          tcHelper.Install (device);
        }

      deviceInterface = ipv6->GetInterfaceForDevice (device);
      if (deviceInterface == -1)
        {
          deviceInterface = ipv6->AddInterface (device);
        }
      NS_ASSERT_MSG (deviceInterface >= 0, "Ipv6AddressHelper::Allocate (): "
                     "Interface index not found");

      ipv6Addr = Ipv6InterfaceAddress (tempIpv6.c_str(), tempIpv6Mask.c_str());
      ipv6->SetMetric (deviceInterface, 1);
      ipv6->AddAddress (deviceInterface, ipv6Addr);
      ipv6->SetUp (deviceInterface);

      tc = node->GetObject<TrafficControlLayer> ();
      if (tc && DynamicCast<LoopbackNetDevice> (device) == 0 && tc->GetRootQueueDiscOnDevice (device) == 0)
        {
          //NS_LOG_LOGIC ("Installing default traffic control configuration");
          TrafficControlHelper tcHelper = TrafficControlHelper::Default ();
          tcHelper.Install (device);
        }

/*     //
     // Create a BulkSendApplication and install it on peer2
     //
      uint16_t port = 9;  // well-known echo port number
      deviceInterface = ipv4->GetInterfaceForDevice (device);
      Ipv4Address addri = ipv4->GetAddress(deviceInterface, 0).GetLocal();

      BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (addri, port));
      // Set the amount of data to send in bytes.  Zero is unlimited.
      source.SetAttribute ("MaxBytes", UintegerValue (2000000));
      ApplicationContainer sourceApps = source.Install (peer2);
      sourceApps.Start (Seconds (1.0));
      sourceApps.Stop (Seconds (duration / 10));

      //
      // Create a PacketSinkApplication and install it on peer
      //
      PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
      ApplicationContainer sinkApps = sink.Install (peer);
      sinkApps.Start (Seconds (2.0));
      sinkApps.Stop (Seconds (duration / 10));
*/
/*
      UdpEchoServerHelper echoServer (9);

      ApplicationContainer serverApps = echoServer.Install (peer2);
      serverApps.Start (Seconds (1.0));
      serverApps.Stop (Seconds (duration / 10));

      deviceInterface = ipv4->GetInterfaceForDevice (device);
      Ipv4Address addri = ipv4->GetAddress(deviceInterface, 0).GetLocal();
      UdpEchoClientHelper echoClient (addri, 9);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

      ApplicationContainer clientApps = echoClient.Install (peer);
      clientApps.Start (Seconds (2.0));
      clientApps.Stop (Seconds (duration / 10));
*/
      cout << "\nCreating point-to-point connection with " << peer << " and " << peer2 << endl;
    }
//====================================
//=================Wifi===============
//====================================
    if(type.compare("wlan") == 0){
      int j = 0;
      string ipv4_addr, ipv6_addr;
      peer = nod.second.get<string>("<xmlattr>.name");
      NodeContainer wifiNodes;
      NetDeviceContainer wifiDevices;
      InternetStackHelper wifiInternet;

      WifiHelper wifi;
      YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
      YansWifiChannelHelper wifiChannel;
      WifiMacHelper wifiMac;

      //wifiPhy.Set("RxGain", DoubleValue(0.0));//may not be needed
      //wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);//?

      wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
      wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
      wifiPhy.SetChannel(wifiChannel.Create());

      string phyMode("DsssRate1Mbps");
      wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
      wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

      cout << "\nCreating new wlan network named " << peer << endl;
      // Go through peer list and add them to the network
      BOOST_FOREACH(ptree::value_type const& p, child.get_child("interface.channel")){
        if(p.first == "peer"){
          peer2 = p.second.get<string>("<xmlattr>.name");

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
            nodes.Add(peer2);
          }

          wifiMac.SetType("ns3::AdhocWifiMac");
          wifiDevices.Add(wifi.Install(wifiPhy, wifiMac, peer2));
          cout << "Adding node " << peer2 << " to WLAN " << peer << endl;

          MobilityHelper mobility;
          mobility.Install(peer2);

          BOOST_FOREACH(ptree::value_type const& p, pt.get_child("ScenarioScript.NetworkPlan")){
            if(p.second.get<string>("<xmlattr>.name") == peer2){
              BOOST_FOREACH(ptree::value_type const& p2, p.second){
                if(p2.first == "interface" && p2.second.get<string>("peer.<xmlattr>.name") == peer){
                  BOOST_FOREACH(ptree::value_type const& p3, p2.second){
                    if(p3.first == "address" && p3.second.get<string>("<xmlattr>.type") == "ipv4"){
                      ipv4_addr = p3.second.data();
                    }
                    else if(p3.first == "address" && p3.second.get<string>("<xmlattr>.type") == "ipv6"){
                      ipv6_addr = p3.second.data();
                    }
                  }
                }
              }
            }
          }

          regex_search(ipv4_addr, r_match, addr);
          string tempIpv4 = r_match.str();
          string tempMask = r_match.suffix().str();

          regex_search(ipv6_addr, r_match, addrIpv6);
          string tempIpv6 = r_match.prefix().str();
          string tempIpv6Mask = r_match.str();

          // NS3 Routing has a bug with netMask 32
          // This is a temporary work around
          if(tempMask.compare("/32") == 0){
            tempMask = "/24";
          }
          if(tempIpv6Mask.compare("/128") == 0){
            tempIpv6Mask = "/64";
          }

          Ptr<NetDevice> device = wifiDevices.Get (j++);
          Ptr<Node> node = device->GetNode ();

          Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
          int32_t deviceInterface = ipv4->GetInterfaceForDevice (device);
          if (deviceInterface == -1)
            {
              deviceInterface = ipv4->AddInterface (device);
            }

          Ipv4InterfaceAddress ipv4Addr = Ipv4InterfaceAddress (tempIpv4.c_str(), tempMask.c_str());
          ipv4->AddAddress (deviceInterface, ipv4Addr);
          ipv4->SetMetric (deviceInterface, 1);
          ipv4->SetUp (deviceInterface);

          Ptr<TrafficControlLayer> tc = node->GetObject<TrafficControlLayer> ();
          if (tc && DynamicCast<LoopbackNetDevice> (device) == 0 && tc->GetRootQueueDiscOnDevice (device) == 0)
            {
              //NS_LOG_LOGIC ("Installing default traffic control configuration");
              TrafficControlHelper tcHelper = TrafficControlHelper::Default ();
              tcHelper.Install (device);
            }

          Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
          deviceInterface = ipv6->GetInterfaceForDevice (device);
          if (deviceInterface == -1)
            {
            deviceInterface = ipv6->AddInterface (device);
            }
          NS_ASSERT_MSG (deviceInterface >= 0, "Ipv6AddressHelper::Allocate (): "
                     "Interface index not found");

          Ipv6InterfaceAddress ipv6Addr = Ipv6InterfaceAddress (tempIpv6.c_str(), tempIpv6Mask.c_str());
          ipv6->SetMetric (deviceInterface, 1);
          ipv6->AddAddress (deviceInterface, ipv6Addr);
          ipv6->SetUp (deviceInterface);

          tc = node->GetObject<TrafficControlLayer> ();
          if (tc && DynamicCast<LoopbackNetDevice> (device) == 0 && tc->GetRootQueueDiscOnDevice (device) == 0)
            {
            //NS_LOG_LOGIC ("Installing default traffic control configuration");
            TrafficControlHelper tcHelper = TrafficControlHelper::Default ();
            tcHelper.Install (device);
            }
        }
      }
/*    //
    // Create OnOff applications to send UDP to the bridge, on the first pair.
    //
      uint16_t port = 50000;
      j = 0;
      BOOST_FOREACH(ptree::value_type const& p, child.get_child("interface.channel")){
        if(p.first == "peer" && j == 0){
          peer = p.second.get<string>("<xmlattr>.name");
          j++;

          Ptr<NetDevice> device = wifiDevices.Get (0);
          Ptr<Ipv4> ipv4 = Names::Find<Node>(peer)->GetObject<Ipv4>();
          int32_t deviceInterface = ipv4->GetInterfaceForDevice (device);
          Ipv4Address addri = ipv4->GetAddress(deviceInterface, 0).GetLocal();

          ApplicationContainer spokeApps;
          OnOffHelper onOffHelper ("ns3::TcpSocketFactory", Address (InetSocketAddress(addri)));

          BOOST_FOREACH(ptree::value_type const& p2, child.get_child("interface.channel")){
            if(p2.first == "peer" && p2.second.get<string>("<xmlattr>.name") != peer){
              peer2 = p2.second.get<string>("<xmlattr>.name");
              spokeApps = onOffHelper.Install(Names::Find<Node>(peer2));
              spokeApps.Start (Seconds (1.0));
              spokeApps.Stop (Seconds (duration / 10));
            }
          }
        }
      }
      PacketSinkHelper sink("ns3::TcpSocketFactory", Address(InetSocketAddress (Ipv4Address::GetAny(), port)));
      ApplicationContainer sink1 = sink.Install(Names::Find<Node>(peer));
      sink1.Start(Seconds(1.0));
      sink1.Stop(Seconds(duration / 10));
*/
    }
//==========================================
//=================Hub/Switch===============
//==========================================
    else if(type.compare("hub") == 0 || type.compare("lanswitch") == 0){
      int j = 0;
      string ipv4_addr, ipv6_addr;
      peer = nod.second.get<string>("<xmlattr>.name");
      NodeContainer csmaNodes;
      NodeContainer bridgeNode;
      NetDeviceContainer csmaDevices;
      NetDeviceContainer bridgeDevice;
      InternetStackHelper internetCsma;

      int nNodes = nodes.GetN();
      int bNodes = bridges.GetN();
      bool pflag = false;
      for(int x = 0; x < bNodes; x++){
        if(peer.compare(Names::FindName(bridges.Get(x))) == 0){
          pflag = true;
          break;
        }
      }

      if(!pflag){
        bridgeNode.Create(1);
        Names::Add(peer, bridgeNode.Get(0));
        bridges.Add(peer);
        bNodes++;
      }

      cout << "\nCreating new "<< type <<" network named " << peer << endl;
      // Go through peer list and add them to the network
      BOOST_FOREACH(ptree::value_type const& p, child.get_child("interface")){
        if(p.first == "channel"){
          string pType;
          peer2 = p.second.get<string>("peer.<xmlattr>.name");
          CsmaHelper csma;

          BOOST_FOREACH(ptree::value_type const& p, pt.get_child("ScenarioScript.NetworkPlan")){
            if(p.second.get<string>("<xmlattr>.name") == peer2){
              pType = p.second.get<string>("interface.<xmlattr>.type", "router");
            }
          }

          bool p2Nflag = false;
          for(int x = 0; x < nNodes; x++){
            if(peer2.compare(Names::FindName(nodes.Get(x))) == 0){
              p2Nflag = true;
              break;
            }
          }
          bool p2Bflag = false;
          for(int x = 0; x < bNodes; x++){
            if(peer2.compare(Names::FindName(bridges.Get(x))) == 0){
              p2Bflag = true;
              break;
            }
          }

          if(!p2Nflag && !p2Bflag){
            if(pType.compare("hub") != 0 && pType.compare("lanswitch") != 0){
              csmaNodes.Create(1);
              Names::Add(peer2, csmaNodes.Get(csmaNodes.GetN() - 1));
              nodes.Add(peer2);
              internetCsma.Install(peer2);
            }
            else{
              bridgeNode.Create(1);
              Names::Add(peer2, bridgeNode.Get(bridgeNode.GetN() - 1));
              bridges.Add(peer2);
            }
          }

          if(p.second.get<int>("delay", -1) != -1){
            csma.SetChannelAttribute("Delay",TimeValue(MicroSeconds(p.second.get<int>("delay"))));
          }
          if(p.second.get<int>("bandwidth", -1) != -1){
            csma.SetChannelAttribute("DataRate", DataRateValue(p.second.get<int>("bandwidth")));
          }

          NetDeviceContainer link = csma.Install(NodeContainer(peer2, peer));

          if(pType.compare("hub") != 0 && pType.compare("lanswitch") != 0){
            csmaDevices.Add(link.Get(0));
            bridgeDevice.Add(link.Get(1));
          }
          else{
            //bridgeDevice.Add(link.Get(0));
            bridgeDevice.Add(link.Get(1));
            //bridgeDevice.Add(link);
            //BridgeHelper bridgeHelp;
            //bridgeHelp.Install(peer2, link.Get(0));
            cout << "Linking " << pType << " " << peer2 << " to a csma(" << type << ") " << peer << endl;
            continue;
          }

          // Set addresses
          BOOST_FOREACH(ptree::value_type const& p, pt.get_child("ScenarioScript.NetworkPlan")){
            if(p.second.get<string>("<xmlattr>.name") == peer2){
              BOOST_FOREACH(ptree::value_type const& p2, p.second){
                if(p2.first == "interface" && p2.second.get<string>("peer.<xmlattr>.name") == peer){
                  BOOST_FOREACH(ptree::value_type const& p3, p2.second){
                    if(p3.first == "address" && p3.second.get<string>("<xmlattr>.type") == "ipv4"){
                      ipv4_addr = p3.second.data();
                    }
                    else if(p3.first == "address" && p3.second.get<string>("<xmlattr>.type") == "ipv6"){
                      ipv6_addr = p3.second.data();
                    }
                  }
                }
              }
            }
          }

          regex_search(ipv4_addr, r_match, addr);
          string tempIpv4 = r_match.str();
          string tempMask = r_match.suffix().str();

          regex_search(ipv6_addr, r_match, addrIpv6);
          string tempIpv6 = r_match.prefix().str();
          string tempIpv6Mask = r_match.str();
          //cout << tempIpv4 << tempMask << endl;
          Ptr<NetDevice> device = csmaDevices.Get (j);

          Ptr<Node> node = device->GetNode ();
          NS_ASSERT_MSG (node, "Ipv4AddressHelper::Assign(): NetDevice is not not associated "
                       "with any node -> fail");
          Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
          NS_ASSERT_MSG (ipv4, "Ipv4AddressHelper::Assign(): NetDevice is associated"
                       " with a node without IPv4 stack installed -> fail "
                       "(maybe need to use InternetStackHelper?)");

          int32_t deviceInterface = ipv4->GetInterfaceForDevice (device);
          if (deviceInterface == -1)
            {
              deviceInterface = ipv4->AddInterface (device);
            }
          NS_ASSERT_MSG (deviceInterface >= 0, "Ipv4AddressHelper::Assign(): "
                       "Interface index not found");


          Ipv4InterfaceAddress ipv4Addr = Ipv4InterfaceAddress (tempIpv4.c_str(), tempMask.c_str());
          ipv4->AddAddress (deviceInterface, ipv4Addr);
          ipv4->SetMetric (deviceInterface, 1);
          ipv4->SetUp (deviceInterface);

          Ptr<TrafficControlLayer> tc = node->GetObject<TrafficControlLayer> ();
          if (tc && DynamicCast<LoopbackNetDevice> (device) == 0 && tc->GetRootQueueDiscOnDevice (device) == 0)
            {
              //NS_LOG_LOGIC ("Installing default traffic control configuration");
              TrafficControlHelper tcHelper = TrafficControlHelper::Default ();
              tcHelper.Install (device);
            }


          Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
          deviceInterface = ipv6->GetInterfaceForDevice (device);
          if (deviceInterface == -1)
            {
            deviceInterface = ipv6->AddInterface (device);
            }
          NS_ASSERT_MSG (deviceInterface >= 0, "Ipv6AddressHelper::Allocate (): "
                       "Interface index not found");

          Ipv6InterfaceAddress ipv6Addr = Ipv6InterfaceAddress (tempIpv6.c_str(), tempIpv6Mask.c_str());
          ipv6->SetMetric (deviceInterface, 1);
          ipv6->AddAddress (deviceInterface, ipv6Addr);
          ipv6->SetUp (deviceInterface);

          tc = node->GetObject<TrafficControlLayer> ();
          if (tc && DynamicCast<LoopbackNetDevice> (device) == 0 && tc->GetRootQueueDiscOnDevice (device) == 0)
            {
            //NS_LOG_LOGIC ("Installing default traffic control configuration");
            TrafficControlHelper tcHelper = TrafficControlHelper::Default ();
            tcHelper.Install (device);
            }

          // increament to get correct device for Address next iteration
          j++;
          cout << "Adding node " << peer2 << " to a csma(" << type << ") " << peer << endl;
        }
      }
/*      //
      // Create OnOff applications to send UDP to the bridge, on the first pair.
      //
      uint16_t port = 50000;
      string peer1;
      j = 0;
      BOOST_FOREACH(ptree::value_type const& p, child.get_child("interface")){
        if(p.first == "channel" && j == 0){
          peer1 = p.second.get<string>("peer.<xmlattr>.name");
          j++;

          Ptr<NetDevice> device = csmaDevices.Get (0);
          Ptr<Ipv4> ipv4 = Names::Find<Node>(peer1)->GetObject<Ipv4>();
          int32_t deviceInterface = ipv4->GetInterfaceForDevice (device);
          Ipv4Address addri = ipv4->GetAddress(deviceInterface, 0).GetLocal();

          ApplicationContainer spokeApps;
          OnOffHelper onOffHelper ("ns3::TcpSocketFactory", Address (InetSocketAddress(addri)));

          BOOST_FOREACH(ptree::value_type const& p2, child.get_child("interface")){
            if(p2.first == "channel" && p2.second.get<string>("peer.<xmlattr>.name") != peer1){
              peer2 = p2.second.get<string>("peer.<xmlattr>.name");
              spokeApps = onOffHelper.Install(Names::Find<Node>(peer2));
              spokeApps.Start (Seconds (1.0));
              spokeApps.Stop (Seconds (duration / 10));
            }
          }
        }
      }
      PacketSinkHelper sink("ns3::TcpSocketFactory", Address(InetSocketAddress (Ipv4Address::GetAny(), port)));
      ApplicationContainer sink1 = sink.Install(Names::Find<Node>(peer1));
      sink1.Start(Seconds(1.0));
      sink1.Stop(Seconds(duration / 10));
*/
      BridgeHelper bridgeHelp;
      bridgeHelp.Install(peer, bridgeDevice);
    }
  }
////////////////////////////////
//END OF TOPOLOGY BUILDER
////////////////////////////////
  cout << "\nCORE topology imported..." << endl; 
////////////////////////////////
//START OF APPLICATION GENERATOR
////////////////////////////////






  cout << "\nSetting NetAnim coordinates for " << nodes.GetN() << " connected nodes..." << endl;

  //
  //set router/pc/p2p/other coordinates for NetAnim
  //
  int nNodes = nodes.GetN(), extra = 0, bNodes = bridges.GetN();
  string nodeName;

  // Set node coordinates for NetAnim use
  BOOST_FOREACH(ptree::value_type const& nod, pt.get_child("ScenarioScript.NetworkPlan")){
   if(nod.second.get<string>("interface.<xmlattr>.type", "router") == "wlan" || nod.second.get<string>("interface.<xmlattr>.type", "router") == "p2p"){
     continue;
   }
    nodeName = nod.second.get<string>("<xmlattr>.name");
    type = nod.second.get<string>("interface.<xmlattr>.type", "router");
    int loc1, loc2;

    bool nflag = false;
    for(int x = 0; x < nNodes; x++){
      if(nodeName.compare(Names::FindName(nodes.Get(x))) == 0){
        nflag = true;
        break;
      }
    }
    for(int x = 0; x < bNodes; x++){
      if(nodeName.compare(Names::FindName(bridges.Get(x))) == 0){
        nflag = true;
        break;
      }
    }

    // if node was rouge, create node to reference
    if(!nflag){
      nodes.Create(1);
      Names::Add(nodeName, nodes.Get(nodes.GetN() - 1));
      InternetStackHelper rStack;
      rStack.Install(nodeName);
      nNodes++;
      extra++;
    }

    int n = Names::Find<Node>(nodeName)->GetId();
    cout << type << " " << nodeName << " with id " << n;

    // get x coord
    string location = nod.second.get<string>("Location").data();
    regex_search(location, r_match, cartesian);
    loc1 = stoi(r_match.str());
    // get y coord
    location = r_match.suffix().str();
    regex_search(location, r_match, cartesian);
    loc2 = stoi(r_match.str());

    AnimationInterface::SetConstantPosition(Names::Find<Node>(nodeName), loc1, loc2);

    cout << " set..." << endl;
  }

  if(extra > 0){
    cout << extra << " rouge (unconnected) node(s) detected!" << endl;
  }

  AnimationInterface anim("NetAnim-xml-to-ns3.xml");
  //anim.EnablePacketMetadata(true);
  //anim.EnableIpv4RouteTracking ("testRouteTrackingXml.xml", Seconds(1.0), Seconds(3.0), Seconds(5));

  // install ns2 mobility script
  ns2.Install();

  cout << "Node coordinates set... \n\nSetting simulation time..." << endl;

  // Turn on global static routing so we can actually be routed across the network.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Configure callback for logging
//  Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange",
//                   MakeBoundCallback (&CourseChange, &os));

  Simulator::Stop (Seconds (duration));
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
