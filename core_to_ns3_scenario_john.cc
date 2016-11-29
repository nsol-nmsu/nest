
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
#include "ns3/flow-monitor-helper.h"
#include "ns3/olsr-routing-protocol.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/netanim-module.h"

#include <regex>
#include <sys/stat.h>

#include "ns3/LatLong-UTMconversion.h"

#include <boost/optional/optional.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <exception>
#include <set>

//--------------------------------------------------------------------
//things from namespace std
//--------------------------------------------------------------------
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

NS_LOG_COMPONENT_DEFINE ("CORE_to_NS3_scenario");

//--------------------------------------------------------------------
// globals for position conversion
//--------------------------------------------------------------------
static double refLat, refLon, refAlt, refScale, refLocx, refLocy;
static double x = 0.0;
static double y = 0.0;
static double refX = 0.0; // in orginal calculations but uneeded
static double refY = 0.0; // in orginal calculations but uneeded
static int refZoneNum;
static char refUTMZone;

//--------------------------------------------------------------------
// globals for splitting strings
//--------------------------------------------------------------------
static regex addr("[0-9]+[.]{0,1}[0-9]+[.]{0,1}[0-9]+[.]{0,1}[0-9]+");
static regex addrIpv6("[/]{1}[0-9]+");
static regex name("[a-zA-Z0-9]+");
static regex interId("[a-zA-Z0-9]+[/]{1}[a-zA-Z0-9]+");
//smatch r_match;

//--------------------------------------------------------------------
// globals for get/set addresses, routing protocols and packet control
//--------------------------------------------------------------------
static string mac_addr  = "skip";// some may not exists, skip them
static string ipv4_addr = "skip";
static string ipv6_addr = "skip";

enum AppType{
  UDP,
  UDPECHO,
  TCP,
  BURST,
  BULK,
  SINK,
  DEFUALT
};

enum RouteType{
  STATIC,
  OLSR,
  AODV,
  DSDV,
  DSR
};

//====================================================================
// convert latitude/longitude location data to y/x Cartasian coordinates
//====================================================================
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

