
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

void assignDeviceAddress(string type, const Ptr<NetDevice> device){
  Ptr<Node> node = device->GetNode ();
  int32_t deviceInterface;
  if(mac_addr.compare("skip") != 0){
    device->SetAddress(Mac48Address(mac_addr.c_str()));
  }

  if(ipv4_addr.compare("skip") != 0){
    regex_search(ipv4_addr, r_match, addr);
    string tempIpv4 = r_match.str();
    string tempMask = r_match.suffix().str();

    // NS3 Routing has a bug with netMask 32 in wireless networks
    // This is a temporary work around
    if(type.compare("wireless") == 0 && tempMask.compare("/32") == 0){
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

    if(type.compare("wireless") == 0 && tempIpv6Mask.compare("/128") == 0){
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

void udpDSTApp(ptree pt, double d){
  uint16_t port = 4000;
  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (NodeContainer("n6"));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (d));

  uint32_t MaxPacketSize = 1024;
  Time interPacketInterval = Seconds (0.05);
  uint32_t maxPacketCount = 320;
  UdpClientHelper client (Ipv4Address("10.0.0.10"), port);
  client.SetAttribute ("MaxPackets", UintegerValue (MaxPacketSize));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
  apps = client.Install (NodeContainer("n17"));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (d));

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

}

void tcpDSTApp(ptree pt, double d){
  ApplicationContainer apps;
  OnOffHelper onoff = OnOffHelper ("ns3::TcpSocketFactory",
                                   InetSocketAddress (Ipv4Address ("10.2.0.2"), 2000));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  onoff.SetAttribute ("Remote", AddressValue (InetSocketAddress (Ipv4Address ("10.0.0.10"), 2000)));
  onoff.SetAttribute ("PacketSize", StringValue ("1024"));
  onoff.SetAttribute ("DataRate", StringValue ("1Mbps"));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1)));
  apps = onoff.Install (NodeContainer("n6"));

  PacketSinkHelper sink = PacketSinkHelper ("ns3::TcpSocketFactory",
                                            InetSocketAddress (Ipv4Address::GetAny (), 2000));
  apps = sink.Install (NodeContainer("n17"));
  apps.Start (Seconds (d));

}

void sinkDSTApp(ptree pt, double d){
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (NodeContainer("n6"));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (d));

  Address remoteAddress (InetSocketAddress (Ipv4Address ("10.0.0.10"), port));
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", remoteAddress);
  clientHelper.SetAttribute("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer clientApp = clientHelper.Install (NodeContainer("n17"));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (d));

//  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (Names::Find<Node>("n17"), TcpSocketFactory::GetTypeId ());

//  Ptr<MyApp> app = CreateObject<MyApp> ();
//  app->Setup (ns3TcpSocket, sinkAddress, 1460, 1000000, DataRate ("100Mbps"));
//  Names::Find<Node>("n17")->AddApplication (app);
//  app->SetStartTime (Seconds (1.));
//  app->SetStopTime (Seconds (d));

/*
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
}

void burstApp(ptree pt, double d){

}

void bulkApp(ptree pt, double d){
  uint16_t port = 50000;  // well-known echo port number

  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address ("10.0.0.10"), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (3000));
  ApplicationContainer sourceApps = source.Install (Names::Find<Node>("n6"));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (d));

//
// Create a PacketSinkApplication and install it on node 1
//
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (Names::Find<Node>("n17"));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (d));
}

void createApp(AppType type, ptree pt, double duration){
  switch(type){
    case UDP   : udpDSTApp(pt, duration); break;
    case TCP   : tcpDSTApp(pt, duration); break;
    case SINK  : sinkDSTApp(pt, duration); break;
    case BURST : burstApp(pt, duration); break;
    case BULK  : bulkApp(pt, duration); break;
    default    : cout << "App type not yet supported\n";
  }
}
