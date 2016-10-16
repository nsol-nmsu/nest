
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
#include "ns3/LatLong-UTMconversion.h"

#include <boost/optional/optional.hpp>
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
using boost::optional;

using namespace ns3;
// globals for position conversion
static double refLat, refLon, refAlt, refScale, refLocx, refLocy;
static double x = 0.0;
static double y = 0.0;
static double refX = 0.0; // in orginal calculations but uneeded
static double refY = 0.0; // in orginal calculations but uneeded
static int refZoneNum;
static char refUTMZone;

// globals for splitting strings
regex addr("[0-9]+[.]{0,1}[0-9]+[.]{0,1}[0-9]+[.]{0,1}[0-9]+");
regex addrIpv6("[/]{1}[0-9]+");
regex name("[a-zA-Z0-9]+");
smatch r_match;

// globals for get/set addresses
static string mac_addr  = "skip";
static string ipv4_addr = "skip";
static string ipv6_addr = "skip";

// convert latitude/longitude location data to y/x Cartasian coordinates
void getXYPosition(const double Lat, const double Lon, double &rx, double &ry){
  char UTMZone;
  int zoneNum;
  double Locx, meterX;
  double Locy, meterY;
  // convert latitude/longitude location data to UTM meter coordinates
  LLtoUTM(23, Lat, Lon, Locy, Locx, UTMZone, zoneNum);

  if(refZoneNum != zoneNum){
    double tempX,tempY, xShift, yShift;
    double lon2 = refLon + 6 * (zoneNum - refZoneNum);
    double lat2 = refLat + (double)(UTMZone - refUTMZone);
    char tempC;
    int tempI;

    // get easting shift to get position x in meters
    LLtoUTM(23, refLat, lon2, tempY, tempX, tempC, tempI);
    xShift = haversine(refLon, refLat, lon2, refLat) - tempX;
    meterX = Locx + xShift;

    // get northing shift to get position y in meters
    LLtoUTM(23, lat2, refLon, tempY, tempX, tempC, tempI);
    yShift = -(haversine(refLon, refLat, refLon, lat2) + tempY);
    meterY = Locy + yShift;

    // convert meters to pixels to match CORE canvas coordinates
    rx = (100.0 * (meterX / refScale)) + refX;
    ry = -((100.0 * (meterY / refScale)) + refY);
  }
  else{
    meterX = Locx - refLocx;
    meterY = Locy - refLocy;

    // convert meters to pixels to match CORE canvas coordinates
    rx = (100.0 * (meterX / refScale)) + refX;
    ry = -((100.0 * (meterY / refScale)) + refY);
  }
}

void getAddresses(ptree pt, string sourceNode, string peerNode){
  BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
    if(pl1.first != "host" && pl1.first != "router"){
      continue;
    }
    if(pl1.second.get<string>("<xmlattr>.name") == sourceNode){
      getXYPosition(pl1.second.get<double>("point.<xmlattr>.lat"), 
                    pl1.second.get<double>("point.<xmlattr>.lon"), x, y);

      AnimationInterface::SetConstantPosition(Names::Find<Node>(sourceNode), x, y);

      BOOST_FOREACH(ptree::value_type const& pl2, pl1.second){
        if(pl2.first == "interface" && pl2.second.get<string>("<xmlattr>.id") == peerNode){
          BOOST_FOREACH(ptree::value_type const& pl3, pl2.second){
            if(pl3.first == "address"){
              if(pl3.second.get<string>("<xmlattr>.type") == "IPv4"){
                ipv4_addr = pl3.second.data();
              }
              else if(pl3.second.get<string>("<xmlattr>.type") == "IPv6"){
                ipv6_addr = pl3.second.data();
              }
              else if(pl3.second.get<string>("<xmlattr>.type") == "mac"){
                mac_addr = pl3.second.data();
              }
            }
          }
        }
      }
    }
  }
}

