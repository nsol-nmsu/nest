
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


  //struct stat st;
  //bool did_num = false, did_sta = false, did_con = false, did_pro = false;

  // simulation locals
  NodeContainer nodes;
  ApplicationContainer apps;
  PointToPointHelper p2p;
  WifiHelper wifi = WifiHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  InternetStackHelper InternetHelper;
  //GlobalRoutingHelper routingHelper;
  //AppHelper consumerHelper ("ns3::ndn::ConsumerTest");
  //AppHelper producerHelper ("ns3::ndn::Producer");
  NetDeviceContainer wifi_devices;

  // setting default parameters for links and channels
  //Config::SetDefault ("ns3::PointToPointNetDevice::DataRate", StringValue (DEFAULT_P2P_RATE));
  //Config::SetDefault ("ns3::PointToPointNetDevice::Mtu", UintegerValue(1500));
  //Config::SetDefault ("ns3::PointToPointChannel::Delay", StringValue (DEFAULT_P2P_DELAY));
  //Config::SetDefault ("ns3::DropTailQueue::MaxPackets", StringValue ("64"));
  //Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  //Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  //Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (WIFI_PHY_MODE));
  
  // set up wifi stuff
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
  wifiPhy.Set ("RxGain", DoubleValue (0) ); 
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue (WIFI_RSS_DBM));
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
  							   "DataMode",    StringValue(WIFI_PHY_MODE),
  							   "ControlMode", StringValue(WIFI_PHY_MODE));

  // read command-line parameters
  CommandLine cmd;
  cmd.AddValue("topo", "Path to intermediate topology file", topo_name);
  //cmd.AddValue("trace","Directory in which to store trace files", trace_prefix);
  //cmd.AddValue("csize","Size per content in MB", content_size);
  //cmd.AddValue("cache","Size of NDN caches in MB", cache_size);
  cmd.Parse (argc, argv);
  
  
  imnHelper imn_container(topo_name.c_str()); //holds entire imn file in a list of node and list link containers
  
  
  
  
	
	

}
