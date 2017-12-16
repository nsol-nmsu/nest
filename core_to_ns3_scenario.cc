
#include "ns3/core-to-ns3-helper.h"

NS_LOG_COMPONENT_DEFINE ("CORE_to_NS3_scenario");

//###################################################################
// Parse CORE XML and create an ns3 scenario file from it
//###################################################################
int main (int argc, char *argv[]) {
  bool global_is_safe = true;

  // config locals
  bool pcap = false;
  bool random = false;
  bool real_time = false;
  double duration = 10.0;

  string peer, peer2, type;
  string topo_name = "",
         apps_file = "",
         ns2_mobility = "/dev/null",
         trace_prefix = "core2ns3_Logs/",
         infrastructure = "",
         access_point = "";

  struct stat st;
  smatch r_match;

  // simulation locals
  NodeContainer nodes;
  NodeContainer bridges;
  NodeContainer hubs;

  NetDeviceContainer nd;

  // read command-line parameters
  CommandLine cmd;
  cmd.AddValue("topo", "Path to intermediate topology file", topo_name);
  cmd.AddValue("apps", "Path to application generator file", apps_file);
  cmd.AddValue("ns2","Ns2 mobility script file", ns2_mobility);
  cmd.AddValue("duration","Duration of Simulation",duration);
  cmd.AddValue("pcap","Enable pcap files",pcap);
  cmd.AddValue("rt","Enable real time simulation",real_time);
  cmd.AddValue("random","Enable random simulation",random);
  cmd.AddValue("infra","Declare WLAN as infrastructure",infrastructure);
  cmd.AddValue("ap","Declare node as an access point/gateway",access_point);
  cmd.AddValue ("traceDir", "Directory in which to store trace files", trace_prefix);
  cmd.Parse (argc, argv);

  // Check command line arguments
  if (topo_name.empty ()){
    std::cout << "Usage of " << argv[0] << " :\n\n"
    "./waf --run \"scratch/core_to_ns3_scenario"
    " --topo=path/to/CORE/files.xml"
    " --apps=path/to/app/files.xml"
    " --ns2=path/to/NS2-mobility/files"
    " --traceDir=core2ns3_Logs/"
    " --pcap=[true/false]"
    " --rt=[true/false]"
    " --random=[true/false]"
    " --infra=wlan1::wlan2::..."
    " --ap=n1::n2::..."
    //" --logFile=ns2-mob.log"
    " --duration=[float]\" \n\n";

    return 0;
  }

  // if bandwidth is not provided, assume CORE definition of 0 == unlimited
  // or in this case, max unsigned 64bit integer value 18446744073709551615
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", DataRateValue(ULLONG_MAX));
  Config::SetDefault("ns3::CsmaChannel::DataRate", DataRateValue(ULLONG_MAX));

  // testing
  //Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(2));

  if(real_time){
    GlobalValue::Bind ("SimulatorImplementationType", 
                       StringValue ("ns3::RealtimeSimulatorImpl"));
  }

  if(random){
    SeedManager::SetSeed(10);
    srand(time(NULL));
    int runNumber = rand() % 100;
    SeedManager::SetRun(runNumber);
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

  //AsciiTraceHelper ascii;
  //Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream (trace_prefix + "core-to-ns3.tr");

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
  refZoneNum;
  refUTMZone;

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

      // CORE occasionally produces empty p2p networks, we skip them
      if(!channel_exists){
        continue;
      }

      NS_LOG_INFO ("Create Point to Point channel.");
      NodeContainer p2pNodes;
      NetDeviceContainer p2pDevices;
      PointToPointHelper p2p;

      string name_holder, name_holder2, pType, p2Type;
      bool fst = true;

      // grab the node names from net
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

      p2p.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1000));

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
            double percent = (stod(p0.second.data()) / 100.0);
            Ptr<RateErrorModel> rem = CreateObjectWithAttributes<RateErrorModel>("ErrorRate", DoubleValue(percent),
                                                                                 "ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
            p2p.SetDeviceAttribute("ReceiveErrorModel",PointerValue(rem));
          }
        }
      }

      p2pDevices.Add(p2p.Install(peer, peer2));

      // add internet stack if not yet created, add routing if found
      if(!pflag){
        getRoutingProtocols(pt, peer, pType);
      }
      if(!p2flag){
        getRoutingProtocols(pt, peer2, p2Type);
      }

      // Get then set addresses
      getAddresses(pt, peer, name_holder);
      Ptr<NetDevice> device = p2pDevices.Get (0);
      assignDeviceAddress(device);

      getAddresses(pt, peer2, name_holder2);
      device = p2pDevices.Get (1);
      assignDeviceAddress(device);

      if(pcap){
        nd.Add(p2pDevices);
        //p2p.EnableAsciiAll (stream);
        //p2p.EnablePcapAll (trace_prefix + "core-to-ns3");
      }

      cout << "\nCreating point-to-point connection with " << peer << " and " << peer2 << endl;
    }
