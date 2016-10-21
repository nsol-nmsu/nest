
#include "ns3/core-to-ns3-helper.h"
//things from namespace std
/*
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
*/
NS_LOG_COMPONENT_DEFINE ("CORE_to_NS3_scenario");


/////////////////////////////////////////////////////////
// Parse CORE XML and create an ns3 scenario file from it
/////////////////////////////////////////////////////////
int main (int argc, char *argv[]) {
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1024));

  // The below value configures the default behavior of global routing.
  // By default, it is disabled.  To respond to interface events, set to true
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));

  // config locals
  double duration = 10.0;
  regex cartesian("[0-9]+");
  string peer, peer2, type;
  string topo_name = "",
         ns2_mobility = "/dev/null",
         trace_prefix = "core2ns3_Logs/";

  struct stat st;

  // simulation locals
  NodeContainer nodes;
  NodeContainer bridges;

  // read command-line parameters
  CommandLine cmd;
  cmd.AddValue("topo", "Path to intermediate topology file", topo_name);
  cmd.AddValue("ns2","Ns2 mobility script file", ns2_mobility);
  cmd.AddValue("duration","Duration of Simulation",duration);
  cmd.AddValue ("traceDir", "Directory in which to store trace files", trace_prefix);
  cmd.Parse (argc, argv);

  // Check command line arguments
  if (topo_name.empty ()){
    std::cout << "Usage of " << argv[0] << " :\n\n"
    "./waf --run \"scratch/xml_to_ns3_scenario"
    " --topo=imn2ns3/imn_sample_files/sample1.xml"
    " --Ns2=imn2ns3/imn_sample_files/sample1.ns_movements"
    " --traceDir=core2ns3_Logs/"
    //" --logFile=ns2-mob.log"
    " --duration=27.0\" \n\n";

    return 0;
  }

  string trace_check = trace_prefix;

  // Verify that trace directory exists
  if(trace_check.at(trace_check.length() - 1) == '/') // trim trailing slash
  	trace_check = trace_check.substr(0, trace_check.length() - 1);
  if(stat(trace_check.c_str(), &st) != 0 || (st.st_mode & S_IFDIR) == 0){
  	std::cerr << "Error: trace directory " << trace_check << " doesn't exist!" << std::endl;
  	return -1;
  }
  else{
  	std::cout << "Writing traces to directory `" << trace_check << "`." << std::endl;
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
      NS_LOG_INFO ("Create Point to Point channel.");
      NodeContainer p2pNodes;
      NetDeviceContainer p2pDevices;
      PointToPointHelper p2p;
      string ipv4_addr, ipv6_addr, mac_addr;
      string ipv4_addr2, ipv6_addr2, mac_addr2;
      string name_holder, name_holder2;
      bool fst = true;
      int band = 0;
      int dela = 0;

      optional<const ptree&> channel_exists = child.get_child_optional("channel");

      if(!channel_exists){ // CORE occasionally produces empty p2p networks, we skip them
        continue;
      }

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
      assignDeviceAddress(type, device);

      getAddresses(pt, peer2, name_holder2);
      device = p2pDevices.Get (1);
      assignDeviceAddress(type, device);

      p2p.EnableAsciiAll (stream);
      //internetP2P.EnableAsciiIpv4All (stream);
      p2p.EnablePcapAll (trace_prefix + "core-to-ns3-scenario");

      cout << "\nCreating point-to-point connection with " << peer << " and " << peer2 << endl;
    }
//====================================
//=================Wifi===============
//====================================
    if(type.compare("wireless") == 0){
      NS_LOG_INFO ("Create Wireless channel.");
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
          assignDeviceAddress(type, device);
        }
      }
      wifiPhy.EnableAsciiAll(stream);
      //wifiInternet.EnableAsciiIpv4All (stream);
      wifiPhy.EnablePcapAll(trace_prefix + "core-to-ns3-scenario");
    }
//==========================================
//=================Hub/Switch===============
//==========================================
    else if(type.compare("hub") == 0 || type.compare("lanswitch") == 0){
      NS_LOG_INFO ("Create CSMA channel.");
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

  NS_ABORT_MSG_IF (peer2.compare(peer) == 0, "CSMA builder : Bridge to bridge broadcast storm detected.");

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
          assignDeviceAddress(type, device);

          csma.EnableAsciiAll(stream);
          csma.EnablePcapAll(trace_prefix + "core-to-ns3-scenario");

          cout << "Adding node " << peer2 << " to a csma(" << type << ") " << peer << endl;
        }

      //internetCsma.EnableAsciiIpv4All(stream);
      }

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


  createApp(UDP, pt, duration);



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

  AnimationInterface anim("NetAnim-core-to-ns3.xml");
  anim.EnablePacketMetadata(true);
  //anim.EnableIpv4RouteTracking ("testRouteTrackingXml.xml", Seconds(1.0), Seconds(3.0), Seconds(5));

  // install ns2 mobility script
  ns2.Install();

  // Turn on global static routing so we can actually be routed across the network.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

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