//====================================================================
// extract address from XML
//====================================================================
void getAddresses(ptree pt, string sourceNode, string peerNode){
  BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
    if(pl1.first != "host" && pl1.first != "router"){
      continue;
    }
    if(pl1.second.get<string>("<xmlattr>.name") == sourceNode){
      getXYPosition(pl1.second.get<double>("point.<xmlattr>.lat"), 
                    pl1.second.get<double>("point.<xmlattr>.lon"), x, y);
      // set node coordinates while we're here
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

//====================================================================
// set mac/ipv4/ipv6 addresses if available
//====================================================================
void assignDeviceAddress(string type, const Ptr<NetDevice> device){
  smatch r_match;
  NS_LOG_INFO ("Assign IP Addresses.");
  Ptr<Node> node = device->GetNode ();
  int32_t deviceInterface = -1;
  if(mac_addr.compare("skip") != 0){
    device->SetAddress(Mac48Address(mac_addr.c_str()));
  }

  if(ipv4_addr.compare("skip") != 0){
    regex_search(ipv4_addr, r_match, addr);
    string tempIpv4 = r_match.str();
    string tempMask = r_match.suffix().str();

    // NS3 Routing has a bug with netMask 32 in wireless networks
    // This is a temporary work around
    //if(type.compare("wireless") == 0 && tempMask.compare("/32") == 0){
      //tempMask = "/24";
    //}

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

    //if(type.compare("wireless") == 0 && tempIpv6Mask.compare("/128") == 0){
      //tempIpv6Mask = "/64";
    //}

    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
    deviceInterface = ipv6->GetInterfaceForDevice (device);
    if (deviceInterface == -1)
      {
      deviceInterface = ipv6->AddInterface (device);
      }
    NS_ASSERT_MSG (deviceInterface >= 0, "Ipv6AddressHelper::Allocate (): "
                   "Interface index not found");
    
    Ipv6InterfaceAddress ipv6Addr = Ipv6InterfaceAddress (tempIpv6.c_str(), atoi(tempIpv6Mask.c_str()+1));
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

//====================================================================
// applications
//====================================================================
void udpApp(ptree pt, double d){
  string receiver, sender, rAddress;
  uint32_t start, end;
  uint16_t sPort = 4000;
  uint16_t rPort = 4000;
  uint32_t packetSize = 1024;
  Time interPacketInterval = Seconds (1.0);
  uint32_t maxPacketCount = 1;

  sender = pt.get<string>("sender.node");
  sPort = pt.get<uint16_t>("sender.port");
  receiver = pt.get<string>("receiver.node");
  rAddress = pt.get<string>("receiver.ipv4Address");
  rPort = pt.get<uint16_t>("receiver.port");
  start = pt.get<uint32_t>("startTime");
  end = pt.get<uint32_t>("endTime");

  cout << "Creating UDP application with sender " << sender << " and receiver " << receiver << endl;

  optional<ptree&> if_exists = pt.get_child_optional("special.packetSize");
  if(if_exists){
    packetSize = pt.get<uint32_t>("special.packetSize");
  }

  if_exists = pt.get_child_optional("special.maxPacketCount");
  if(if_exists){
    maxPacketCount = pt.get<uint32_t>("special.maxPacketCount");
  }

  if_exists = pt.get_child_optional("special.packetIntervalTime");
  if(if_exists){
    interPacketInterval = Seconds(pt.get<double>("special.packetIntervalTime"));
  }

  // make sure end time is not beyond simulation time
  end = (end <= d)? end : d;

//
// Create one udpServer applications on destination.
//
  UdpServerHelper server (rPort);
  ApplicationContainer apps = server.Install (NodeContainer(receiver));
  apps.Start (Seconds (start));
  apps.Stop (Seconds (end));

//
// Create one UdpClient application to send UDP datagrams from source to destination.
//
  UdpClientHelper client (Ipv4Address(rAddress.c_str()), sPort);
  client.SetAttribute ("MaxPackets", UintegerValue (packetSize * maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  apps = client.Install (NodeContainer(sender));
  apps.Start (Seconds (start));
  apps.Stop (Seconds (end));
}

void udpEchoApp(ptree pt, double d){
  string receiver, sender, rAddress;
  uint32_t start, end;
  uint16_t sPort = 4000;
  uint16_t rPort = 4000;
  uint32_t packetSize = 1024;
  Time interPacketInterval = Seconds (1.0);
  uint32_t maxPacketCount = 1;

  sender = pt.get<string>("sender.node");
  sPort = pt.get<uint16_t>("sender.port");
  receiver = pt.get<string>("receiver.node");
  rAddress = pt.get<string>("receiver.ipv4Address");
  rPort = pt.get<uint16_t>("receiver.port");
  start = pt.get<uint32_t>("startTime");
  end = pt.get<uint32_t>("endTime");

  cout << "Creating UDPECHO application with sender " << sender << " and receiver " << receiver << endl;

  optional<ptree&> if_exists = pt.get_child_optional("special.packetSize");
  if(if_exists){
    packetSize = pt.get<uint32_t>("special.packetSize");
  }

  if_exists = pt.get_child_optional("special.maxPacketCount");
  if(if_exists){
    maxPacketCount = pt.get<uint32_t>("special.maxPacketCount");
  }

  if_exists = pt.get_child_optional("special.packetIntervalTime");
  if(if_exists){
    interPacketInterval = Seconds(pt.get<double>("special.packetIntervalTime"));
  }

  // make sure end time is not beyond simulation time
  end = (end <= d)? end : d;

//
// Create one udpServer applications on destination.
//
  UdpEchoServerHelper server (rPort);
  ApplicationContainer apps = server.Install (NodeContainer(receiver));
  apps.Start (Seconds (start));
  apps.Stop (Seconds (end));

//
// Create one UdpClient application to send UDP datagrams from source to destination.
//
  UdpEchoClientHelper client (Ipv4Address(rAddress.c_str()), sPort);
  client.SetAttribute ("MaxPackets", UintegerValue (packetSize * maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  apps = client.Install (NodeContainer(sender));
  apps.Start (Seconds (start));
  apps.Stop (Seconds (end));

#if 0
//
// Users may find it convenient to initialize echo packets with actual data;
// the below lines suggest how to do this
//
  client.SetFill (apps.Get (0), "Hello World");

  client.SetFill (apps.Get (0), 0xa5, 1024);

  uint8_t fill[] = { 0, 1, 2, 3, 4, 5, 6};
  client.SetFill (apps.Get (0), fill, sizeof(fill), 1024);
#endif
}

void tcpApp(ptree pt, double d){
  string receiver, sender, rAddress, sAddress;
  uint32_t start, end;
  uint16_t sPort = 4000;
  uint16_t rPort = 4000;
  uint32_t packetSize = 1024;
  Time interPacketInterval = Seconds (1.0);
  uint32_t maxPacketCount = 1;

  sender = pt.get<string>("sender.node");
  sPort = pt.get<uint16_t>("sender.port");
  receiver = pt.get<string>("receiver.node");
  rAddress = pt.get<string>("receiver.ipv4Address");
  rPort = pt.get<uint16_t>("receiver.port");
  start = pt.get<uint32_t>("startTime");
  end = pt.get<uint32_t>("endTime");

  cout << "Creating TCP application with sender " << sender << " and receiver " << receiver << endl;

  optional<ptree&> if_exists = pt.get_child_optional("special.packetSize");
  if(if_exists){
    packetSize = pt.get<uint32_t>("special.packetSize");
  }

  if_exists = pt.get_child_optional("special.maxPacketCount");
  if(if_exists){
    maxPacketCount = pt.get<uint32_t>("special.maxPacketCount");
  }

  if_exists = pt.get_child_optional("special.packetIntervalTime");
  if(if_exists){
    interPacketInterval = Seconds(pt.get<double>("special.packetIntervalTime"));
  }

  // make sure end time is not beyond simulation time
  end = (end <= d)? end : d;

  PacketSinkHelper sink ("ns3::TcpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), rPort)));
  ApplicationContainer sinkApp = sink.Install (NodeContainer(receiver));
  sinkApp.Start(Seconds(start));
  sinkApp.Stop(Seconds(end));

  OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address(InetSocketAddress (Ipv4Address (rAddress.c_str()), sPort)));
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  onOffHelper.SetAttribute("PacketSize",UintegerValue(packetSize));

  ApplicationContainer clientApp;
  clientApp = onOffHelper.Install (NodeContainer(sender));
  clientApp.Start (Seconds (start));
  clientApp.Stop (Seconds (end));
}

void sinkApp(ptree pt, double d){
  string receiver, sender, rAddress, sAddress;
  uint32_t start, end;
  uint16_t sPort = 4000;
  uint16_t rPort = 4000;
  uint32_t packetSize = 1024;
  Time interPacketInterval = Seconds (1.0);
  uint32_t maxPacketCount = 1;

  sender = pt.get<string>("sender.node");
  sPort = pt.get<uint16_t>("sender.port");
  receiver = pt.get<string>("receiver.node");
  rAddress = pt.get<string>("receiver.ipv4Address");
  rPort = pt.get<uint16_t>("receiver.port");
  start = pt.get<uint32_t>("startTime");
  end = pt.get<uint32_t>("endTime");

  cout << "Creating SINK application with sender " << sender << " and receiver/s ";

  optional<ptree&> if_exists = pt.get_child_optional("special.packetSize");
  if(if_exists){
    packetSize = pt.get<uint32_t>("special.packetSize");
  }

  if_exists = pt.get_child_optional("special.maxPacketCount");
  if(if_exists){
    maxPacketCount = pt.get<uint32_t>("special.maxPacketCount");
  }

  if_exists = pt.get_child_optional("special.packetIntervalTime");
  if(if_exists){
    interPacketInterval = Seconds(pt.get<double>("special.packetIntervalTime"));
  }

  // make sure end time is not beyond simulation time
  end = (end <= d)? end : d;

  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), rPort));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  //PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (NodeContainer(receiver));
  sinkApp.Start (Seconds (start));
  sinkApp.Stop (Seconds (end));

  BOOST_FOREACH(ptree::value_type const& nod, pt){
    if(nod.first == "sender"){
      sender = nod.second.get<string>("node");
      //sAddress = nod.second.get<string>("ipv4Address");
      sPort = nod.second.get<uint16_t>("port");

      cout << receiver << " ";

      Address remoteAddress (InetSocketAddress (Ipv4Address (rAddress.c_str()), sPort));
      OnOffHelper clientHelper ("ns3::TcpSocketFactory", remoteAddress);
      //OnOffHelper clientHelper ("ns3::UdpSocketFactory", remoteAddress);
      clientHelper.SetAttribute("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      clientHelper.SetAttribute("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      ApplicationContainer clientApp = clientHelper.Install (NodeContainer(sender));
      clientApp.Start (Seconds (start));
      clientApp.Stop (Seconds (end));
    }
  }
  cout << endl;
//  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (Names::Find<Node>("n17"), TcpSocketFactory::GetTypeId ());

//  Ptr<MyApp> app = CreateObject<MyApp> ();
//  app->Setup (ns3TcpSocket, sinkAddress, 1460, 1000000, DataRate ("100Mbps"));
//  Names::Find<Node>("n17")->AddApplication (app);
//  app->SetStartTime (Seconds (1.));
//  app->SetStopTime (Seconds (d));
}
//////////////////////////////////////////////////////////////
// TODO TODO TODO
//////////////////////////////////////////////////////////////
void burstApp(ptree pt, double d){

}

void bulkApp(ptree pt, double d){
  string receiver, sender, rAddress, sAddress;
  uint32_t start, end;
  uint16_t sPort = 4000;
  uint16_t rPort = 4000;
  uint32_t packetSize = 1024;
  Time interPacketInterval = Seconds (1.0);
  uint32_t maxPacketCount = 1;

  sender = pt.get<string>("sender.node");
  sPort = pt.get<uint16_t>("sender.port");
  receiver = pt.get<string>("receiver.node");
  rAddress = pt.get<string>("receiver.ipv4Address");
  rPort = pt.get<uint16_t>("receiver.port");
  start = pt.get<uint32_t>("startTime");
  end = pt.get<uint32_t>("endTime");

  cout << "Creating SINK application with sender " << sender << " and receiver/s ";

  optional<ptree&> if_exists = pt.get_child_optional("special.packetSize");
  if(if_exists){
    packetSize = pt.get<uint32_t>("special.packetSize");
  }

  if_exists = pt.get_child_optional("special.maxPacketCount");
  if(if_exists){
    maxPacketCount = pt.get<uint32_t>("special.maxPacketCount");
  }

  if_exists = pt.get_child_optional("special.packetIntervalTime");
  if(if_exists){
    interPacketInterval = Seconds(pt.get<double>("special.packetIntervalTime"));
  }

  // make sure end time is not beyond simulation time
  end = (end <= d)? end : d;

  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), rPort));
  ApplicationContainer sinkApps = sink.Install (Names::Find<Node>(receiver));
  sinkApps.Start (Seconds (start));
  sinkApps.Stop (Seconds (end));

  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address (sAddress.c_str()), sPort));
  source.SetAttribute ("MaxBytes", UintegerValue (packetSize));
  ApplicationContainer sourceApps = source.Install (Names::Find<Node>(sender));
  sourceApps.Start (Seconds (start));
  sourceApps.Stop (Seconds (end));
}

void createApp(ptree pt, double duration){
  NS_LOG_INFO ("Create Applications.");

  BOOST_FOREACH(ptree::value_type const& app, pt.get_child("Applications")){
    if(app.first == "application"){
      string protocol = app.second.get<string>("type");
      if(protocol.compare("Udp") == 0)          { udpApp(app.second, duration); }
      else if(protocol.compare("UdpEcho") == 0) { udpEchoApp(app.second, duration); }
      else if(protocol.compare("Tcp") == 0)     { tcpApp(app.second, duration); }
      else if(protocol.compare("Sink") == 0)    { sinkApp(app.second, duration); }
      else if(protocol.compare("Burst") == 0)   { burstApp(app.second, duration); }
      else if(protocol.compare("Bulk") == 0)    { bulkApp(app.second, duration); }
      else { cout << protocol << " protocol type not supported\n";}
    }
  }
}
//###################################################################
// Parse CORE XML and create an ns3 scenario file from it
//###################################################################
int main (int argc, char *argv[]) {
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1024));

  // The below value configures the default behavior of global routing.
  // By default, it is disabled.  To respond to interface events, set to true
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));

  // config locals
  bool   pcap = false;
  double duration = 10.0;
  regex  cartesian("[0-9]+");
  string peer, peer2, type;
  string topo_name = "",
         apps_file = "",
         ns2_mobility = "/dev/null",
         trace_prefix = "core2ns3_Logs/";

  struct stat st;
  smatch r_match;

  // simulation locals
  NodeContainer nodes;
  NodeContainer bridges;
  NodeContainer hubs;

  // read command-line parameters
  CommandLine cmd;
  cmd.AddValue("topo", "Path to intermediate topology file", topo_name);
  cmd.AddValue("apps", "Path to application generator file", apps_file);
  cmd.AddValue("ns2","Ns2 mobility script file", ns2_mobility);
  cmd.AddValue("duration","Duration of Simulation",duration);
  cmd.AddValue("pcap","Enable pcap files",pcap);
  cmd.AddValue ("traceDir", "Directory in which to store trace files", trace_prefix);
  cmd.Parse (argc, argv);

  // Check command line arguments
  if (topo_name.empty ()){
    std::cout << "Usage of " << argv[0] << " :\n\n"
    "./waf --run \"scratch/core_to_ns3_scenario"
    " --topo=imn2ns3/CORE-XML-files/sample1.xml"
    " --apps=imn2ns3/apps-files/sample1.xml"
    " --ns2=imn2ns3/NS2-mobility-files/sample1.ns_movements"
    " --traceDir=core2ns3_Logs/"
    " --pcap=true"
    //" --logFile=ns2-mob.log"
    " --duration=27.0\" \n\n";

    return 0;
  }

  string trace_check = trace_prefix;

  // Verify that trace directory exists
  if(trace_check.at(trace_check.length() - 1) == '/') // trim trailing slash
  	trace_check = trace_check.substr(0, trace_check.length() - 1);
  if(stat(trace_check.c_str(), &st) != 0 || (st.st_mode & S_IFDIR) == 0){
  	cerr << "Error: trace directory " << trace_check << " doesn't exist!" << endl;
  	return -1;
  }
  else{
  	cout << "Writing traces to directory `" << trace_check << "`." << endl;
  }

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream (trace_prefix + "core-to-ns3-scenario.tr");

  //holds entire list of nodes and list links containers  
  ptree pt;
  read_xml(topo_name, pt); 

  // Create Ns2MobilityHelper with the specified trace log file as parameter
  Ns2MobilityHelper ns2 = Ns2MobilityHelper (ns2_mobility);

  // open log file for output
  //ofstream os;
  //os.open(logFile.c_str());

  // Get xml position reference for calculations
  refLat = pt.get<double>("scenario.CORE:sessionconfig.origin.<xmlattr>.lat");
  refLon = pt.get<double>("scenario.CORE:sessionconfig.origin.<xmlattr>.lon");
  refAlt = pt.get<double>("scenario.CORE:sessionconfig.origin.<xmlattr>.alt");
  refScale = pt.get<double>("scenario.CORE:sessionconfig.origin.<xmlattr>.scale100");

  LLtoUTM(23,refLat,refLon,refLocy,refLocx,refUTMZone,refZoneNum);