//===================================================================
// WIFI
//-------------------------------------------------------------------
    if(type.compare("wireless") == 0){
      NS_LOG_INFO ("Create Wireless channel.");
      //global_is_safe = false;
      int j = 0;
      double dist = 0.0;
      bool twoRay_set = false;
      bool freespace_set = false;
      bool oneWarning = true;

      peer = nod.second.get<string>("<xmlattr>.name");
      NodeContainer wifiNodes;
      NetDeviceContainer wifiDevices;

      WifiHelper wifi;
      YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default();
      YansWifiChannelHelper wifiChannel;
      WifiMacHelper wifiMac;

      wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

      Config::SetDefault("ns3::WifiMacQueue::MaxPacketNumber", UintegerValue (1000)); 

      BOOST_FOREACH(ptree::value_type const& p0, child.get_child("channel")){
        if(p0.first == "type" && p0.second.data() == "basic_range"){
          BOOST_FOREACH(ptree::value_type const& p1, child.get_child("channel")){
            if(p1.first == "parameter"){
              if(p1.second.get<string>("<xmlattr>.name") == "range"){
                dist = stod(p1.second.data());
              }
              else if(p1.second.get<string>("<xmlattr>.name") == "bandwidth"){
                int bw = stoi(p1.second.data());

                if(bw <= 1000000){
                  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
                  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                "DataMode", StringValue ("DsssRate1Mbps"),
                                                "ControlMode", StringValue ("DsssRate1Mbps"));
                }
                else if(bw <= 2000000){
                  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
                  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                "DataMode", StringValue ("DsssRate2Mbps"),
                                                "ControlMode", StringValue ("DsssRate2Mbps"));
                }
                else if(bw <= 5000000){
                  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
                  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                "DataMode", StringValue ("DsssRate5_5Mbps"),
                                                "ControlMode", StringValue ("DsssRate5_5Mbps"));
                }
                else if(bw <= 6000000){
                  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
                  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                "DataMode", StringValue ("OfdmRate6Mbps"),
                                                "ControlMode", StringValue ("OfdmRate6Mbps"));
                }
                else if(bw <= 9000000){
                  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
                  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                "DataMode", StringValue ("OfdmRate9Mbps"),
                                                "ControlMode", StringValue ("OfdmRate9Mbps"));
                }
                else if(bw <= 11000000){
                  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
                  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                "DataMode", StringValue ("DsssRate11Mbps"),
                                                "ControlMode", StringValue ("DsssRate11Mbps"));
                }
                else if(bw <= 12000000){
                  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
                  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                "DataMode", StringValue ("OfdmRate12Mbps"),
                                                "ControlMode", StringValue ("OfdmRate12Mbps"));
                }
                else if(bw <= 18000000){
                  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
                  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                "DataMode", StringValue ("OfdmRate18Mbps"),
                                                "ControlMode", StringValue ("OfdmRate18Mbps"));
                }
                else if(bw <= 24000000){
                  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
                  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                "DataMode", StringValue ("OfdmRate24Mbps"),
                                                "ControlMode", StringValue ("OfdmRate24Mbps"));
                }
                else if(bw <= 36000000){
                  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
                  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                "DataMode", StringValue ("OfdmRate36Mbps"),
                                                "ControlMode", StringValue ("OfdmRate36Mbps"));
                }
                else if(bw <= 48000000){
                  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
                  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                "DataMode", StringValue ("OfdmRate48Mbps"),
                                                "ControlMode", StringValue ("OfdmRate48Mbps"));
                }
                else if(bw <= 54000000){
                  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
                  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                "DataMode", StringValue ("OfdmRate54Mbps"),
                                                "ControlMode", StringValue ("OfdmRate54Mbps"));
                }
                else{
                  cout << "Incorrect wireless unicast rate detected " << p1.second.data() << endl;
                  exit(-3);
                }
              }
            }
          }
          break;
        }
        else if(p0.first == "type" && p0.second.data() == "emane_ieee80211abg"){
          BOOST_FOREACH(ptree::value_type const& p1, p0.second){
            if(p1.first == "parameter"){
              if(p1.second.get<string>("<xmlattr>.name") == "mode"){
                switch(stoi(p1.second.data())){
                  case 0 :
                  case 1 : wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
                           break;
                  case 2 :
                  case 3 : wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
                           break;
                  default : cout << "Incorrect wireless mode detected " << p1.second.data() << endl;
                            exit(-2);
                }
              }
              else if(p1.second.get<string>("<xmlattr>.name") == "distance"){
                // capture distance in meters and conver to pixel distance
                // to match CORE scenario
                dist = 100.0 * (stod(p1.second.data()) / refScale);
              }
              else if(p1.second.get<string>("<xmlattr>.name") == "unicastrate"){
                switch(stoi(p1.second.data())){
                  case 1 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                         "DataMode", StringValue ("DsssRate1Mbps"),
                                                         "ControlMode", StringValue ("DsssRate1Mbps"));
                           break;
                  case 2 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                         "DataMode", StringValue ("DsssRate2Mbps"),
                                                         "ControlMode", StringValue ("DsssRate2Mbps"));
                           break;
                  case 3 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                         "DataMode", StringValue ("DsssRate5_5Mbps"),
                                                         "ControlMode", StringValue ("DsssRate5_5Mbps"));
                           break;
                  case 4 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                         "DataMode", StringValue ("DsssRate11Mbps"),
                                                         "ControlMode", StringValue ("DsssRate11Mbps"));
                           break;
                  case 5 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                         "DataMode", StringValue ("OfdmRate6Mbps"),
                                                         "ControlMode", StringValue ("OfdmRate6Mbps"));
                           break;
                  case 6 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                         "DataMode", StringValue ("OfdmRate9Mbps"),
                                                         "ControlMode", StringValue ("OfdmRate9Mbps"));
                           break;
                  case 7 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                         "DataMode", StringValue ("OfdmRate12Mbps"),
                                                         "ControlMode", StringValue ("OfdmRate12Mbps"));
                           break;
                  case 8 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                         "DataMode", StringValue ("OfdmRate18Mbps"),
                                                         "ControlMode", StringValue ("OfdmRate18Mbps"));
                           break;
                  case 9 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                         "DataMode", StringValue ("OfdmRate24Mbps"),
                                                         "ControlMode", StringValue ("OfdmRate24Mbps"));
                           break;
                  case 10 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                          "DataMode", StringValue ("OfdmRate36Mbps"),
                                                          "ControlMode", StringValue ("OfdmRate36Mbps"));
                           break;
                  case 11 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                          "DataMode", StringValue ("OfdmRate48Mbps"),
                                                          "ControlMode", StringValue ("OfdmRate48Mbps"));
                           break;
                  case 12 : wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                          "DataMode", StringValue ("OfdmRate54Mbps"),
                                                          "ControlMode", StringValue ("OfdmRate54Mbps"));
                           break;
                  default : cout << "Incorrect wireless unicast rate detected " << p1.second.data() << endl;
                            exit(-3);
                }
              }
              else if(p1.second.get<string>("<xmlattr>.name") == "rtsthreshold"){
                wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                              "RtsCtsThreshold", UintegerValue(stoi(p1.second.data())));
              }
              else if(p1.second.get<string>("<xmlattr>.name") == "retrylimit"){
                wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                              "MaxSlrc", StringValue (p1.second.data()));
                wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                              "MaxSsrc", StringValue (p1.second.data()));
              }
              else if(p1.second.get<string>("<xmlattr>.name") == "antennagain"){
                wifiPhyHelper.Set("RxGain", DoubleValue(stod(p1.second.data())));
              }
              else if(p1.second.get<string>("<xmlattr>.name") == "txpower"){
                wifiPhyHelper.Set("TxPowerStart", DoubleValue(stod(p1.second.data())));
                wifiPhyHelper.Set("TxPowerEnd", DoubleValue(stod(p1.second.data())));
              }
              else if(p1.second.get<string>("<xmlattr>.name") == "frequency"){
                string tempHz, unit;
                uint32_t Hz = 0;
                tempHz = p1.second.data();
                regex_search(tempHz, r_match, category);
                Hz = stoi(r_match.str());
                unit = r_match.suffix().str();

                if(unit == "G"){
                  Hz = Hz * 1000;
                }
                else if(unit == "M"){
                  // keep value
                }
                else{
                  Hz = 0;
                }

                wifiPhyHelper.Set("Frequency", UintegerValue(Hz));
              }
              else if(p1.second.get<string>("<xmlattr>.name") == "pathlossmode"){
                if(p1.second.data() == "2ray"){
                  twoRay_set = true;
                }
                else if(p1.second.data() == "freespace"){
                  freespace_set = true;
                }
              }
              else if(p1.second.get<string>("<xmlattr>.name") == "queuesize"){
                string tempQ, Q;
                tempQ = p1.second.data();
                regex_search(tempQ, r_match, rateUnit);
                Q = r_match.str();

                Config::SetDefault("ns3::WifiMacQueue::MaxPacketNumber", UintegerValue (atoi(Q.c_str()+1))); 
              }
              else if(p0.second.get<string>("<xmlattr>.name") == "cwmin"){
                string tempCw, Cw;
                tempCw = p1.second.data();
                regex_search(tempCw, r_match, rateUnit);
                Cw = r_match.str();

                Config::SetDefault("ns3::Dcf::MinCw", UintegerValue (atoi(Cw.c_str()+1))); 
                Config::SetDefault("ns3::EdcaTxopN::MinCw", UintegerValue (atoi(Cw.c_str()+1)));
              }
              else if(p0.second.get<string>("<xmlattr>.name") == "cwmax"){
                string tempCw, Cw;
                tempCw = p1.second.data();
                regex_search(tempCw, r_match, rateUnit);
                Cw = r_match.str();

                Config::SetDefault("ns3::Dcf::MaxCw", UintegerValue (atoi(Cw.c_str()+1)));
                Config::SetDefault("ns3::EdcaTxopN::MaxCw", UintegerValue (atoi(Cw.c_str()+1)));
              }
              else if(p0.second.get<string>("<xmlattr>.name") == "aifs"){
                string tempAifs, Aifs;
                tempAifs = p1.second.data();
                regex_search(tempAifs, r_match, rateUnit);
                Aifs = r_match.str();

                Config::SetDefault("ns3::Dcf::Aifs", UintegerValue (atoi(Aifs.c_str()+1))); 
                Config::SetDefault("ns3::EdcaTxopN::Aifs", UintegerValue (atoi(Aifs.c_str()+1))); 
              }
          //else if(p0.second.get<string>("<xmlattr>.name") == "flowcontroltokens"){
            //Config::SetDefault("ns3::tdtbfqsFlowPerf_t::debtLimit", UintegerValue (stoi(p0.second.data())));
          //}
            }
          }
          break;
        }
      }

      // set propagation
      if(dist > 0.0){
        if(twoRay_set){
          wifiChannel.AddPropagationLoss("ns3::TwoRayGroundPropagationLossModel",
                                         "minDistance", UintegerValue (dist));
        }
        else if(freespace_set){
          wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
        }
        else{
          wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel",
                                         "MaxRange", DoubleValue (dist));
        }
      }
      else{
        if(twoRay_set){
          wifiChannel.AddPropagationLoss("ns3::TwoRayGroundPropagationLossModel");
        }
        else{
          wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
        }
      }

      wifiPhyHelper.SetChannel(wifiChannel.Create());
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

          // add internet stack if not yet created, add routing if found
          if(!p2flag){
            getRoutingProtocols(pt, peer2, p2Type);

            if(oneWarning){
              cout << "Note: OSPFv2 routing unavailable for wireless nodes. \n"
                   << "      NS-3 recommends using OLSR if routing is of no consequence." << endl; 
              oneWarning = false;
            }
          }
          bool gateway = false;
          if(!infrastructure.empty()){
            // if peer2 in access_point, go to else
            if(access_point.find(peer2) == string::npos){
              // setup sta.
              wifiMac.SetType ("ns3::StaWifiMac",
                               "Ssid", SsidValue (peer),
                               "ActiveProbing", BooleanValue (false));
            }
            else{
              // setup ap.
              wifiMac.SetType ("ns3::ApWifiMac",
                               "Ssid", SsidValue (peer),
                               "BeaconGeneration", BooleanValue (true),
                               "BeaconInterval", TimeValue(Seconds(2.5)));
              gateway = true;
              cout << peer2 << " declared as access point." << endl;
            }
          }
          else{
            // setup adhoc.
            wifiMac.SetType("ns3::AdhocWifiMac");
          }

          wifiDevices.Add(wifi.Install(wifiPhyHelper, wifiMac, peer2));
          cout << "Adding node " << peer2 << " to WLAN " << peer << endl;

          MobilityHelper mobility;
          mobility.Install(peer2);

          // Get then set address
          getAddresses(pt, peer2, name_holder);
          Ptr<NetDevice> device = wifiDevices.Get (j++);
          assignDeviceAddress(device);


          // attempt to define gateway TODO
          if(gateway){
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
            olsrrp_Gw->AddHostNetworkAssociation (Ipv4Address::GetAny (), Ipv4Mask ("0.0.0.0"));
          }

        }
      }

      if(pcap){
        nd.Add(wifiDevices);
        //wifiPhyHelper.EnableAsciiAll(stream);
        //wifiPhyHelper.EnablePcapAll(trace_prefix + "core-to-ns3");
      }
    }
