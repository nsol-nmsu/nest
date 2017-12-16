
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
void assignDeviceAddress(const Ptr<NetDevice> device){
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
// Get Routing Protocols
//====================================================================
void getRoutingProtocols(ptree pt, string peer, string pType){
  OlsrHelper olsr;
  Ipv4GlobalRoutingHelper globalRouting;
  Ipv4StaticRoutingHelper staticRouting;
  RipHelper ripRouting;
  RipNgHelper ripNgRouting;
  InternetStackHelper internetStack;

  bool applyDefaultServices = true;

  // get local services
  BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
    if(pl1.first != "host" && pl1.first != "router"){
      continue;
    }

    optional<const ptree&> service_exists = pl1.second.get_child_optional("CORE:services");
    if(service_exists){
      if(pl1.second.get<string>("<xmlattr>.name") == peer){
        Ipv4ListRoutingHelper list;

        BOOST_FOREACH(ptree::value_type const& pl2, pl1.second.get_child("CORE:services")){
          if(pl2.first == "service"){
            if(pl2.second.get<string>("<xmlattr>.name") == "StaticRoute"){
              list.Add (staticRouting, 0);
            }
            else if(pl2.second.get<string>("<xmlattr>.name") == "OLSR" ||
                    pl2.second.get<string>("<xmlattr>.name") == "OLSRORG"){
              list.Add(olsr, 10);
            }
            else if(pl2.second.get<string>("<xmlattr>.name") == "RIP"){
              list.Add(ripRouting, 5);
            }
            else if(pl2.second.get<string>("<xmlattr>.name") == "OSPFv2"){
              list.Add(globalRouting, -10);
            }
            //else if(pl2.second.get<string>("<xmlattr>.name") == "RIPNG"){
            //  list.Add(ripNgRouting, 0);
            //}
          }
        }
        internetStack.SetRoutingHelper(list);
        internetStack.Install(peer);
        applyDefaultServices = false;
      }
    }
  }
  // if there were no local, set default services according to type
  BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
    if(applyDefaultServices && pl1.first == "CORE:defaultservices"){
      BOOST_FOREACH(ptree::value_type const& pl2, pl1.second){
        if(pl2.second.get<string>("<xmlattr>.type") == pType){
          Ipv4ListRoutingHelper list;

          BOOST_FOREACH(ptree::value_type const& pl3, pl2.second){
            if(pl3.first == "service"){
              if(pl3.second.get<string>("<xmlattr>.name") == "StaticRoute"){
                list.Add (staticRouting, 0);
              }
              else if(pl3.second.get<string>("<xmlattr>.name") == "OLSR" ||
                      pl3.second.get<string>("<xmlattr>.name") == "OLSRORG"){
                list.Add(olsr, 10);
              }
              else if(pl3.second.get<string>("<xmlattr>.name") == "RIP"){
                list.Add(ripRouting, 5);
              }
              else if(pl3.second.get<string>("<xmlattr>.name") == "OSPFv2"){
                list.Add(globalRouting, -10);
              }
              //else if(pl3.second.get<string>("<xmlattr>.name") == "RIPNG"){
              //  list.Add(ripNgRouting, 0);
              //}
            }
          }
          internetStack.SetRoutingHelper(list);
          internetStack.Install(peer);
        }
      }
    }
  }

  Ptr<Node> peerNode = Names::Find<Node>(peer);
  if(!peerNode->GetObject<Ipv4>()){
    internetStack.Install(peer);
  }

  NS_ASSERT(peerNode->GetObject<Ipv4>());
}

//====================================================================
// Pcap Sniff Tx Event (ns3 segment)
//====================================================================
static void
PcapSniffTxEvent (
  Ptr<PcapFileWrapper> file,
  Ptr<const Packet>    packet,
  uint16_t             channelFreqMhz,
  uint16_t             channelNumber,
  uint32_t             rate,
  WifiPreamble         preamble,
  WifiTxVector         txVector,
  struct mpduInfo      aMpdu)
{
  uint32_t dlt = file->GetDataLinkType ();