//========================================================================
// Build topology
//========================================================================
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
//===================================================================
// P2P
//-------------------------------------------------------------------
    if(type.compare("p2p") == 0){
      optional<const ptree&> channel_exists = child.get_child_optional("channel");

      if(!channel_exists){ // CORE occasionally produces empty p2p networks, we skip them
        continue;
      }

      NS_LOG_INFO ("Create Point to Point channel.");
      NodeContainer p2pNodes;
      NetDeviceContainer p2pDevices;
      PointToPointHelper p2p;
      //string ipv4_addr, ipv6_addr, mac_addr;
      //string ipv4_addr2, ipv6_addr2, mac_addr2;
      string name_holder, name_holder2, pType, p2Type;
      bool fst = true;
      int band = 0;
      int dela = 0;

      // grap the node names from net
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

      // set type for service matching
      BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
        if(pl1.first != "host" && pl1.first != "router"){
          continue;
        }
        if(pl1.second.get<string>("<xmlattr>.name") == peer){
          pType = pl1.second.get<string>("type");
        }
        if(pl1.second.get<string>("<xmlattr>.name") == peer2){
          p2Type = pl1.second.get<string>("type");
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
      // set channel parameters
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

      // add internet stack if not yet created, add routing if found
      p2pDevices.Add(p2p.Install(peer, peer2));
      InternetStackHelper internetP2P;

      OlsrHelper olsr;
      Ipv4StaticRoutingHelper staticRouting;

      bool applyDefaultServices1 = true;
      bool applyDefaultServices2 = true;

      // get local services
      BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
        if(pl1.first != "host" && pl1.first != "router"){
          continue;
        }

        optional<const ptree&> service_exists = pl1.second.get_child_optional("CORE:services");
        if(service_exists){
          if(!pflag && pl1.second.get<string>("<xmlattr>.name") == peer){
            Ipv4ListRoutingHelper list;

            BOOST_FOREACH(ptree::value_type const& pl2, pl1.second.get_child("CORE:services")){
              if(pl2.first == "service"){
                if(pl2.second.get<string>("<xmlattr>.name") == "StaticRoute"){
                  list.Add (staticRouting, 0);
                }
                else if(pl2.second.get<string>("<xmlattr>.name") == "OLSR"){
                  list.Add(olsr, 10);
                }
              }
            }
            internetP2P.SetRoutingHelper(list);
            internetP2P.Install(peer);
            applyDefaultServices1 = false;
          }
          if(!p2flag && pl1.second.get<string>("<xmlattr>.name") == peer2){
            Ipv4ListRoutingHelper list;

            BOOST_FOREACH(ptree::value_type const& pl2, pl1.second.get_child("CORE:services")){
              if(pl2.first == "service"){
                if(pl2.second.get<string>("<xmlattr>.name") == "StaticRoute"){
                  list.Add (staticRouting, 0);
                }
                else if(pl2.second.get<string>("<xmlattr>.name") == "OLSR"){
                  list.Add(olsr, 10);
                }
              }
            }
            internetP2P.SetRoutingHelper(list); // has effect on the next Install ()
            internetP2P.Install(peer2);
            applyDefaultServices2 = false;
          }
        }
      }
      // if there were no local, set default services according to type
      BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
        if(pl1.first == "CORE:defaultservices"){
          if(!pflag && applyDefaultServices1 && pl1.second.get<string>("device.<xmlattr>.type") == pType){
            Ipv4ListRoutingHelper list;

            BOOST_FOREACH(ptree::value_type const& pl2, pl1.second.get_child("device")){
              if(pl2.first == "service"){
                if(pl2.second.get<string>("<xmlattr>.name") == "StaticRoute"){
                  list.Add (staticRouting, 0);
                }
                else if(pl2.second.get<string>("<xmlattr>.name") == "OLSR"){
                  list.Add(olsr, 10);
                }
              }
            }
            internetP2P.SetRoutingHelper(list);
            internetP2P.Install(peer);
          }
          if(!p2flag && applyDefaultServices2 && pl1.second.get<string>("device.<xmlattr>.type") == p2Type){
            Ipv4ListRoutingHelper list;

            BOOST_FOREACH(ptree::value_type const& pl2, pl1.second.get_child("device")){
              if(pl2.first == "service"){
                if(pl2.second.get<string>("<xmlattr>.name") == "StaticRoute"){
                  list.Add (staticRouting, 0);
                }
                else if(pl2.second.get<string>("<xmlattr>.name") == "OLSR"){
                  list.Add(olsr, 10);
                }
              }
            }
            internetP2P.SetRoutingHelper(list);
            internetP2P.Install(peer2);
          }
        }
      }