//===================================================================
// HUB/SWITCH
//-------------------------------------------------------------------
    else if(type.compare("hub") == 0 || type.compare("lanswitch") == 0){
      NS_LOG_INFO ("Create CSMA channel.");

      int nNodes = nodes.GetN();
      int bNodes = bridges.GetN();
      int hNodes = hubs.GetN();

      string tempName = "";
      string ethId = "";
      string p2Type;

      peer = nod.second.get<string>("<xmlattr>.name");

      NodeContainer csmaNodes;
      NodeContainer bridgeNode;
      NodeContainer hubNode;
      NetDeviceContainer csmaDevices;
      NetDeviceContainer bridgeDevices;

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
        else if((p0.first == "hub" || p0.first == "switch" || p0.first == "host") && (p0.second.get<string>("<xmlattr>.name") == peer)){
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
          //        6 = direct router/end-device found,
          //        7 = indirect router/end-device found,
          //        8 = indirect hub/switch found

          // if hub/switch to hub/switch, build through state 6 or 8
          // else if end-divices, build through state 6 or 7
          BOOST_FOREACH(ptree::value_type const& tp0, p0.second){
            if(tp0.first == "member"){
              name_holder = tp0.second.data();
              // get first name for comparison
              regex_search(name_holder, r_match, name);
              tempPeer2 = r_match.str();
              // get member id to help identify link order
              regex_search(name_holder, r_match, interId);
              memInterId = r_match.str();

//=========================================================================
// Assume direct link to a switch or hub
              if(tempPeer2.compare(peer) == 0){
//-------------------------------------------------------------------------
// direct link found, if out of order, indirect link is possible
                if(ethId.compare(memInterId) == 0){
                  if(state == 0)     { state = 1; }
                  else if(state == 3){ state = 6; } // direct router identified
                  else if(state == 4){ state = 5; } // direct hub/switch TODO
                  else if(state == 1){              // error on state == 1
                    cerr << "Error: Topology for" << type << " " << peer << "could not be built: " << name_holder << endl;
                    return -1;
                  }
                }
//-------------------------------------------------------------------------
// possible indirect link or router found, if out of order, 
// direct link is possible
                else{
                  if(state == 0){      // some hub/switch found
                    linkName = memInterId.substr(peer.length() + 1);
                    state = 4;
                  }
                  else if(state == 1){ // direct hub/switch identified
                    linkName = memInterId.substr(peer.length() + 1);
                    state = 5;
                  }
                  else if(state == 3){ // indirect router identified
                    linkName = memInterId.substr(peer.length() + 1);
                    state = 7;
                  }
                  else{
                    // error
                    cerr << "Error: Topology for" << type << " " << peer << "could not be built: " << name_holder << endl;
                    return -1;
                  }
                }
              }// end assumed direct
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
                  cerr << "Error: Topology for" << type << " " << peer << "could not be built: " << name_holder << endl;
                  return -1;
                }
              }// end assumed indirect
            }// end if member
          }// end boost for loop