  switch (dlt)
    {
    case PcapHelper::DLT_IEEE802_11:
      file->Write (Simulator::Now (), packet);
      return;
    case PcapHelper::DLT_PRISM_HEADER:
      {
        NS_FATAL_ERROR ("PcapSniffTxEvent(): DLT_PRISM_HEADER not implemented");
        return;
      }
    case PcapHelper::DLT_IEEE802_11_RADIO:
      {
        Ptr<Packet> p = packet->Copy ();
        RadiotapHeader header;
        uint8_t frameFlags = RadiotapHeader::FRAME_FLAG_NONE;
        header.SetTsft (Simulator::Now ().GetMicroSeconds ());

        //Our capture includes the FCS, so we set the flag to say so.
        frameFlags |= RadiotapHeader::FRAME_FLAG_FCS_INCLUDED;

        if (preamble == WIFI_PREAMBLE_SHORT)
          {
            frameFlags |= RadiotapHeader::FRAME_FLAG_SHORT_PREAMBLE;
          }

        if (txVector.IsShortGuardInterval ())
          {
            frameFlags |= RadiotapHeader::FRAME_FLAG_SHORT_GUARD;
          }

        header.SetFrameFlags (frameFlags);
        header.SetRate (rate);

        uint16_t channelFlags = 0;
        switch (rate)
          {
          case 2:  //1Mbps
          case 4:  //2Mbps
          case 10: //5Mbps
          case 22: //11Mbps
            channelFlags |= RadiotapHeader::CHANNEL_FLAG_CCK;
            break;

          default:
            channelFlags |= RadiotapHeader::CHANNEL_FLAG_OFDM;
            break;
          }

        if (channelFreqMhz < 2500)
          {
            channelFlags |= RadiotapHeader::CHANNEL_FLAG_SPECTRUM_2GHZ;
          }
        else
          {
            channelFlags |= RadiotapHeader::CHANNEL_FLAG_SPECTRUM_5GHZ;
          }

        header.SetChannelFrequencyAndFlags (channelFreqMhz, channelFlags);

        if (preamble == WIFI_PREAMBLE_HT_MF || preamble == WIFI_PREAMBLE_HT_GF || preamble == WIFI_PREAMBLE_NONE)
          {
            uint8_t mcsRate = 0;
            uint8_t mcsKnown = RadiotapHeader::MCS_KNOWN_NONE;
            uint8_t mcsFlags = RadiotapHeader::MCS_FLAGS_NONE;

            mcsKnown |= RadiotapHeader::MCS_KNOWN_INDEX;
            mcsRate = rate - 128;

            mcsKnown |= RadiotapHeader::MCS_KNOWN_BANDWIDTH;
            if (txVector.GetChannelWidth () == 40)
              {
                mcsFlags |= RadiotapHeader::MCS_FLAGS_BANDWIDTH_40;
              }

            mcsKnown |= RadiotapHeader::MCS_KNOWN_GUARD_INTERVAL;
            if (txVector.IsShortGuardInterval ())
              {
                mcsFlags |= RadiotapHeader::MCS_FLAGS_GUARD_INTERVAL;
              }

            mcsKnown |= RadiotapHeader::MCS_KNOWN_HT_FORMAT;
            if (preamble == WIFI_PREAMBLE_HT_GF)
              {
                mcsFlags |= RadiotapHeader::MCS_FLAGS_HT_GREENFIELD;
              }

            mcsKnown |= RadiotapHeader::MCS_KNOWN_NESS;
            if (txVector.GetNess () & 0x01) //bit 1
              {
                mcsFlags |= RadiotapHeader::MCS_FLAGS_NESS_BIT_0;
              }
            if (txVector.GetNess () & 0x02) //bit 2
              {
                mcsKnown |= RadiotapHeader::MCS_KNOWN_NESS_BIT_1;
              }

            mcsKnown |= RadiotapHeader::MCS_KNOWN_FEC_TYPE; //only BCC is currently supported

            mcsKnown |= RadiotapHeader::MCS_KNOWN_STBC;
            if (txVector.IsStbc ())
              {
                mcsFlags |= RadiotapHeader::MCS_FLAGS_STBC_STREAMS;
              }

            header.SetMcsFields (mcsKnown, mcsFlags, mcsRate);
          }

        if (txVector.IsAggregation ())
          {
            uint16_t ampduStatusFlags = RadiotapHeader::A_MPDU_STATUS_NONE;
            ampduStatusFlags |= RadiotapHeader::A_MPDU_STATUS_LAST_KNOWN;
            /* For PCAP file, MPDU Delimiter and Padding should be removed by the MAC Driver */
            AmpduSubframeHeader hdr;
            uint32_t extractedLength;
            p->RemoveHeader (hdr);
            extractedLength = hdr.GetLength ();
            p = p->CreateFragment (0, static_cast<uint32_t> (extractedLength));
            if (aMpdu.type == LAST_MPDU_IN_AGGREGATE || (hdr.GetEof () == true && hdr.GetLength () > 0))
              {
                ampduStatusFlags |= RadiotapHeader::A_MPDU_STATUS_LAST;
              }
            header.SetAmpduStatus (aMpdu.mpduRefNumber, ampduStatusFlags, hdr.GetCrc ());
          }

        if (preamble == WIFI_PREAMBLE_VHT)
          {
            uint16_t vhtKnown = RadiotapHeader::VHT_KNOWN_NONE;
            uint8_t vhtFlags = RadiotapHeader::VHT_FLAGS_NONE;
            uint8_t vhtBandwidth = 0;
            uint8_t vhtMcsNss[4] = {0,0,0,0};
            uint8_t vhtCoding = 0;
            uint8_t vhtGroupId = 0;
            uint16_t vhtPartialAid = 0;

            vhtKnown |= RadiotapHeader::VHT_KNOWN_STBC;
            if (txVector.IsStbc ())
              {
                vhtFlags |= RadiotapHeader::VHT_FLAGS_STBC;
              }

            vhtKnown |= RadiotapHeader::VHT_KNOWN_GUARD_INTERVAL;
            if (txVector.IsShortGuardInterval ())
              {
                vhtFlags |= RadiotapHeader::VHT_FLAGS_GUARD_INTERVAL;
              }

            vhtKnown |= RadiotapHeader::VHT_KNOWN_BEAMFORMED; //Beamforming is currently not supported

            vhtKnown |= RadiotapHeader::VHT_KNOWN_BANDWIDTH;
            //not all bandwidth values are currently supported
            if (txVector.GetChannelWidth () == 40)
              {
                vhtBandwidth = 1;
              }
            else if (txVector.GetChannelWidth () == 80)
              {
                vhtBandwidth = 4;
              }
            else if (txVector.GetChannelWidth () == 160)
              {
                vhtBandwidth = 11;
              }

            //only SU PPDUs are currently supported
            vhtMcsNss[0] |= (txVector.GetNss () & 0x0f);
            vhtMcsNss[0] |= (((rate - 128) << 4) & 0xf0);

            header.SetVhtFields (vhtKnown, vhtFlags, vhtBandwidth, vhtMcsNss, vhtCoding, vhtGroupId, vhtPartialAid);
          }

        p->AddHeader (header);
        file->Write (Simulator::Now (), p);
        return;
      }
    default:
      NS_ABORT_MSG ("PcapSniffTxEvent(): Unexpected data link type " << dlt);
    }
}

