
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


	///////////////////////////////////////////////////////////////////////////////////////
	
	
	//here we will create a loop to get blocks from imn
	//imn file speperates definition of nodes(included are hubs, wifi, routers)
	//in blocks with definitions inside {}
	
	topo_stream.open(topo_name.c_str(), std::ifstream::in);
	int line_count = 0;
	imnHelper imn_helper(topo_name.c_str());
	imn_helper.readNode(); //generate statistics of file
	nodes.Create(imn_helper.node_count); //generate nodes needed
	
	/*cout << "There are " << imn_helper.node_count << " routers." << endl;
  cout << "There are " << imn_helper.other_count << " other nodes." << endl;
  cout << "There are " << imn_helper.p2p_count << " point-to-point connected routers." << endl;
  cout << "There are " << imn_helper.wifi_count << " wireless interfaces." << endl;
  cout << "There are " << imn_helper.LAN_count << " LAN interfaces." << endl;
  cout << "There are " << imn_helper.link_count << " links." << endl;
	cout << "ending reading." << endl;
	*/
	/*for(vector<int>::size_type i = 0; i != imn_helper.wlan_devices.size(); i++){
	  cout << "wlan is node " << imn_helper.wlan_devices[i] << endl;
	}
	for(vector<int>::size_type i = 0; i != imn_helper.csma_devices.size(); i++){
	  cout << "csma is node " << imn_helper.csma_devices[i] << endl;
	}*/
	
	
	regex node_names("[{]*n[0-9]+[}]*"); //regex to match node names of type n + number ie n1,n2,..ect
	regex number("[0-9]+"); //match number, use to extract node number
	smatch r_match;
	
	int current_node_number = 0;
	int current_node_pair = 0;
	int install_node = 0;

	
	if(!topo_stream.is_open()){
  	cerr << "Error: could not open imn file `" << topo_name << "`." << endl;
  	return -1;
  }
  else{
    //uint16_t last_ap_id = -1;
    //Ssid ssid = Ssid("no-network");
    
  	cout << "Loading imn file `" << topo_name << "`..." << endl;
  	
		int track_curly_brackets = 0;  //keep track of curly brackets to know when block starts and ends
		string line;
		while(topo_stream.good()){		//file will be read line by line, try seperating by blocks
			
			//read line
			std::getline(topo_stream,line);
			
			//remove leading space from line
			line = imn_helper.removeLeadSpaces(line);
			
			if(line.find("{") != string::npos){
				track_curly_brackets++;
			}
			
			if(line.find("}") != string::npos){
				track_curly_brackets--;
			}
			
			if(track_curly_brackets != 0){ //if 0, then finished with current block, process next
				
				string temp_line = line;
				vector<string> tokens = imn_helper.split(temp_line," "); //tokenize current string
				if(tokens[0].compare("node") == 0 && tokens[tokens.size() - 1].compare("{") == 0 ){ //begining of block, get node number
					
					regex_search(tokens[1],r_match,number);
					current_node_number = stoi(r_match[0]);
					//cout << "current node: " << current_node_number << endl;
				
				}
				if(tokens[0].compare("type") == 0){
					if(tokens[1].compare("router") == 0){ 
						install_node = 1;
					}else{
						install_node = 0;
					}
				}
				//interface-peer {eth1 n4}
				if(tokens[0].compare("interface-peer") == 0 && install_node == 1){ //install node
					regex_search(tokens[tokens.size() - 1],r_match,number);
					current_node_pair = stoi(r_match[0]);
					int pair_exist = 0;
					
					for(vector<int>::size_type i = 0; i != imn_helper.wlan_devices.size(); i++){ //check if wifi node
	  				//cout << "wlan is node " << imn_helper.wlan_devices[i] << endl;
	  				if(current_node_pair == imn_helper.wlan_devices[i]){
	  					cout << "install node " << current_node_number << " to wlan device " << current_node_pair << endl;
	  					pair_exist = 1;
	  				}
	  				
					}
					for(vector<int>::size_type i = 0; i != imn_helper.csma_devices.size(); i++){
						if(current_node_pair == imn_helper.csma_devices[i]){
							cout << "install node " << current_node_number << " to csma device " << current_node_pair << endl;
							pair_exist = 1;
						}
					}
					if(pair_exist == 0){
						cout << "install node " << current_node_number << " by p2p link to " << current_node_pair << endl;
					}
				
				
				}
				
			
			
			}
			
			
		
		line_count++;
		} //finished processing imn file
	
	}
	
	
	topo_stream.close();

}