/*      if(!pflag && !p2flag){
        OlsrHelper olsr;
        Ipv4StaticRoutingHelper staticRouting;
        Ipv4ListRoutingHelper list;
        list.Add (staticRouting, 0);
        list.Add (olsr, 10);
        internetP2P.SetRoutingHelper (list); // has effect on the next Install ()
        internetP2P.Install(p2pNodes);
      }
      else if(pflag && !p2flag){
        OlsrHelper olsr;
        Ipv4StaticRoutingHelper staticRouting;
        Ipv4ListRoutingHelper list;
        list.Add (staticRouting, 0);
        list.Add (olsr, 10);
        internetP2P.SetRoutingHelper (list); // has effect on the next Install ()
        internetP2P.Install(peer2);
      }
      else if(!pflag && p2flag){
        OlsrHelper olsr;
        Ipv4StaticRoutingHelper staticRouting;
        Ipv4ListRoutingHelper list;
        list.Add (staticRouting, 0);
        list.Add (olsr, 10);
        internetP2P.SetRoutingHelper (list); // has effect on the next Install ()
        internetP2P.Install(peer);
      }
*/
      // Get then set addresses
      getAddresses(pt, peer, name_holder);
      Ptr<NetDevice> device = p2pDevices.Get (0);
      assignDeviceAddress(type, device);

      getAddresses(pt, peer2, name_holder2);
      device = p2pDevices.Get (1);
      assignDeviceAddress(type, device);

      p2p.EnableAsciiAll (stream);
      //internetP2P.EnableAsciiIpv4All (stream);
      if(pcap){
        p2p.EnablePcapAll (trace_prefix + "core-to-ns3-scenario");
      }

      cout << "\nCreating point-to-point connection with " << peer << " and " << peer2 << endl;
    }