//====================================================================
// Pcap Sniff Rx Event (ns3 segment)
//====================================================================
static void
PcapSniffRxEvent (
  Ptr<PcapFileWrapper>  file,
  Ptr<const Packet>     packet,
  uint16_t              channelFreqMhz,
  uint16_t              channelNumber,
  uint32_t              rate,
  WifiPreamble          preamble,
  WifiTxVector          txVector,
  struct mpduInfo       aMpdu,
  struct signalNoiseDbm signalNoise)
{
  uint32_t dlt = file->GetDataLinkType ();

  switch (dlt)
    {
    case PcapHelper::DLT_IEEE802_11:
      file->Write (Simulator::Now (), packet);
      return;
    case PcapHelper::DLT_PRISM_HEADER:
      {
        NS_FATAL_ERROR ("PcapSniffRxEvent(): DLT_PRISM_HEADER not implemented");
        return;
      }
    case PcapHelper::DLT_IEEE802_11_RADIO:
      {
        Ptr<Packet> p = packet->Copy ();
        RadiotapHeader header;
        uint8_t frameFlags = RadiotapHeader::FRAME_FLAG_NONE;
        header.SetTsft (Simulator::Now ().GetMicroSeconds ());

        //Our capture includes the FCS, so we set the flag to say so.
        frameFlags |= RadiotapHeader::FRAME_FLAG_FCS_INCLUDED;

        if (preamble == WIFI_PREAMBLE_SHORT)
          {
            frameFlags |= RadiotapHeader::FRAME_FLAG_SHORT_PREAMBLE;
          }

        if (txVector.IsShortGuardInterval ())
          {
            frameFlags |= RadiotapHeader::FRAME_FLAG_SHORT_GUARD;
          }

        header.SetFrameFlags (frameFlags);
        header.SetRate (rate);

        uint16_t channelFlags = 0;
        switch (rate)
          {
          case 2:  //1Mbps
          case 4:  //2Mbps
          case 10: //5Mbps
          case 22: //11Mbps
            channelFlags |= RadiotapHeader::CHANNEL_FLAG_CCK;
            break;

          default:
            channelFlags |= RadiotapHeader::CHANNEL_FLAG_OFDM;
            break;
          }

        if (channelFreqMhz < 2500)
          {
            channelFlags |= RadiotapHeader::CHANNEL_FLAG_SPECTRUM_2GHZ;
          }
        else
          {
            channelFlags |= RadiotapHeader::CHANNEL_FLAG_SPECTRUM_5GHZ;
          }

        header.SetChannelFrequencyAndFlags (channelFreqMhz, channelFlags);

        header.SetAntennaSignalPower (signalNoise.signal);
        header.SetAntennaNoisePower (signalNoise.noise);

        if (preamble == WIFI_PREAMBLE_HT_MF || preamble == WIFI_PREAMBLE_HT_GF || preamble == WIFI_PREAMBLE_NONE)
          {
            uint8_t mcsRate = 0;
            uint8_t mcsKnown = RadiotapHeader::MCS_KNOWN_NONE;
            uint8_t mcsFlags = RadiotapHeader::MCS_FLAGS_NONE;

            mcsKnown |= RadiotapHeader::MCS_KNOWN_INDEX;
            mcsRate = rate - 128;

            mcsKnown |= RadiotapHeader::MCS_KNOWN_BANDWIDTH;
            if (txVector.GetChannelWidth () == 40)
              {
                mcsFlags |= RadiotapHeader::MCS_FLAGS_BANDWIDTH_40;
              }

            mcsKnown |= RadiotapHeader::MCS_KNOWN_GUARD_INTERVAL;
            if (txVector.IsShortGuardInterval ())
              {
                mcsFlags |= RadiotapHeader::MCS_FLAGS_GUARD_INTERVAL;
              }

            mcsKnown |= RadiotapHeader::MCS_KNOWN_HT_FORMAT;
            if (preamble == WIFI_PREAMBLE_HT_GF)
              {
                mcsFlags |= RadiotapHeader::MCS_FLAGS_HT_GREENFIELD;
              }

            mcsKnown |= RadiotapHeader::MCS_KNOWN_NESS;
            if (txVector.GetNess () & 0x01) //bit 1
              {
                mcsFlags |= RadiotapHeader::MCS_FLAGS_NESS_BIT_0;
              }
            if (txVector.GetNess () & 0x02) //bit 2
              {
                mcsKnown |= RadiotapHeader::MCS_KNOWN_NESS_BIT_1;
              }

            mcsKnown |= RadiotapHeader::MCS_KNOWN_FEC_TYPE; //only BCC is currently supported

            mcsKnown |= RadiotapHeader::MCS_KNOWN_STBC;
            if (txVector.IsStbc ())
              {
                mcsFlags |= RadiotapHeader::MCS_FLAGS_STBC_STREAMS;
              }

            header.SetMcsFields (mcsKnown, mcsFlags, mcsRate);
          }

        if (txVector.IsAggregation ())
          {
            uint16_t ampduStatusFlags = RadiotapHeader::A_MPDU_STATUS_NONE;
            ampduStatusFlags |= RadiotapHeader::A_MPDU_STATUS_DELIMITER_CRC_KNOWN;
            ampduStatusFlags |= RadiotapHeader::A_MPDU_STATUS_LAST_KNOWN;
            /* For PCAP file, MPDU Delimiter and Padding should be removed by the MAC Driver */
            AmpduSubframeHeader hdr;
            uint32_t extractedLength;
            p->RemoveHeader (hdr);
            extractedLength = hdr.GetLength ();
            p = p->CreateFragment (0, static_cast<uint32_t> (extractedLength));
            if (aMpdu.type == LAST_MPDU_IN_AGGREGATE || (hdr.GetEof () == true && hdr.GetLength () > 0))
              {
                ampduStatusFlags |= RadiotapHeader::A_MPDU_STATUS_LAST;
              }
            header.SetAmpduStatus (aMpdu.mpduRefNumber, ampduStatusFlags, hdr.GetCrc ());
          }

        if (preamble == WIFI_PREAMBLE_VHT)
          {
            uint16_t vhtKnown = RadiotapHeader::VHT_KNOWN_NONE;
            uint8_t vhtFlags = RadiotapHeader::VHT_FLAGS_NONE;
            uint8_t vhtBandwidth = 0;
            uint8_t vhtMcsNss[4] = {0,0,0,0};
            uint8_t vhtCoding = 0;
            uint8_t vhtGroupId = 0;
            uint16_t vhtPartialAid = 0;

            vhtKnown |= RadiotapHeader::VHT_KNOWN_STBC;
            if (txVector.IsStbc ())
              {
                vhtFlags |= RadiotapHeader::VHT_FLAGS_STBC;
              }

            vhtKnown |= RadiotapHeader::VHT_KNOWN_GUARD_INTERVAL;
            if (txVector.IsShortGuardInterval ())
              {
                vhtFlags |= RadiotapHeader::VHT_FLAGS_GUARD_INTERVAL;
              }

            vhtKnown |= RadiotapHeader::VHT_KNOWN_BEAMFORMED; //Beamforming is currently not supported

            vhtKnown |= RadiotapHeader::VHT_KNOWN_BANDWIDTH;
            //not all bandwidth values are currently supported
            if (txVector.GetChannelWidth () == 40)
              {
                vhtBandwidth = 1;
              }
            else if (txVector.GetChannelWidth () == 80)
              {
                vhtBandwidth = 4;
              }
            else if (txVector.GetChannelWidth () == 160)
              {
                vhtBandwidth = 11;
              }

            //only SU PPDUs are currently supported
            vhtMcsNss[0] |= (txVector.GetNss () & 0x0f);
            vhtMcsNss[0] |= (((rate - 128) << 4) & 0xf0);

            header.SetVhtFields (vhtKnown, vhtFlags, vhtBandwidth, vhtMcsNss, vhtCoding, vhtGroupId, vhtPartialAid);
          }

        p->AddHeader (header);
        file->Write (Simulator::Now (), p);
        return;
      }
    default:
      NS_ABORT_MSG ("PcapSniffRxEvent(): Unexpected data link type " << dlt);
    }
}