//=========================================================================
// Connect a bridge with a direct edge to controlling bridge (a.k.a "peer")
//-------------------------------------------------------------------------
          if(state == 5){
            csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1000));

            BOOST_FOREACH(ptree::value_type const& p1, p0.second){
              if(p1.first == "parameter"){
                if(p1.second.get<string>("<xmlattr>.name") == "bw"){
                  csma.SetChannelAttribute("DataRate", DataRateValue(stoi(p1.second.data())));
                }
                else if(p1.second.get<string>("<xmlattr>.name") == "delay"){
                  csma.SetChannelAttribute("Delay",TimeValue(MicroSeconds(stoi(p1.second.data()))));
                }
                else if(p1.second.get<string>("<xmlattr>.name") == "loss"){
                  double percent = (stod(p1.second.data()) / 100.0);
                  Ptr<RateErrorModel> rem = CreateObjectWithAttributes<RateErrorModel>("ErrorRate", DoubleValue(percent),
                                                                                       "ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
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
// Connect two bridges unrelated to controlling bridge (a.k.a "peer")
//-------------------------------------------------------------------------
          else if(state == 8){
            csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1000));

            BOOST_FOREACH(ptree::value_type const& p1, p0.second){
              if(p1.first == "parameter"){
                if(p1.second.get<string>("<xmlattr>.name") == "bw"){
                  csma.SetChannelAttribute("DataRate", DataRateValue(stoi(p1.second.data())));
                }
                else if(p1.second.get<string>("<xmlattr>.name") == "delay"){
                  csma.SetChannelAttribute("Delay",TimeValue(MicroSeconds(stoi(p1.second.data()))));
                }
                else if(p1.second.get<string>("<xmlattr>.name") == "loss"){
                  double percent = (stod(p1.second.data()) / 100.0);
                  Ptr<RateErrorModel> rem = CreateObjectWithAttributes<RateErrorModel>("ErrorRate", DoubleValue(percent),
                                                                                       "ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
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
            }

            // add internet stack if not yet created, add routing if found
            if(!p2Nflag){
              getRoutingProtocols(pt, peer2, p2Type);
            }

            csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1000));

            BOOST_FOREACH(ptree::value_type const& p1, p0.second){
              if(p1.first == "parameter"){
                if(p1.second.get<string>("<xmlattr>.name") == "bw"){
                  csma.SetChannelAttribute("DataRate", DataRateValue(stoi(p1.second.data())));
                }
                else if(p1.second.get<string>("<xmlattr>.name") == "delay"){
                  csma.SetChannelAttribute("Delay",TimeValue(MicroSeconds(stoi(p1.second.data()))));
                }
                else if(p1.second.get<string>("<xmlattr>.name") == "loss"){
                  double percent = (stod(p1.second.data()) / 100.0);
                  Ptr<RateErrorModel> rem = CreateObjectWithAttributes<RateErrorModel>("ErrorRate", DoubleValue(percent),
                                                                                       "ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
                  csma.SetDeviceAttribute("ReceiveErrorModel",PointerValue(rem));
                }
              }
            }
            // set the link between node and hub/switch
            NetDeviceContainer link = csma.Install(NodeContainer(peer2, peer));

            csmaDevices.Add(link.Get(0));
            bridgeDevices.Add(link.Get(1));
            //BridgeHelper bridgeHelper;
            //bridgeHelper.Install(peer, link.Get(1));

            // Get then set address
            getAddresses(pt, peer2, endInterId);
            Ptr<NetDevice> device = link.Get(0);//csmaDevices.Get (j++);
            assignDeviceAddress(device);

            if(pcap){
              nd.Add(csmaDevices);
              //csma.EnableAsciiAll(stream);
              //csma.EnablePcapAll(trace_prefix + "core-to-ns3");
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
            }

            // add internet stack if not yet created, add routing if found
            if(!p2Nflag){
              getRoutingProtocols(pt, peer2, p2Type);
            }

            csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1000));

            BOOST_FOREACH(ptree::value_type const& p1, p0.second){
              if(p1.first == "parameter"){
                if(p1.second.get<string>("<xmlattr>.name") == "bw"){
                  csma.SetChannelAttribute("DataRate", DataRateValue(stoi(p1.second.data())));
                }
                else if(p1.second.get<string>("<xmlattr>.name") == "delay"){
                  csma.SetChannelAttribute("Delay",TimeValue(MicroSeconds(stoi(p1.second.data()))));
                }
                else if(p1.second.get<string>("<xmlattr>.name") == "loss"){
                  double percent = (stod(p1.second.data()) / 100.0);
                  Ptr<RateErrorModel> rem = CreateObjectWithAttributes<RateErrorModel>("ErrorRate", DoubleValue(percent),
                                                                                       "ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
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
            Ptr<NetDevice> device = link.Get(0);//csmaDevices.Get (j++);
            assignDeviceAddress(device);

            if(pcap){
              nd.Add(csmaDevices);
              //csma.EnableAsciiAll(stream);
              //csma.EnablePcapAll(trace_prefix + "core-to-ns3");
            }

            cout << "Adding node " << peer2 << " to a csma(" << type << ") " << linkName << endl;
          }// end indirect end divice
        }// end if channel
      }// end of boost for loop
      BridgeHelper bridgeHelper;
      bridgeHelper.Install(peer, bridgeDevices);
    }// end of if switch/hub
  }// end of topology builder

  cout << "\nCORE topology imported..." << endl;

//====================================================
//create applications if given
//----------------------------------------------------
  if(!apps_file.empty()){
    ptree a_pt;

    try{
      read_xml(apps_file, a_pt);
      createApp(a_pt, duration, trace_prefix);
    } catch(const boost::property_tree::xml_parser::xml_parser_error& ex){
      cerr << "error in file " << ex.filename() << " line " << ex.line() << endl;
      exit(-5);
    }
  }

  cout << endl << nodes.GetN() << " defined node names with their respective id's..." << endl;

  int nNodes = nodes.GetN(), extra = 0;//, bNodes = bridges.GetN();
  string nodeName;

  // Set node coordinates for rouge nodes
  BOOST_FOREACH(ptree::value_type const& nod, pt.get_child("scenario")){
    if(nod.first != "router" && nod.first != "host"){
      continue;
    }

    nodeName = nod.second.get<string>("<xmlattr>.name");
    //int Locx, Locy;

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

  // Turn on global static routing if no wifi network was defined.
  if(global_is_safe){
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  }

  // enable pcap
  if(pcap){
    for(unsigned int i = 0; i < nd.GetN(); i++){
      enablePcapAll(trace_prefix, nd.Get(i));
    }
  }
/*
  // Trace routing tables 
  Ipv4GlobalRoutingHelper g;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (trace_prefix + "core2ns3-globalRoutingTables.routes", std::ios::out);
  g.PrintRoutingTableAllAt (Seconds (duration), routingStream);
*/
  // Flow monitor
  FlowMonitorHelper flowHelper;
  flowHelper.InstallAll ();

  cout << "Simulating..." << endl;

  Simulator::Stop (Seconds (duration));

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  
  cout << "Done." << endl;

  return 0;
}