//===================================================================
// WIFI
//-------------------------------------------------------------------
    if(type.compare("wireless") == 0){
      NS_LOG_INFO ("Create Wireless channel.");
      int j = 0;
      double dist = 0.0;
      //string ipv4_addr, ipv6_addr, mac_addr;
      peer = nod.second.get<string>("<xmlattr>.name");
      NodeContainer wifiNodes;
      NetDeviceContainer wifiDevices;
      InternetStackHelper wifiInternet;

      WifiHelper wifi;
      YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default();
      //YansWifiPhy wifiPhy;
      YansWifiChannelHelper wifiChannel;
      WifiMacHelper wifiMac;

      // working default
      wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
      wifiPhyHelper.SetChannel(wifiChannel.Create());

      string phyMode("DsssRate1Mbps");
      wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
      wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));
      
      // Used default values from Core's Kitchen Sink
      wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue (0.0));
      wifiPhy.SetRxGain(double(0.0));
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "RtsCtsThreshold", UintegerValue(0));
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "MaxSlrc", StringValue("0:3 1:3 2:3 3:3")); // default from Core
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "MaxSsrc", StringValue("0:3 1:3 2:3 3:3")); // default from Core
      wifiPhy.SetNTxPower(UintegerValue(0))); // default from Core in dBm
      wifiPhy.SetFrequency(UintegerValue(2347)); //default from Core in MHz
      Config::SetDefault ("ns3::Queue::MaxPackets", StringValue ("0:255 1:255 2:255 3:255");
      Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(4));
      Config::SetDefault ("ns3::Dcf::MinCw",StringValue ("0:32 1:32 2:16 3:8"); 
      Config::SetDefault ("ns3::EdcaTxopN::MinCw",StringValue ("0:32 1:32 2:16 3:8");
      Config::SetDefault ("ns3::Dcf::MaxCw",StringValue ("1024 1:1024 2:64 3:16");
      Config::SetDefault ("ns3::EdcaTxopN::MaxCw",StringValue ("1024 1:1024 2:64 3:16");
      Config::SetDefault ("ns3::Dcf::Aifs",StringValue ("0:2 1:2 2:2 3:1")); 
      Config::SetDefault ("ns3::EdcaTxopN::Aifs",StringValue ("0:2 1:2 2:2 3:1")); 
      Config::SetDefault("ns3::tdtbfqsFlowPerf_t::debtLimit", UintegerValue (10);
      
      // skipped setting default propagation loss model and
      // moved to top of attributes being set in BOOST section below
      
      // all attributes have already been set with default values
      BOOST_FOREACH(ptree::value_type const& p0, child.get_child("channel")){
        if(p0.first == "parameter"){
          if(p0.second.get<string>("<xmlattr>.name") == "mode"){
            switch(stoi(p0.second.data())){
              case 0 :
              case 1 : wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
                       break;
              case 2 :
              case 3 : wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
                       break;
              default : cout << "Incorrect wireless mode detected " << p0.second.data() << endl;
                        exit(-2);
            }
          }
          // need to establish distance before pathlossmode?
          if(p0.second.get<string>("<xmlattr>.name") == "distance"){
            dist = stod(p0.second.data());
            wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue (dist));
          }
          
          else if(p0.second.get<string>("<xmlattr>.name") == "pathlossmode"){
            if(p0.second.data() == "2ray"){
              // do we need the following if statement?
              if(dist > 0){
              wifiChannel.AddPropagationLoss("ns3::TwoRayGroundPropagationLossModel", "minDistance", UintegerValue (dist));
              }
              else if(p0.second.data() == "pathloss"){
              wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel", "minDistance", UintegerValue (dist));
              }
              else if(p0.second.data() == "freespace"){
              wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel", "minDistance", UintegerValue (dist));
              }
            }
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "unicastrate"){
            switch(stoi(p0.second.data())){
              case 1 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("DsssRate1Mbps"),
                                                                                     "ControlMode", StringValue ("DsssRate1Mbps"));
                       break;
              case 2 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("DsssRate2Mbps"),
                                                                                     "ControlMode", StringValue ("DsssRate2Mbps"));
                       break;
              case 3 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("DsssRate5_5Mbps"),
                                                                                     "ControlMode", StringValue ("DsssRate5_5Mbps"));
                       break;
              case 4 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("DsssRate11Mbps"),
                                                                                     "ControlMode", StringValue ("DsssRate11Mbps"));
                       break;
              case 5 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6Mbps"),
                                                                                     "ControlMode", StringValue ("OfdmRate6Mbps"));
                       break;
              case 6 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate9Mbps"),
                                                                                     "ControlMode", StringValue ("OfdmRate9Mbps"));
                       break;
              case 7 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate12Mbps"),
                                                                                     "ControlMode", StringValue ("OfdmRate12Mbps"));
                       break;
              case 8 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate18Mbps"),
                                                                                     "ControlMode", StringValue ("OfdmRate18Mbps"));
                       break;
              case 9 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate24Mbps"),
                                                                                     "ControlMode", StringValue ("OfdmRate24Mbps"));
                       break;
              case 10 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate36Mbps"),
                                                                                     "ControlMode", StringValue ("OfdmRate36Mbps"));
                       break;
              case 11 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate48Mbps"),
                                                                                     "ControlMode", StringValue ("OfdmRate48Mbps"));
                       break;
              case 12 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate54Mbps"),
                                                                                     "ControlMode", StringValue ("OfdmRate54Mbps"));
                       break;
              default : cout << "Incorrect wireless unicast rate detected " << p0.second.data() << endl;
                        exit(-3);
            }
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "rtsthreshold"){
            wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "RtsCtsThreshold", UintegerValue(stoi(p0.second.data())));
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "retrylimit"){
            wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "MaxSlrc", StringValue (p0.second.data()));
            wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "MaxSsrc", StringValue (p0.second.data()));
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "antennagain"){
            wifiPhy.SetRxGain(stod(p0.second.data()));
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "txpower"){
            wifiPhy.SetNTxPower(stod(p0.second.data()));
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "frequency"){
            wifiPhy.SetFrequency(stoi(p0.second.data()));
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "queuesize"){
            Config::SetDefault ("ns3::Queue::MaxPackets", StringValue (p0.second.data()));
            Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(4));
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "cwmin"){
            Config::SetDefault ("ns3::Dcf::MinCw",StringValue (p0.second.data())); 
            Config::SetDefault ("ns3::EdcaTxopN::MinCw",StringValue (p0.second.data()));
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "cwmax"){
            Config::SetDefault ("ns3::Dcf::MaxCw",StringValue (p0.second.data()));
            Config::SetDefault ("ns3::EdcaTxopN::MaxCw",StringValue (p0.second.data()));
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "aifs"){
            Config::SetDefault ("ns3::Dcf::Aifs",StringValue ("0:2 1:2 2:2 3:1")); 
            Config::SetDefault ("ns3::EdcaTxopN::Aifs",StringValue ("0:2 1:2 2:2 3:1")); 
          }
          else if(p0.second.get<string>("<xmlattr>.name") == "flowcontroltokens"){
            Config::SetDefault("ns3::tdtbfqsFlowPerf_t::debtLimit", UintegerValue (stoi(p0.second.data())));
          }
        }
      }

/*      // set emane ieee80211abg settings if any
      // <type domain="CORE">emane_ieee80211abg</type> - no direct equivalency for abg
      wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager");
      //wifi.SetStandard(WIFI_PHY_STANDARD_80211a); //default if not set anyway 
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("DsssRate1Mbps"),
                                                                    "ControlMode", StringValue ("DsssRate1Mbps"));

      wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
      wifiPhyHelper.SetChannel(wifiChannel.Create());
      //wifiPhyHelper.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11);
*/
      cout << "\nCreating new wlan network named " << peer << endl;
      // Go through peer list and add them to the network
      BOOST_FOREACH(ptree::value_type const& p, child.get_child("channel")){
        if(p.first == "member"){
          string name_holder, p2Type;
          name_holder = p.second.data();
          regex_search(name_holder, r_match, name);
          peer2 = r_match.str();
          // ignore interface channel name
          if(peer2.compare(peer) == 0){
            continue;
          }

        // set type for service matching
        BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
          if(pl1.first != "host" && pl1.first != "router"){
            continue;
          }
          if(pl1.second.get<string>("<xmlattr>.name") == peer2){
            p2Type = pl1.second.get<string>("type");
          }
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
            nodes.Add(peer2);
          }
/*          else{
            Ptr<Node> gateway = Names::Find<Node>(peer2);
            int nDev = gateway->GetNDevices();
            for(int inter = 0; inter < nDev; inter++){
              olsr.ExcludeInterface(gateway, inter);
            }
            //olsr.Create(gateway);
          }
*/

        OlsrHelper olsr;
        Ipv4StaticRoutingHelper staticRouting;
        Ipv4ListRoutingHelper list;
        bool applyDefaultServices2 = true;

        // get local services
        BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
          if(pl1.first != "host" && pl1.first != "router"){
            continue;
          }

          optional<const ptree&> service_exists = pl1.second.get_child_optional("CORE:services");
          if(service_exists){
            if(pl1.second.get<string>("<xmlattr>.name") == peer2 && !p2flag){
              BOOST_FOREACH(ptree::value_type const& pl2, pl1.second.get_child("CORE:services")){
                if(pl2.first == "service"){
                  if(pl2.second.get<string>("<xmlattr>.name") == "StaticRoute"){
                    list.Add (staticRouting, 0);
                  }
                  else if(pl2.second.get<string>("<xmlattr>.name") == "OLSR"){
                    Ptr<Node> gateway = Names::Find<Node>(peer2);
                    int nDev = gateway->GetNDevices();
                    for(int inter = 0; inter < nDev; inter++){
                      olsr.ExcludeInterface(gateway, inter);
                    }

                    list.Add(olsr, 10);
                  }
                }
              }
              wifiInternet.SetRoutingHelper(list); // has effect on the next Install ()
              wifiInternet.Install(peer2);
              applyDefaultServices2 = false;
            }
          }
        }
        // if there were no local, set default services according to type
        BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
          if(pl1.first == "CORE:defaultservices"){
            if(!p2flag && applyDefaultServices2 && pl1.second.get<string>("device.<xmlattr>.type") == p2Type){
              BOOST_FOREACH(ptree::value_type const& pl2, pl1.second.get_child("device")){
                if(pl2.first == "service"){
                  if(pl2.second.get<string>("<xmlattr>.name") == "StaticRoute"){
                    list.Add (staticRouting, 0);
                  }
                  else if(pl2.second.get<string>("<xmlattr>.name") == "OLSR"){
                    Ptr<Node> gateway = Names::Find<Node>(peer2);
                    int nDev = gateway->GetNDevices();
                    for(int inter = 0; inter < nDev; inter++){
                      olsr.ExcludeInterface(gateway, inter);
                    }

                    list.Add(olsr, 10);
                  }
                }
              }
              wifiInternet.SetRoutingHelper(list);
              wifiInternet.Install(peer2);
            }
          }
        }

          wifiMac.SetType("ns3::AdhocWifiMac");
          wifiDevices.Add(wifi.Install(wifiPhyHelper, wifiMac, peer2));
          cout << "Adding node " << peer2 << " to WLAN " << peer << endl;

          MobilityHelper mobility;
          mobility.Install(peer2);

          // Get then set address
          getAddresses(pt, peer2, name_holder);
          Ptr<NetDevice> device = wifiDevices.Get (j++);
          assignDeviceAddress(type, device);

          if(p2flag){
            // Obtain olsr::RoutingProtocol instance of gateway node
            Ptr<Ipv4> stack = Names::Find<Node>(peer2)->GetObject<Ipv4> ();
            Ptr<Ipv4RoutingProtocol> rp_Gw = (stack->GetRoutingProtocol ());
            Ptr<Ipv4ListRouting> lrp_Gw = DynamicCast<Ipv4ListRouting> (rp_Gw);

            //lrp_Gw->AddRoutingProtocol(olsr.Create(Names::Find<Node>(peer2)), 10);

            Ptr<olsr::RoutingProtocol> olsrrp_Gw;

            for (uint32_t i = 0; i < lrp_Gw->GetNRoutingProtocols ();  i++){
              int16_t priority;
              Ptr<Ipv4RoutingProtocol> temp = lrp_Gw->GetRoutingProtocol (i, priority);
              if (DynamicCast<olsr::RoutingProtocol> (temp)){
                olsrrp_Gw = DynamicCast<olsr::RoutingProtocol> (temp);
              }
            }
            // Specify the required associations directly.
            olsrrp_Gw->AddHostNetworkAssociation (Ipv4Address::GetAny (), Ipv4Mask ("255.255.0.0"));
          }
        }
      }
      wifiPhyHelper.EnableAsciiAll(stream);
      //wifiInternet.EnableAsciiIpv4All (stream);
      if(pcap){
        wifiPhyHelper.EnablePcapAll(trace_prefix + "core-to-ns3-scenario");
      }
    }