//====================================================================
// Enable Pcap All
//====================================================================
void enablePcapAll(string prefix, Ptr<NetDevice> nd){

  Ptr<CsmaNetDevice> csmaDevice = nd->GetObject<CsmaNetDevice> ();
  if (csmaDevice != 0){
  PcapHelper pcapHelper;
    
  string filename;
  filename = pcapHelper.GetFilenameFromDevice (prefix + "core2ns3", csmaDevice);

  Ptr<PcapFileWrapper> file = pcapHelper.CreateFile (filename, std::ios::out, 
                                                     PcapHelper::DLT_EN10MB);

  pcapHelper.HookDefaultSink<CsmaNetDevice> (csmaDevice, "PromiscSniffer", file);
  //pcapHelper.HookDefaultSink<CsmaNetDevice> (csmaDevice, "Sniffer", file);
  return;
  }

  Ptr<PointToPointNetDevice> p2pDevice = nd->GetObject<PointToPointNetDevice> ();
  if (p2pDevice != 0){    
    PcapHelper pcapHelper;

    string filename;
    filename = pcapHelper.GetFilenameFromDevice (prefix + "core2ns3", p2pDevice);


    Ptr<PcapFileWrapper> file = pcapHelper.CreateFile (filename, std::ios::out, 
                                                       PcapHelper::DLT_PPP);
    pcapHelper.HookDefaultSink<PointToPointNetDevice> (p2pDevice, "PromiscSniffer", file);
    return;
  }

  Ptr<WifiNetDevice> wifiDevice = nd->GetObject<WifiNetDevice> ();
  if (wifiDevice != 0){
    Ptr<WifiPhy> phy = wifiDevice->GetPhy ();
    NS_ABORT_MSG_IF (phy == 0, "WifiPhyHelper::EnablePcapInternal(): Phy layer in WifiNetDevice must be set");

    PcapHelper pcapHelper;

    string filename;
    filename = pcapHelper.GetFilenameFromDevice (prefix + "core2ns3", wifiDevice);

    Ptr<PcapFileWrapper> file = pcapHelper.CreateFile (filename, std::ios::out, PcapHelper::DLT_IEEE802_11);

    phy->TraceConnectWithoutContext ("MonitorSnifferTx", MakeBoundCallback (&PcapSniffTxEvent, file));
    phy->TraceConnectWithoutContext ("MonitorSnifferRx", MakeBoundCallback (&PcapSniffRxEvent, file));

    return;
  }
  else{
    //no device
    return;
  }
}