void assignDeviceAddress(const Ptr<NetDevice> device){
  Ptr<Node> node = device->GetNode ();
  int32_t deviceInterface;
  if(mac_addr.compare("skip") != 0){
    device->SetAddress(Mac48Address(mac_addr.c_str()));
  }

  if(ipv4_addr.compare("skip") != 0){
    regex_search(ipv4_addr, r_match, addr);
    string tempIpv4 = r_match.str();
    string tempMask = r_match.suffix().str();

    // NS3 Routing has a bug with netMask 32
    // This is a temporary work around
    if(tempMask.compare("/32") == 0){
      tempMask = "/24";
    }

    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    deviceInterface = ipv4->GetInterfaceForDevice (device);
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
}

  if(ipv6_addr.compare("skip") != 0){
    regex_search(ipv6_addr, r_match, addrIpv6);
    string tempIpv6 = r_match.prefix().str();
    string tempIpv6Mask = r_match.str();

    if(tempIpv6Mask.compare("/128") == 0){
      tempIpv6Mask = "/64";
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
}

  Ptr<TrafficControlLayer> tc = node->GetObject<TrafficControlLayer> ();
  if (tc && DynamicCast<LoopbackNetDevice> (device) == 0 && tc->GetRootQueueDiscOnDevice (device) == 0)
     {
     //NS_LOG_LOGIC ("Installing default traffic control configuration");
     TrafficControlHelper tcHelper = TrafficControlHelper::Default ();
     tcHelper.Install (device);
     }

  mac_addr  = "skip";
  ipv4_addr = "skip";
  ipv6_addr = "skip";
}

//
// Parse CORE XML and create an ns3 scenario file from it
//
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

  // Get xml position reference for calculations
  refLat = pt.get<double>("scenario.CORE:sessionconfig.origin.<xmlattr>.lat");
  refLon = pt.get<double>("scenario.CORE:sessionconfig.origin.<xmlattr>.lon");
  refAlt = pt.get<double>("scenario.CORE:sessionconfig.origin.<xmlattr>.alt");
  refScale = pt.get<double>("scenario.CORE:sessionconfig.origin.<xmlattr>.scale100");

  LLtoUTM(23,refLat,refLon,refLocy,refLocx,refUTMZone,refZoneNum);

  // Build topology
  BOOST_FOREACH(ptree::value_type const& nod, pt.get_child("scenario")){
    if(nod.first != "network"){
      continue;
    }
    const ptree& child = nod.second;
    type = child.get<string>("type");

    // Correctly determine network topology to build
    if(type.compare("ethernet") == 0){
      optional<const ptree&> bridge_exists = child.get_child_optional("hub");
      if(!bridge_exists){
        bridge_exists = child.get_child_optional("switch");
        if(!bridge_exists){
          bridge_exists = child.get_child_optional("host");
          if(!bridge_exists){
            type = "p2p";
          }
          else{
            type = child.get<string>("host.type");
          }
        }
        else{
          type = "lanswitch";
        }
      }
      else{
        type = "hub";
      }
    }
//===================================
//=================P2P===============
//===================================
    if(type.compare("p2p") == 0){
      NodeContainer p2pNodes;
      NetDeviceContainer p2pDevices;
      PointToPointHelper p2p;
      string ipv4_addr, ipv6_addr, mac_addr;
      string ipv4_addr2, ipv6_addr2, mac_addr2;
      string name_holder, name_holder2;
      bool fst = true;
      int band = 0;
      int dela = 0;

      BOOST_FOREACH(ptree::value_type const& p0, child.get_child("channel")){
        if(p0.first == "member" && p0.second.get<string>("<xmlattr>.type") == "interface"){
          if(fst){
            name_holder = p0.second.data();
            regex_search(name_holder, r_match, name);
            peer = r_match.str();
            fst = false;
          }
          else{
            name_holder2 = p0.second.data();
            regex_search(name_holder2, r_match, name);
            peer2 = r_match.str();
          }
        }
      }

      // Check if nodes already exists, 
      // if so set flag and only reference by name, do not recreate
      int nNodes = nodes.GetN();
      bool pflag = false, p2flag = false;
      for(int i = 0; i < nNodes; i++){
        if(peer.compare(Names::FindName(nodes.Get(i))) == 0){
          pflag = true;
        }
        if(peer2.compare(Names::FindName(nodes.Get(i))) == 0){
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

      BOOST_FOREACH(ptree::value_type const& p0, child.get_child("channel")){
        if(p0.first == "parameter"){
          if(p0.second.get<string>("<xmlattr>.name") == "bw"){
            p2p.SetDeviceAttribute("DataRate", DataRateValue(stoi(p0.second.data())));
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "delay"){
            p2p.SetChannelAttribute("Delay",TimeValue(MicroSeconds(stoi(p0.second.data()))));
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "loss"){
            Ptr<RateErrorModel> rem = CreateObjectWithAttributes<RateErrorModel>("ErrorRate", DoubleValue(stod(p0.second.data())));
            p2p.SetDeviceAttribute("ReceiveErrorModel",PointerValue(rem));
          }
        }
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

      // Get then set addresses
      getAddresses(pt, peer, name_holder);
      Ptr<NetDevice> device = p2pDevices.Get (0);
      assignDeviceAddress(device);

      getAddresses(pt, peer2, name_holder2);
      device = p2pDevices.Get (1);
      assignDeviceAddress(device);

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
    if(type.compare("wireless") == 0){
      int j = 0;
      string ipv4_addr, ipv6_addr, mac_addr;
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
      BOOST_FOREACH(ptree::value_type const& p, child.get_child("channel")){
        if(p.first == "member"){
          string name_holder;
          name_holder = p.second.data();
          regex_search(name_holder, r_match, name);
          peer2 = r_match.str();
          if(peer2.compare(peer) == 0){
            continue;
          }

          int nNodes = nodes.GetN();
          bool p2flag = false;
          for(int i = 0; i < nNodes; i++){
            if(peer2.compare(Names::FindName(nodes.Get(i))) == 0){
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

          // Get then set address
          getAddresses(pt, peer2, name_holder);
          Ptr<NetDevice> device = wifiDevices.Get (j++);
          assignDeviceAddress(device);
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
      string ipv4_addr, ipv6_addr, mac_addr;
      peer = nod.second.get<string>("<xmlattr>.name");
      NodeContainer csmaNodes;
      NodeContainer bridgeNode;
      NetDeviceContainer csmaDevices;
      NetDeviceContainer bridgeDevice;
      InternetStackHelper internetCsma;

      int nNodes = nodes.GetN();
      int bNodes = bridges.GetN();
      bool pflag = false;
      for(int i = 0; i < bNodes; i++){
        if(peer.compare(Names::FindName(bridges.Get(i))) == 0){
          pflag = true;
          break;
        }
      }

      if(!pflag){
        bridgeNode.Create(1);
        Names::Add(peer, bridgeNode.Get(0));
        bridges.Add(peer);
        bNodes++;

        getXYPosition(nod.second.get<double>("point.<xmlattr>.lat"), 
                      nod.second.get<double>("point.<xmlattr>.lon"), x, y);

        AnimationInterface::SetConstantPosition(Names::Find<Node>(peer), x, y);
      }

      cout << "\nCreating new "<< type <<" network named " << peer << endl;
      // Go through channels and add neighboring members to the network
      BOOST_FOREACH(ptree::value_type const& p0, child){
        if(p0.first == "channel"){
          string param, name_holder;
          CsmaHelper csma;

          BOOST_FOREACH(ptree::value_type const& tp0, p0.second){
            if(tp0.first == "member"){
              name_holder = tp0.second.data();
              regex_search(name_holder, r_match, name);
              peer2 = r_match.str();

              if(peer2.compare(peer) != 0){
                break;
              }
            }
          }

          bool p2Nflag = false;
          for(int i = 0; i < nNodes; i++){
            if(peer2.compare(Names::FindName(nodes.Get(i))) == 0){
              p2Nflag = true;
              break;
            }
          }
          bool p2Bflag = false;
          for(int i = 0; i < bNodes; i++){
            if(peer2.compare(Names::FindName(bridges.Get(i))) == 0){
              p2Bflag = true;
              break;
            }
          }
          // TODO: check if peer is another hub/lanswitch
          // to avoid broadcast storm
          if(!p2Nflag && !p2Bflag){
              csmaNodes.Create(1);
              Names::Add(peer2, csmaNodes.Get(csmaNodes.GetN() - 1));
              nodes.Add(peer2);
              internetCsma.Install(peer2);
          }

            BOOST_FOREACH(ptree::value_type const& p1, p0.second){
            if(p1.first == "parameter"){
              if(p1.second.get<string>("<xmlattr>.name") == "bw"){
                csma.SetChannelAttribute("DataRate", DataRateValue(stoi(p1.second.data())));
              }
              else if(p1.second.get<string>("<xmlattr>.name") == "delay"){
                csma.SetChannelAttribute("Delay",TimeValue(MicroSeconds(stoi(p1.second.data()))));
              }
              else if(p1.second.get<string>("<xmlattr>.name") == "loss"){
                Ptr<RateErrorModel> rem = CreateObjectWithAttributes<RateErrorModel>("ErrorRate", DoubleValue(stod(p1.second.data())));
                csma.SetDeviceAttribute("ReceiveErrorModel",PointerValue(rem));
              }
            }
          }

          NetDeviceContainer link = csma.Install(NodeContainer(peer2, peer));

          //if(pType.compare("hub") != 0 && pType.compare("lanswitch") != 0){
            csmaDevices.Add(link.Get(0));
            bridgeDevice.Add(link.Get(1));
          //}
          //else{
            //bridgeDevice.Add(link.Get(0));
            //bridgeDevice.Add(link.Get(1));
            //bridgeDevice.Add(link);
            //BridgeHelper bridgeHelp;
            //bridgeHelp.Install(peer2, link.Get(0));
            //cout << "Linking " << pType << " " << peer2 << " to a csma(" << type << ") " << peer << endl;
            //continue;
          //}

          // Get then set address
          getAddresses(pt, peer2, name_holder);
          Ptr<NetDevice> device = csmaDevices.Get (j++);
          assignDeviceAddress(device);

          cout << "Adding node " << peer2 << " to a csma(" << type << ") " << peer << endl;
        }
      }
/*      //
      // Create OnOff applications to send UDP to the bridge, on the first pair.
      //
      uint16_t port = 50000;
      string peer1;
      j = 0;
      BOOST_FOREACH(ptree::value_type const& pa1, child.get_child("interface")){
        if(pa1.first == "channel" && j == 0){
          peer1 = pa1.second.get<string>("peer.<xmlattr>.name");
          j++;

          Ptr<NetDevice> device = csmaDevices.Get (0);
          Ptr<Ipv4> ipv4 = Names::Find<Node>(peer1)->GetObject<Ipv4>();
          int32_t deviceInterface = ipv4->GetInterfaceForDevice (device);
          Ipv4Address addri = ipv4->GetAddress(deviceInterface, 0).GetLocal();

          ApplicationContainer spokeApps;
          OnOffHelper onOffHelper ("ns3::TcpSocketFactory", Address (InetSocketAddress(addri)));

          BOOST_FOREACH(ptree::value_type const& pa2, child.get_child("interface")){
            if(pa2.first == "channel" && pa2.second.get<string>("peer.<xmlattr>.name") != peer1){
              peer2 = pa2.second.get<string>("peer.<xmlattr>.name");
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
//START OF APPLICATION GENERATOR TODO
////////////////////////////////






  cout << endl << nodes.GetN() << " defined node names with their respective id's..." << endl;

  int nNodes = nodes.GetN(), extra = 0, bNodes = bridges.GetN();
  string nodeName;

  // Set node coordinates for rouge nodes
  BOOST_FOREACH(ptree::value_type const& nod, pt.get_child("scenario")){
    if(nod.first != "router" && nod.first != "host"){
      continue;
    }

    nodeName = nod.second.get<string>("<xmlattr>.name");
    int Locx, Locy;

    bool nflag = false;
    for(int x = 0; x < nNodes; x++){
      if(nodeName.compare(Names::FindName(nodes.Get(x))) == 0){
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
    else{
      int n = Names::Find<Node>(nodeName)->GetId();
      cout << nodeName << " with id " << n << endl;
      continue;
    }

    int n = Names::Find<Node>(nodeName)->GetId();
    cout << type << " " << nodeName << " with id " << n;

    getXYPosition(nod.second.get<double>("point.<xmlattr>.lat"), 
                  nod.second.get<double>("point.<xmlattr>.lon"), x, y);

    AnimationInterface::SetConstantPosition(Names::Find<Node>(nodeName), x, y);

    cout << " set..." << endl;
  }

  if(extra > 0){
    cout << extra << " rouge (unconnected) node(s) detected!" << endl;
  }

  AnimationInterface anim("NetAnim-core-to-ns3.xml");
  //anim.EnablePacketMetadata(true);
  //anim.EnableIpv4RouteTracking ("testRouteTrackingXml.xml", Seconds(1.0), Seconds(3.0), Seconds(5));

  // install ns2 mobility script
  ns2.Install();

  cout << "Setting simulation time..." << endl;

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