//===================================================================
// HUB/SWITCH
//-------------------------------------------------------------------
    else if(type.compare("hub") == 0 || type.compare("lanswitch") == 0){
      NS_LOG_INFO ("Create CSMA channel.");
      int j = 0;
      int k = 0; //index latest added extra hub/switch
      int hub_count = 0;
      int switch_count = 0;
      int nNodes = nodes.GetN();
      int bNodes = bridges.GetN();
      int hNodes = hubs.GetN();

      string tempName = "";
      string tempType = "";
      string ethId = "";
      string p2Type;
      //string ipv4_addr, ipv6_addr, mac_addr;

      peer = nod.second.get<string>("<xmlattr>.name");

      NodeContainer csmaNodes;
      NodeContainer bridgeNode;
      NodeContainer hubNode;
      NetDeviceContainer csmaDevices;
      InternetStackHelper internetCsma;

      // check for real type and if network includes more than one hub/switch
      BOOST_FOREACH(ptree::value_type const& p0, child){
        if(p0.first == "hub" && p0.second.get<string>("<xmlattr>.name") != peer){
          tempName = p0.second.get<string>("<xmlattr>.name");
          Ptr<Node> h1 = CreateObject<Node>();
          Names::Add(tempName, h1);
          hubs.Add(tempName);
          hNodes++;
          getXYPosition(p0.second.get<double>("point.<xmlattr>.lat"), 
                        p0.second.get<double>("point.<xmlattr>.lon"), x, y);

          AnimationInterface::SetConstantPosition(Names::Find<Node>(tempName), x, y);
        }
        else if(p0.first == "switch" && p0.second.get<string>("<xmlattr>.name") != peer){
          tempName = p0.second.get<string>("<xmlattr>.name");
          Ptr<Node> s1 = CreateObject<Node>();
          Names::Add(tempName, s1);
          bridges.Add(tempName);
          bNodes++;
          getXYPosition(p0.second.get<double>("point.<xmlattr>.lat"), 
                        p0.second.get<double>("point.<xmlattr>.lon"), x, y);

          AnimationInterface::SetConstantPosition(Names::Find<Node>(tempName), x, y);
        }
        else if(p0.first == "host" && p0.second.get<string>("<xmlattr>.name") != peer){
          if(p0.second.get<string>("type") == "hub"){
            tempName = p0.second.get<string>("<xmlattr>.name");
            Ptr<Node> h2 = CreateObject<Node>();
            Names::Add(tempName, h2);
            hubs.Add(tempName);
            hNodes++;
            getXYPosition(p0.second.get<double>("point.<xmlattr>.lat"), 
                          p0.second.get<double>("point.<xmlattr>.lon"), x, y);

            AnimationInterface::SetConstantPosition(Names::Find<Node>(tempName), x, y);
          }
          else if(p0.second.get<string>("type") == "lanswitch"){
            tempName = p0.second.get<string>("<xmlattr>.name");
            Ptr<Node> s2 = CreateObject<Node>();
            Names::Add(tempName, s2);
            bridges.Add(tempName);
            bNodes++;
            getXYPosition(p0.second.get<double>("point.<xmlattr>.lat"), 
                          p0.second.get<double>("point.<xmlattr>.lon"), x, y);

            AnimationInterface::SetConstantPosition(Names::Find<Node>(tempName), x, y);
          }
        }
        else if(p0.first == "hub" || p0.first == "switch" || p0.first == "host" && p0.second.get<string>("<xmlattr>.name") == peer){
          type = p0.second.get<string>("type");
          ethId = p0.second.get<string>("<xmlattr>.id");

          if(type.compare("lanswitch") == 0){
            bridgeNode.Create(1);
            Names::Add(peer, bridgeNode.Get(0));
            bridges.Add(peer);
            bNodes++;
          }
          else if(type.compare("hub") == 0){
            hubNode.Create(1);
            Names::Add(peer, hubNode.Get(0));
            hubs.Add(peer);
            hNodes++;
          }

          getXYPosition(p0.second.get<double>("point.<xmlattr>.lat"), 
                        p0.second.get<double>("point.<xmlattr>.lon"), x, y);

          AnimationInterface::SetConstantPosition(Names::Find<Node>(peer), x, y);
        }
      }

      cout << "\nCreating new "<< type <<" network named " << peer << endl;
      // Go through channels and add neighboring members to the network
      BOOST_FOREACH(ptree::value_type const& p0, child){
        if(p0.first == "channel"){
          string param, name_holder, memInterId, endInterId, linkName, tempPeer2;
          CsmaHelper csma;
          int state = 0;
          //  state 0 = clean,
          //        1 = something direct detected,
          //        2 = something indirect detected,
          //        3 = some router detected,
          //        4 = some hub/switch detected,
          //        5 = direct hub/switch found,
          //        6 = direct router/end device found,
          //        7 = indirect router/end device found,
          //        8 = indirect hub/switch found

          // if hub/switch to hub/switch, build through calling function
          // else if end divices, build through state 6 or 7
          BOOST_FOREACH(ptree::value_type const& tp0, p0.second){
            if(tp0.first == "member"){
              name_holder = tp0.second.data();
              // get first name for comparison
              regex_search(name_holder, r_match, name);
              tempPeer2 = r_match.str();
              // get member id to help identify link order
              regex_search(name_holder, r_match, interId);
              memInterId = r_match.str();
//cout << ethId << " " << tempPeer2 << " " << memInterId << " :" << name_holder << ": " << linkName << endl;

//=========================================================================
// Assume direct link to a switch or hub
              if(tempPeer2.compare(peer) == 0){
//-------------------------------------------------------------------------
// direct link found, if out of order, indirect link is possible
                if(ethId.compare(memInterId) == 0){
                  if(state == 0){
                    state = 1;
                  }
                  else if(state == 3){ // direct router identified
                    state = 6;
                  }
                  else if(state == 4){// direct hub/switch TODO
                    state = 5;
                  }
                  else{
                    // do nothing or error on state == 1
                  }
                }
//-------------------------------------------------------------------------
// possible indirect link or router found, if out of order, 
// direct link is possible
                else{
                  if(state == 0){ // some hub/switch found
                    linkName = memInterId.substr(peer.length() + 1);
                    state = 4;
                  }
                  else if(state == 1){ // direct hub/switch identified
                    linkName = memInterId.substr(peer.length() + 1);
                    state = 5;
                  }
                  else if(state == 3){// indirect router identified
                    linkName = memInterId.substr(peer.length() + 1);
                    state = 7;
                  }
                  else if(state == 8){//indirect hub/switch to hub/switch
                    // should already have built it
                    // do nothing
                  }
                  else{
                    // error
                  }
                }
              }
//=========================================================================
// Assume indirect link to a switch or hub, possible direct router
// if out of order
              else{
                if(state == 0){ // figure out if its router or not
                  linkName = memInterId.substr(tempPeer2.length() + 1);

                  // find if indirect hub/switch link
                  bool skip = false;
                  for(int i = 0; i < hNodes; i++){
                    if(tempPeer2.compare(Names::FindName(hubs.Get(i))) == 0){
                      peer2 = tempPeer2;
                      state = 8;
                      skip = true;
                      break;
                    }
                  }
                  for(int i = 0; i < bNodes && !skip; i++){
                    if(tempPeer2.compare(Names::FindName(bridges.Get(i))) == 0){
                      peer2 = tempPeer2;
                      state = 8;
                      skip = true;
                      break;
                    }
                  }
                  // else it is some router
                  if(!skip){
                    endInterId = name_holder;
                    peer2 = tempPeer2;
                    state = 3;
                  }
                  else{
                    break;
                  }
                }
                else if(state == 1){ // direct router identified
                  endInterId = name_holder;
                  state = 6;
                }
                else if(state == 4){//figure out if its a router or not
                  // find if indirect hub/switch link
                  bool skip = false;
                  for(int i = 0; i < hNodes; i++){
                    if(tempPeer2.compare(Names::FindName(hubs.Get(i))) == 0){
                      linkName = memInterId.substr(tempPeer2.length() + 1);
                      peer2 = tempPeer2;
                      state = 8;
                      skip = true;
                      break;
                    }
                  }
                  for(int i = 0; i < bNodes && !skip; i++){
                    if(tempPeer2.compare(Names::FindName(bridges.Get(i))) == 0){
                      linkName = memInterId.substr(tempPeer2.length() + 1);
                      peer2 = tempPeer2;
                      state = 8;
                      skip = true;
                      break;
                    }
                  }
                  // else it is indirect router
                  if(!skip){
                    endInterId = name_holder;
                    state = 7;
                  }
                  else{
                    break;
                  }
                }
                else{
                  // error
                  cerr << "Error: Topology for" << type << " " << peer << "could not be built: " << name_holder << endl;
                  return -1;
                }
              }
            }
          }

//=========================================================================
// Connect a bridge with a direct edge to controlling switch (a.k.a "peer")
//-------------------------------------------------------------------------
          if(state == 5){
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
            // set the link between node and hub/switch
            NetDeviceContainer link = csma.Install(NodeContainer(linkName, peer));

            cout << "Connection bridge " << peer << " to bridge " << linkName << endl;
            BridgeHelper bridgeHelper;
            bridgeHelper.Install(linkName, link.Get(0));
            bridgeHelper.Install(peer, link.Get(1));
          }
//=========================================================================
// Connect two bridges unrelated to controlling switch (a.k.a "peer")
//-------------------------------------------------------------------------
          else if(state == 8){
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
            // set the link between node and hub/switch
            NetDeviceContainer link = csma.Install(NodeContainer(linkName, peer2));

            cout << "Connection bridge " << peer2 << " to bridge " << linkName << endl;
            BridgeHelper bridgeHelper;
            bridgeHelper.Install(linkName, link.Get(0));
            bridgeHelper.Install(peer2, link.Get(1));
          }
//=========================================================================
// Connect a node with a direct edge to controlling switch (a.k.a "peer")
//-------------------------------------------------------------------------
          else if(state == 6){
            bool p2Nflag = false;
            for(int i = 0; i < nNodes; i++){
              if(peer2.compare(Names::FindName(nodes.Get(i))) == 0){
                p2Nflag = true;
                break;
              }
            }

            // set type for service matching
            BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
              if(pl1.first != "host" && pl1.first != "router"){
                continue;
              }
              if(pl1.second.get<string>("<xmlattr>.name") == peer2){
                p2Type = pl1.second.get<string>("type");
              }
            }

            if(!p2Nflag){
              csmaNodes.Create(1);
              Names::Add(peer2, csmaNodes.Get(csmaNodes.GetN() - 1));
              nodes.Add(peer2);
              //internetCsma.Install(peer2);
            }

            OlsrHelper olsr;
            Ipv4StaticRoutingHelper staticRouting;
            Ipv4ListRoutingHelper list;
            bool applyDefaultServices2 = true;

            // get local services
            BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
              if(pl1.first != "host" && pl1.first != "router"){
                continue;
              }

              optional<const ptree&> service_exists = pl1.second.get_child_optional("CORE:services");
              if(service_exists){
                if(!p2Nflag && pl1.second.get<string>("<xmlattr>.name") == peer2){
                  BOOST_FOREACH(ptree::value_type const& pl2, pl1.second.get_child("CORE:services")){
                    if(pl2.first == "service"){
                      Ipv4ListRoutingHelper list;

                      if(pl2.second.get<string>("<xmlattr>.name") == "StaticRoute"){
                        list.Add (staticRouting, 0);
                      }
                      else if(pl2.second.get<string>("<xmlattr>.name") == "OLSR"){
                        list.Add(olsr, 10);
                      }
                    }
                  }
                  internetCsma.SetRoutingHelper(list); // has effect on the next Install ()
                  internetCsma.Install(peer2);
                  applyDefaultServices2 = false;
                }
              }
            }
            // if there were no local, set default services according to type
            BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
              if(pl1.first == "CORE:defaultservices"){
                if(!p2Nflag && applyDefaultServices2 && pl1.second.get<string>("device.<xmlattr>.type") == p2Type){
                  BOOST_FOREACH(ptree::value_type const& pl2, pl1.second.get_child("device")){
                    if(pl2.first == "service"){
                      if(pl2.second.get<string>("<xmlattr>.name") == "StaticRoute"){
                        list.Add (staticRouting, 0);
                      }
                      else if(pl2.second.get<string>("<xmlattr>.name") == "OLSR"){
                        list.Add(olsr, 10);
                      }
                    }
                  }
                  internetCsma.SetRoutingHelper(list);
                  internetCsma.Install(peer2);
                }
              }
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
            // set the link between node and hub/switch
            NetDeviceContainer link = csma.Install(NodeContainer(peer2, peer));

            csmaDevices.Add(link.Get(0));

            BridgeHelper bridgeHelper;
            bridgeHelper.Install(peer, link.Get(1));

            // Get then set address
            getAddresses(pt, peer2, endInterId);
            Ptr<NetDevice> device = csmaDevices.Get (j++);
            assignDeviceAddress(type, device);

            csma.EnableAsciiAll(stream);
            if(pcap){
              csma.EnablePcapAll(trace_prefix + "core-to-ns3-scenario");
            }

            cout << "Adding node " << peer2 << " to a csma(" << type << ") " << peer << endl;
          }// end direct end device
//=========================================================================
// Connect a node to a bridge that is not the controlling switch (a.k.a "peer")
//-------------------------------------------------------------------------
          else if(state == 7){
            bool p2Nflag = false;
            for(int i = 0; i < nNodes; i++){
              if(peer2.compare(Names::FindName(nodes.Get(i))) == 0){
                p2Nflag = true;
                break;
              }
            }

            if(!p2Nflag){
              csmaNodes.Create(1);
              Names::Add(peer2, csmaNodes.Get(csmaNodes.GetN() - 1));
              nodes.Add(peer2);
              //internetCsma.Install(peer2);
            }

            OlsrHelper olsr;
            Ipv4StaticRoutingHelper staticRouting;
            Ipv4ListRoutingHelper list;
            bool applyDefaultServices2 = true;

            // get local services
            BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
              if(pl1.first != "host" && pl1.first != "router"){
                continue;
              }

              optional<const ptree&> service_exists = pl1.second.get_child_optional("CORE:services");
              if(service_exists){
                if(!p2Nflag && pl1.second.get<string>("<xmlattr>.name") == peer2){
                  BOOST_FOREACH(ptree::value_type const& pl2, pl1.second.get_child("CORE:services")){
                    if(pl2.first == "service"){
                      if(pl2.second.get<string>("<xmlattr>.name") == "StaticRoute"){
                        list.Add (staticRouting, 0);
                      }
                      else if(pl2.second.get<string>("<xmlattr>.name") == "OLSR"){
                        list.Add(olsr, 10);
                      }
                    }
                  }
                  internetCsma.SetRoutingHelper(list); // has effect on the next Install ()
                  internetCsma.Install(peer2);
                  applyDefaultServices2 = false;
                }
              }
            }
            // if there were no local, set default services according to type
            BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
              if(pl1.first == "CORE:defaultservices"){
                if(!p2Nflag && applyDefaultServices2 && pl1.second.get<string>("device.<xmlattr>.type") == p2Type){
                  BOOST_FOREACH(ptree::value_type const& pl2, pl1.second.get_child("device")){
                    if(pl2.first == "service"){
                      if(pl2.second.get<string>("<xmlattr>.name") == "StaticRoute"){
                        list.Add (staticRouting, 0);
                      }
                      else if(pl2.second.get<string>("<xmlattr>.name") == "OLSR"){
                        list.Add(olsr, 10);
                      }
                    }
                  }
                  internetCsma.SetRoutingHelper(list);
                  internetCsma.Install(peer2);
                }
              }
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
            // set the link between node and hub/switch
            NetDeviceContainer link = csma.Install(NodeContainer(peer2, linkName));

            csmaDevices.Add(link.Get(0));

            BridgeHelper bridgeHelper;
            bridgeHelper.Install(linkName, link.Get(1));

            // Get then set address
            getAddresses(pt, peer2, endInterId);
            Ptr<NetDevice> device = csmaDevices.Get (j++);
            assignDeviceAddress(type, device);

            csma.EnableAsciiAll(stream);
            if(pcap){
              csma.EnablePcapAll(trace_prefix + "core-to-ns3-scenario");
            }

            cout << "Adding node " << peer2 << " to a csma(" << type << ") " << linkName << endl;
          }// end indirect end divice
        }
      //internetCsma.EnableAsciiIpv4All(stream);
      }
    }
  }
  cout << "\nCORE topology imported..." << endl; 