//====================================================================
// applications
//====================================================================
void udpEchoApp(ptree pt, double d, string trace_prefix){
  string receiver, sender, rAddress;
  float start, end;
  uint16_t sPort = 4000;
  uint16_t rPort = 4000;
  uint32_t packetSize = 1024;
  Time interPacketInterval = Seconds (1.0);
  uint32_t maxPacketCount = 1;
  bool pcap = false;

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

  if_exists = pt.get_child_optional("special.pcap");
  if(if_exists){
    pcap = pt.get<bool>("special.pcap");
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
  UdpEchoClientHelper client (Ipv4Address(rAddress.c_str()), rPort);
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

  if(pcap){
    NodeContainer pcapNodes;
    PcapHelperForDevice *helper;

    Ptr<Node> rNode = Names::Find<Node>(receiver);
    Ptr<Node> sNode = Names::Find<Node>(sender);

    for(int i = 0; i < rNode->GetNDevices(); i++){
      Ptr<NetDevice> ptrNetDevice = rNode->GetDevice(i);
      enablePcapAll(trace_prefix, ptrNetDevice);
    }
    for(int i = 0; i < sNode->GetNDevices(); i++){
      Ptr<NetDevice> ptrNetDevice = sNode->GetDevice(i);
      enablePcapAll(trace_prefix, ptrNetDevice);
    }
  }
}

//====================================================================
// TCP/UDP Application
//====================================================================
void patchApp(ptree pt, double d, string trace_prefix){
  string receiver, sender, rAddress, offVar, protocol;
  ostringstream onVar;
  float start, end;
  uint16_t sPort = 4000;
  uint16_t rPort = 4000;
  //uint32_t dataRate = 1024;
  uint32_t packetSize = 1024;
  uint32_t maxPacketCount = 1;
  double packetsPerSec = 1;
  bool pcap = false;

  sender = pt.get<string>("sender.node");
  sPort = pt.get<uint16_t>("sender.port");
  receiver = pt.get<string>("receiver.node");
  rAddress = pt.get<string>("receiver.ipv4Address");
  rPort = pt.get<uint16_t>("receiver.port");
  start = pt.get<float>("startTime");
  end = pt.get<float>("endTime");
  protocol = pt.get<string>("type");

  cout << "Creating " << protocol << " clients with destination " << receiver << " and source/s " << sender << endl;

  optional<ptree&> if_exists = pt.get_child_optional("special.packetSize");
  if(if_exists){
    packetSize = pt.get<uint32_t>("special.packetSize");
  }

  if(protocol.compare("Udp") == 0){
    protocol = "ns3::UdpSocketFactory";
  }
  else if(protocol.compare("Tcp") == 0){
    protocol = "ns3::TcpSocketFactory";

    // ns3 segment size default of 536 where we need it to adapt
    // to requested MGEN data size but not exceed a logical value,
    // suggested to be 1448.

    //uint32_t segment_size = (packetSize <= 1448)? packetSize : 1448;
    uint32_t segment_size = 1448;
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (segment_size));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue (1));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (187380));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (16384));
    Config::SetDefault("ns3::TcpSocket::ConnTimeout", TimeValue (Seconds(1)));
    Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(Seconds(0.2)));
  }

  if_exists = pt.get_child_optional("special.pcap");
  if(if_exists){
    pcap = pt.get<bool>("special.pcap");
  }

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

  OnOffHelper onOffHelper(protocol, Address(InetSocketAddress (Ipv4Address (rAddress.c_str()), rPort)));
  onOffHelper.SetAttribute("OnTime", StringValue(onVar.str()));
  onOffHelper.SetAttribute("OffTime", StringValue(offVar));

  onOffHelper.SetAttribute("DataRate",DataRateValue(packetSize * 8 * packetsPerSec));
  onOffHelper.SetAttribute("PacketSize",UintegerValue(packetSize));
  onOffHelper.SetAttribute("MaxBytes",UintegerValue(packetSize * maxPacketCount));

  ApplicationContainer clientApp;
  clientApp = onOffHelper.Install (NodeContainer(sender));
  clientApp.Start (Seconds (start));
  clientApp.Stop (Seconds (end));

  if(pcap){
    NodeContainer pcapNodes;
    PcapHelperForDevice *helper;

    Ptr<Node> rNode = Names::Find<Node>(receiver);
    Ptr<Node> sNode = Names::Find<Node>(sender);

    for(int i = 0; i < rNode->GetNDevices(); i++){
      Ptr<NetDevice> ptrNetDevice = rNode->GetDevice(i);
      enablePcapAll(trace_prefix, ptrNetDevice);
    }
    for(int i = 0; i < sNode->GetNDevices(); i++){
      Ptr<NetDevice> ptrNetDevice = sNode->GetDevice(i);
      enablePcapAll(trace_prefix, ptrNetDevice);
    }
  }
}
//====================================================================
// Sink Application
//====================================================================
void sinkApp(ptree pt, double d, string trace_prefix){
  string receiver, rAddress, protocol;
  uint16_t rPort = 4000;
  float start, end;
  bool pcap = false;

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

  optional<ptree&> if_exists = pt.get_child_optional("special.pcap");
  if(if_exists){
    pcap = pt.get<bool>("special.pcap");
  }

  // make sure end time is not beyond simulation time
  end = (end <= d)? end : d;

  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), rPort));
  PacketSinkHelper sinkHelper (protocol, sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (NodeContainer(receiver));
  sinkApp.Start (Seconds (start));
  sinkApp.Stop (Seconds (end));

  if(pcap){
    NodeContainer pcapNodes;
    PcapHelperForDevice *helper;

    Ptr<Node> rNode = Names::Find<Node>(receiver);

    for(int i = 0; i < rNode->GetNDevices(); i++){
      Ptr<NetDevice> ptrNetDevice = rNode->GetDevice(i);
      enablePcapAll(trace_prefix, ptrNetDevice);
    }
  }
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

//====================================================================
// Select Application
//====================================================================
void createApp(ptree pt, double duration, string trace_prefix){
  //NS_LOG_INFO ("Create Applications.");

  BOOST_FOREACH(ptree::value_type const& app, pt.get_child("Applications")){
    if(app.first == "application"){
      string protocol = app.second.get<string>("type");
      if(protocol.compare("UdpEcho") == 0)      { udpEchoApp(app.second, duration, trace_prefix); }
      else if(protocol.compare("Udp") == 0)     { patchApp  (app.second, duration, trace_prefix); }
      else if(protocol.compare("Tcp") == 0)     { patchApp  (app.second, duration, trace_prefix); }
      else if(protocol.compare("UdpSink") == 0) { sinkApp   (app.second, duration, trace_prefix); }
      else if(protocol.compare("TcpSink") == 0) { sinkApp   (app.second, duration, trace_prefix); }
      //else if(protocol.compare("Burst") == 0)   { burstApp(app.second, duration); }
      //else if(protocol.compare("Bulk") == 0)    { bulkApp(app.second, duration); }
      else { cout << protocol << " protocol type not supported\n";}
    }
  }
}





