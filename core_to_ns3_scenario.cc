
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/ns2-mobility-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/olsr-routing-protocol.h"
#include "ns3/olsr-helper.h"
#include "ns3/aodv-helper.h"
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
#include <iostream>
#include <climits>
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
using std::ostringstream;
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
static regex category("[:]{1}[0-9]+");
static regex rateUnit("[0-9]+[.]{0,1}[0-9]*");
//smatch r_match;

//--------------------------------------------------------------------
// globals for get/set addresses, routing protocols and packet control
//--------------------------------------------------------------------
static string mac_addr  = "skip";// some may not exists, skip them
static string ipv4_addr = "skip";
static string ipv6_addr = "skip";

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
  string receiver, sender, rAddress, sAddress, offVar, protocol;
  ostringstream onVar;
  float start, end;
  uint16_t sPort = 4000;
  uint16_t rPort = 4000;
  //uint32_t dataRate = 1024;
  uint32_t packetSize = 1024;
  uint32_t maxPacketCount = 1;
  double packetsPerSec = 1;
  bool pcap = false;

  sAddress = pt.get<string>("sender.ipv4Address");
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
    uint32_t segment_size = (packetSize <= 1448)? packetSize : 1448;
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (segment_size));
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
//###################################################################
// Parse CORE XML and create an ns3 scenario file from it
//###################################################################
int main (int argc, char *argv[]) {
  bool global_is_safe = true;

  // config locals
  bool pcap = false;
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

  if(real_time){
    GlobalValue::Bind ("SimulatorImplementationType", 
                       StringValue ("ns3::RealtimeSimulatorImpl"));
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
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream (trace_prefix + "core-to-ns3.tr");

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
            double percent = stod(p0.second.data());
            Ptr<RateErrorModel> rem = CreateObjectWithAttributes<RateErrorModel>("ErrorRate", DoubleValue(percent / 100.0),
                                                                                 "ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
            p2p.SetDeviceAttribute("ReceiveErrorModel",PointerValue(rem));
          }
        }
      }

      // add internet stack if not yet created, add routing if found
      p2pDevices.Add(p2p.Install(peer, peer2));

      OlsrHelper olsr;
      Ipv4GlobalRoutingHelper globalRouting;
      Ipv4StaticRoutingHelper staticRouting;
      RipHelper ripRouting;
      RipNgHelper ripNgRouting;
      InternetStackHelper internetP2P_1;
      InternetStackHelper internetP2P_2;

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
            internetP2P_1.SetRoutingHelper(list);
            internetP2P_1.Install(peer);
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
            internetP2P_2.SetRoutingHelper(list); // has effect on the next Install ()
            internetP2P_2.Install(peer2);
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
            internetP2P_1.SetRoutingHelper(list);
            internetP2P_1.Install(peer);
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
            internetP2P_2.SetRoutingHelper(list);
            internetP2P_2.Install(peer2);
          }
        }
      }

      // Assert internet stack was installed correctly
      Ptr<Node> peerNode = Names::Find<Node>(peer);
      Ptr<Node> peer2Node = Names::Find<Node>(peer2);
      if(!peerNode->GetObject<Ipv4>()){
        internetP2P_1.Install(peer);
      }
      if(!peer2Node->GetObject<Ipv4>()){
        internetP2P_2.Install(peer2);
      }

      NS_ASSERT(peerNode->GetObject<Ipv4>());
      NS_ASSERT(peer2Node->GetObject<Ipv4>());

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
      global_is_safe = false;
      int j = 0;
      double dist = 0.0;
      bool twoRay_set = false;
      bool freespace_set = false;

      peer = nod.second.get<string>("<xmlattr>.name");
      NodeContainer wifiNodes;
      NetDeviceContainer wifiDevices;

      WifiHelper wifi;
      YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default();
      YansWifiChannelHelper wifiChannel;
      WifiMacHelper wifiMac;

      wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

      Config::Set("ns3::WifiMacQueue::MaxPacketNumber", UintegerValue (1000)); 

      BOOST_FOREACH(ptree::value_type const& p0, child.get_child("channel")){
        if(p0.first == "type" && p0.second.data() == "basic_range"){
          BOOST_FOREACH(ptree::value_type const& p1, p0.second){
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

                Config::Set("ns3::WifiMacQueue::MaxPacketNumber", UintegerValue (atoi(Q.c_str()+1))); 
              }
              else if(p0.second.get<string>("<xmlattr>.name") == "cwmin"){
                string tempCw, Cw;
                tempCw = p1.second.data();
                regex_search(tempCw, r_match, rateUnit);
                Cw = r_match.str();

                Config::Set("ns3::Dcf::MinCw", UintegerValue (atoi(Cw.c_str()+1))); 
                Config::Set("ns3::EdcaTxopN::MinCw", UintegerValue (atoi(Cw.c_str()+1)));
              }
              else if(p0.second.get<string>("<xmlattr>.name") == "cwmax"){
                string tempCw, Cw;
                tempCw = p1.second.data();
                regex_search(tempCw, r_match, rateUnit);
                Cw = r_match.str();

                Config::Set("ns3::Dcf::MaxCw", UintegerValue (atoi(Cw.c_str()+1)));
                Config::Set("ns3::EdcaTxopN::MaxCw", UintegerValue (atoi(Cw.c_str()+1)));
              }
              else if(p0.second.get<string>("<xmlattr>.name") == "aifs"){
                string tempAifs, Aifs;
                tempAifs = p1.second.data();
                regex_search(tempAifs, r_match, rateUnit);
                Aifs = r_match.str();

                Config::Set("ns3::Dcf::Aifs", UintegerValue (atoi(Aifs.c_str()+1))); 
                Config::Set("ns3::EdcaTxopN::Aifs", UintegerValue (atoi(Aifs.c_str()+1))); 
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

          Ipv4GlobalRoutingHelper globalRouting;
          Ipv4StaticRoutingHelper staticRouting;
          OlsrHelper olsr;
          RipHelper ripRouting;
          RipNgHelper ripNgRouting;
          Ipv4ListRoutingHelper list;
          InternetStackHelper wifiInternet;
          bool applyDefaultServices2 = true;

          // get local services
          BOOST_FOREACH(ptree::value_type const& pl1, pt.get_child("scenario")){
            if(pl1.first != "host" && pl1.first != "router"){
              continue;
            }

            optional<const ptree&> service_exists = pl1.second.get_child_optional("CORE:services");
            if(service_exists){
              if(!p2flag && pl1.second.get<string>("<xmlattr>.name") == peer2){
                bool olsrRoutingSet = false;

                BOOST_FOREACH(ptree::value_type const& pl2, pl1.second.get_child("CORE:services")){
                  if(pl2.first == "service"){
                    if(pl2.second.get<string>("<xmlattr>.name") == "StaticRoute"){
                      list.Add (staticRouting, 0);
                    }
                    else if(!olsrRoutingSet && pl2.second.get<string>("<xmlattr>.name") == "OLSR"){
                      list.Add(olsr, 10);
                    }
                    else if(pl2.second.get<string>("<xmlattr>.name") == "RIP"){
                      list.Add(ripRouting, 5);
                    }
                    else if(!olsrRoutingSet && pl2.second.get<string>("<xmlattr>.name") == "OSPFv2"){
                      list.Add(globalRouting, -10);
                      cout << "Warning: OSPFv2 routing unavailable for wireless nodes. \n"
                           << " NS-3 recommends using OLSR if routing is of on consequence." << endl; 
                    }
                    //else if(pl2.second.get<string>("<xmlattr>.name") == "RIPNG"){
                    //  list.Add(ripNgRouting, 0);
                    //}
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
                bool olsrRoutingSet = false;

                BOOST_FOREACH(ptree::value_type const& pl2, pl1.second.get_child("device")){
                  if(pl2.first == "service"){
                    if(pl2.second.get<string>("<xmlattr>.name") == "StaticRoute"){
                      list.Add (staticRouting, 0);
                    }
                    else if(!olsrRoutingSet && pl2.second.get<string>("<xmlattr>.name") == "OLSR"){
                      list.Add(olsr, 10);
                      olsrRoutingSet = true;
                    }
                    else if(pl2.second.get<string>("<xmlattr>.name") == "RIP"){
                      list.Add(ripRouting, 5);
                    }
                    else if(!olsrRoutingSet && pl2.second.get<string>("<xmlattr>.name") == "OSPFv2"){
                      list.Add(olsr, 10);
                      olsrRoutingSet = true;
                      list.Add(globalRouting, -10);
                    }
                    //else if(pl2.second.get<string>("<xmlattr>.name") == "RIPNG"){
                    //  list.Add(ripNgRouting, 0);
                    //}
                  }
                }
                wifiInternet.SetRoutingHelper(list);
                wifiInternet.Install(peer2);
              }
            }
          }

          Ptr<Node> peer2Node = Names::Find<Node>(peer2);
          if(!peer2Node->GetObject<Ipv4>()){
            wifiInternet.Install(peer2);
          }

          NS_ASSERT(peer2Node->GetObject<Ipv4>());

          if(!infrastructure.empty()){
            if(access_point.find(peer2) != string::npos){
              // setup sta.
              wifiMac.SetType ("ns3::StaWifiMac",
                               "Ssid", SsidValue (peer),
                               "ActiveProbing", BooleanValue (false));
            }
            else{
              // setup ap.
              wifiMac.SetType ("ns3::ApWifiMac",
                               "Ssid", SsidValue (peer));
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
/*
          // attempt to define gateway TODO
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
            //olsrrp_Gw->AddHostNetworkAssociation (Ipv4Address::GetAny (), Ipv4Mask ("255.255.0.0"));
          }
*/
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
                  else{
                    // error
                    cerr << "Error: Topology for" << type << " " << peer << "could not be built: " << name_holder << endl;
                    return -1;
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
                  double percent = stod(p1.second.data());
                  Ptr<RateErrorModel> rem = CreateObjectWithAttributes<RateErrorModel>("ErrorRate", DoubleValue(percent / 100.0),
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
// Connect two bridges unrelated to controlling switch (a.k.a "peer")
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
                  double percent = stod(p1.second.data());
                  Ptr<RateErrorModel> rem = CreateObjectWithAttributes<RateErrorModel>("ErrorRate", DoubleValue(percent / 100.0),
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

            InternetStackHelper internetCsma;
            if(!p2Nflag){
              csmaNodes.Create(1);
              Names::Add(peer2, csmaNodes.Get(csmaNodes.GetN() - 1));
              nodes.Add(peer2);
              //internetCsma.Install(peer2);
            }

            OlsrHelper olsr;
            Ipv4GlobalRoutingHelper globalRouting;
            Ipv4StaticRoutingHelper staticRouting;
            RipHelper ripRouting;
            RipNgHelper ripNgRouting;
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
                  internetCsma.SetRoutingHelper(list);
                  internetCsma.Install(peer2);
                }
              }
            }

            Ptr<Node> peer2Node = Names::Find<Node>(peer2);
            if(!peer2Node->GetObject<Ipv4>()){
              internetCsma.Install(peer2);
            }

            NS_ASSERT(peer2Node->GetObject<Ipv4>());

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
                  double percent = stod(p1.second.data());
                  Ptr<RateErrorModel> rem = CreateObjectWithAttributes<RateErrorModel>("ErrorRate", DoubleValue(percent / 100.0),
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

            InternetStackHelper internetCsma;
            if(!p2Nflag){
              csmaNodes.Create(1);
              Names::Add(peer2, csmaNodes.Get(csmaNodes.GetN() - 1));
              nodes.Add(peer2);
              //internetCsma.Install(peer2);
            }

            OlsrHelper olsr;
            Ipv4GlobalRoutingHelper globalRouting;
            Ipv4StaticRoutingHelper staticRouting;
            RipHelper ripRouting;
            RipNgHelper ripNgRouting;
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
                  internetCsma.SetRoutingHelper(list);
                  internetCsma.Install(peer2);
                }
              }
            }

            Ptr<Node> peer2Node = Names::Find<Node>(peer2);
            if(!peer2Node->GetObject<Ipv4>()){
              internetCsma.Install(peer2);
            }

            NS_ASSERT(peer2Node->GetObject<Ipv4>());

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
                  double percent = stod(p1.second.data());
                  Ptr<RateErrorModel> rem = CreateObjectWithAttributes<RateErrorModel>("ErrorRate", DoubleValue(percent / 100.0),
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

  // Turn on global static routing if no wifi network was defined
  if(global_is_safe){
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  }

  // enable pcap
  if(pcap){
    for(int i = 0; i < nd.GetN(); i++){
      enablePcapAll(trace_prefix, nd.Get(i));
    }
  }
/*
  // Trace routing tables 
  Ipv4GlobalRoutingHelper g;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (trace_prefix + "core2ns3-global-routing.routes", std::ios::out);
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