//====================================================
// END OF TOPOLOGY BUILDER
//====================================================


//====================================================
//create applications if given
//----------------------------------------------------
  if(!apps_file.empty()){
    ptree a_pt;

    try{
      read_xml(apps_file, a_pt);
      createApp(a_pt, duration);
    } catch(const boost::property_tree::xml_parser::xml_parser_error& ex){
      cerr << "error in file " << ex.filename() << " line " << ex.line() << endl;
      exit(-5);
    }
  }

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
    cout << nodeName << " with id " << n << " (rouge)";

    getXYPosition(nod.second.get<double>("point.<xmlattr>.lat"), 
                  nod.second.get<double>("point.<xmlattr>.lon"), x, y);

    AnimationInterface::SetConstantPosition(Names::Find<Node>(nodeName), x, y);

    cout << " set..." << endl;
  }

  if(extra > 0){
    cout << extra << " rouge (unconnected) node(s) detected!" << endl;
  }

  // Create NetAnim xml file
  AnimationInterface anim("NetAnim-core-to-ns3.xml");
  anim.EnablePacketMetadata(true);
  //anim.EnableIpv4RouteTracking ("testRouteTrackingXml.xml", Seconds(1.0), Seconds(3.0), Seconds(5));

  // install ns2 mobility script
  ns2.Install();

  // Turn on global static routing so we can actually be routed across the network.
  //Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Trace routing tables 
  Ipv4GlobalRoutingHelper g;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (trace_prefix + "core2ns3-global-routing.routes", std::ios::out);
  g.PrintRoutingTableAllAt (Seconds (duration), routingStream);

  // Flow monitor
  FlowMonitorHelper flowHelper;
  flowHelper.InstallAll ();


  // Configure callback for logging
//  Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange",
//                   MakeBoundCallback (&CourseChange, &os));

  cout << "Simulating..." << endl;

  Simulator::Stop (Seconds (duration));
  // traces
  //ndn::L3RateTracer::InstallAll ((trace_prefix + "/rate-trace.txt").c_str(), Seconds (SIMULATION_RUNTIME + 0.9999));
  //L2RateTracer::InstallAll ((trace_prefix + "/drop-trace.txt").c_str(), Seconds (SIMULATION_RUNTIME + 0.999));
  //ndn::CsTracer::InstallAll ((trace_prefix + "/cs-trace.txt").c_str(), Seconds (SIMULATION_RUNTIME + 0.9999));
  //ndn::AppDelayTracer::InstallAll ((trace_prefix + "/app-delays-trace.txt").c_str());
  //wifiPhy.EnablePcap ((trace_prefix + "/wifi.pcap").c_str(), wifi_devices);

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  
  cout << "Done." << endl;

  return 0;
}






