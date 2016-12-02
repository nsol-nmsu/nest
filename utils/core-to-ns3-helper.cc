
#include "core-to-ns3-helper.h"

// globals for position conversion
double refLat, refLon, refAlt, refScale, refLocx, refLocy;
double x = 0.0;
double y = 0.0;
double refX = 0.0; // in orginal calculations but uneeded
double refY = 0.0; // in orginal calculations but uneeded
int refZoneNum;
char refUTMZone;

// globals for get/set addresses
string mac_addr = "skip";// some may not exists, skip them
string ipv4_addr = "skip";
string ipv6_addr = "skip";


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
  //NS_LOG_INFO ("Assign IP Addresses.");
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
void udpEchoApp(ptree pt, double d){
  string receiver, sender, rAddress;
  float start, end;
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
  start = pt.get<float>("startTime");
  end = pt.get<float>("endTime");

  cout << "Creating UDPECHO clients with destination " << receiver << " and source " << sender << endl;

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

void patchApp(ptree pt, double d){
  string receiver, sender, rAddress, offVar, protocol;
  ostringstream onVar;
  float start, end;
  uint16_t sPort = 4000;
  uint16_t rPort = 4000;
  //uint32_t dataRate = 1024;
  uint32_t packetSize = 1024;
  uint32_t maxPacketCount = 1;
  double packetsPerSec = 1;

  sender = pt.get<string>("sender.node");
  sPort = pt.get<uint16_t>("sender.port");
  receiver = pt.get<string>("receiver.node");
  rAddress = pt.get<string>("receiver.ipv4Address");
  rPort = pt.get<uint16_t>("receiver.port");
  start = pt.get<float>("startTime");
  end = pt.get<float>("endTime");
  protocol = pt.get<string>("type");

  cout << "Creating " << protocol << " clients with destination " << receiver << " and source/s " << sender << endl;

  if(protocol.compare("Udp") == 0){
    protocol = "ns3::UdpSocketFactory";
  }
  else if(protocol.compare("Tcp") == 0){
    protocol = "ns3::TcpSocketFactory";
  }

  optional<ptree&> if_exists = pt.get_child_optional("special.packetSize");
  if(if_exists){
    packetSize = pt.get<uint32_t>("special.packetSize");
  }

  //if_exists = pt.get_child_optional("special.dataRate");
  //if(if_exists){
  //  dataRate = pt.get<uint32_t>("special.dataRate");
  //}

  if_exists = pt.get_child_optional("special.maxPacketCount");
  if(if_exists){
    maxPacketCount = pt.get<uint32_t>("special.maxPacketCount");
  }

  if_exists = pt.get_child_optional("special.periodic");
  if(if_exists){
    packetsPerSec = pt.get<double>("special.periodic");
    onVar << "ns3::ConstantRandomVariable[Constant=" << packetsPerSec << "]";
    offVar = "ns3::ConstantRandomVariable[Constant=0]";
  }

  if_exists = pt.get_child_optional("special.poisson");
  if(if_exists){
    packetsPerSec = pt.get<double>("special.poisson");
    onVar << "ns3::ExponentialRandomVariable[Mean=" << packetsPerSec << "]";
    offVar = "ns3::ExponentialRandomVariable[Mean=0]";
  }

  // make sure end time is not beyond simulation time
  end = (end <= d)? end : d;

  PacketSinkHelper sink (protocol, Address(InetSocketAddress(Ipv4Address::GetAny(), rPort)));
  ApplicationContainer sinkApp = sink.Install (NodeContainer(receiver));
  sinkApp.Start(Seconds(start));
  sinkApp.Stop(Seconds(end));

  OnOffHelper onOffHelper(protocol, Address(InetSocketAddress (Ipv4Address (rAddress.c_str()), sPort)));
  onOffHelper.SetAttribute("OnTime", StringValue(onVar.str()));
  onOffHelper.SetAttribute("OffTime", StringValue(offVar));

  onOffHelper.SetAttribute("DataRate",DataRateValue(packetSize* 8 * packetsPerSec));
  onOffHelper.SetAttribute("PacketSize",UintegerValue(packetSize));
  onOffHelper.SetAttribute("MaxBytes",UintegerValue(packetSize * maxPacketCount));

  ApplicationContainer clientApp;
  clientApp = onOffHelper.Install (NodeContainer(sender));
  clientApp.Start (Seconds (start));
  clientApp.Stop (Seconds (end));
}

void sinkApp(ptree pt, double d){
  string receiver, rAddress, protocol;
  uint16_t rPort = 4000;
  float start, end;

  receiver = pt.get<string>("receiver.node");
  rAddress = pt.get<string>("receiver.ipv4Address");
  rPort = pt.get<uint16_t>("receiver.port");
  start = pt.get<float>("startTime");
  end = pt.get<float>("endTime");
  protocol = pt.get<string>("type");

  cout << "Creating " << protocol << " node named " << receiver << endl;

  if(protocol.compare("UdpSink") == 0){
    protocol = "ns3::UdpSocketFactory";
  }
  else if(protocol.compare("TcpSink") == 0){
    protocol = "ns3::TcpSocketFactory";
  }

  // make sure end time is not beyond simulation time
  end = (end <= d)? end : d;

  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), rPort));
  PacketSinkHelper sinkHelper (protocol, sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (NodeContainer(receiver));
  sinkApp.Start (Seconds (start));
  sinkApp.Stop (Seconds (end));

}

// TODO
/*void burstApp(ptree pt, double d){

}

void bulkApp(ptree pt, double d){
  string receiver, sender, rAddress, sAddress;
  float start, end;
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
  start = pt.get<float>("startTime");
  end = pt.get<float>("endTime");

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
}*/

void createApp(ptree pt, double duration){
  //NS_LOG_INFO ("Create Applications.");

  BOOST_FOREACH(ptree::value_type const& app, pt.get_child("Applications")){
    if(app.first == "application"){
      string protocol = app.second.get<string>("type");
      if(protocol.compare("UdpEcho") == 0)      { udpEchoApp(app.second, duration); }
      else if(protocol.compare("Udp") == 0)     { patchApp(app.second, duration); }
      else if(protocol.compare("Tcp") == 0)     { patchApp(app.second, duration); }
      else if(protocol.compare("UdpSink") == 0) { sinkApp(app.second, duration); }
      else if(protocol.compare("TcpSink") == 0) { sinkApp(app.second, duration); }
      //else if(protocol.compare("Burst") == 0)   { burstApp(app.second, duration); }
      //else if(protocol.compare("Bulk") == 0)    { bulkApp(app.second, duration); }
      else { cout << protocol << " protocol type not supported\n";}
    }
  }
}
