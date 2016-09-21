#include "xmlParser.h"

void xmlParser(string& out_format, string& fname, string& output){
  file_name = fname;
  output_file = output;

  parse_xml(out_format);
}

//destructor
//~xmlParser(){}

void parse_xml(string& of){
  if(of.compare("ns3") == 0){
    to_ns3();
  }
  else if(of.compare("imn") == 0){
    //to_imn();
  }
  else{
    cout << "Output format not regonized, supported format is ns3 or imn"
         << endl;
    exit(1);
  }
}

void to_ns3(){
  // populate tree structure pt
  ptree pt;
  read_xml(file_name, pt);

  ofstream out_file;
  out_file.open(output_file.c_str());

  ns3_header(pt, out_file);

  out_file.close();
}

void ns3_header(ptree& pt, ofstream& out_file){
  out_file << "\n"
"#include <string>\n"
"#include <iostream>\n"
"#include <sstream>\n"
"#include <fstream>\n"
"#include <sys/stat.h>\n"
"#include <regex>\n"
"\n"
"#include \"ns3/core-module.h\"\n"
"#include \"ns3/network-module.h\"\n"
"#include \"ns3/mobility-module.h\"\n"
"#include \"ns3/wifi-module.h\"\n"
"#include \"ns3/point-to-point-module.h\"\n"
"#include \"ns3/csma-module.h\"\n"
"#include \"ns3/internet-module.h\"\n"
"#include \"ns3/applications-module.h\"\n"
"#include \"ns3/bridge-module.h\"\n"
"#include \"ns3/traffic-control-helper.h\"\n"
"#include \"ns3/traffic-control-layer.h\"\n"
"#include \"ns3/ns2-mobility-helper.h\"\n"
"\n"
"#include \"ns3/netanim-module.h\"\n"
"\n"
"#include \"ns3/imnHelper.h\" //custom created file to parse imn\n"
"\n"
"//thinds from namespace std\n"
"using std::cout;\n"
"using std::endl;\n"
"using std::cerr;\n"
"using std::string;\n"
"using std::vector;\n"
"using std::ostream;\n"
"\n"
"// settings\n"
"#define SIMULATION_RUNTIME 1600\n"
"#define MINIMUM_FREQUENCY  \"1\"\n"
"#define CHUNK_SIZE         1436\n"
"#define WIFI_RSS_DBM       -80\n"
"#define WIFI_PHY_MODE      \"DsssRate11Mbps\"\n"
"#define DEFAULT_P2P_RATE   \"1000Mbps\"\n"
"#define DEFAULT_P2P_DELAY  \"1ms\"\n"
"#define DEBUG_OUTPUT       0\n"
"\n"
"using namespace ns3;\n"
"\n"
"//trying to parse an imn file and create an ns3 scenario file from it\n"
"int main (int argc, char *argv[]) {\n"
"  Config::SetDefault (\"ns3::OnOffApplication::PacketSize\", UintegerValue (1024));\n"
"//Config::Set(\"/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/EnergyDetectionThreshold\", DoubleValue(-82.0));\n"
"\n"
"  double duration = 10.0;\n"
"\n"
"  // Enable logging from the ns2 helper\n"
"  //LogComponentEnable (\"Ns2MobilityHelper\",LOG_LEVEL_DEBUG);\n"
"\n"
"  // config locals\n"
"  string topo_name = \"\",\n"
"         //logFile = \"ns2-mob.log\",\n"
"         traceFile = \"/dev/null\";\n"
"\n"
"  // simulation locals\n"
"  NodeContainer nodes;\n"
"\n"
"  // read command-line parameters\n"
"  CommandLine cmd;\n"
"  cmd.AddValue(\"topo\", \"Path to intermediate topology file\", topo_name);\n"
"  cmd.AddValue(\"traceFile\",\"Ns2 movement trace file\", traceFile);\n"
"  cmd.AddValue(\"duration\",\"Duration of Simulation\",duration);\n"
"  //cmd.AddValue (\"logFile\", \"Log file\", logFile);\n"
"  cmd.Parse (argc, argv);\n"
"\n"
"  // Check command line arguments\n"
"  if (topo_name.empty ()){\n"
"    std::cout << \"Usage of \" << argv[0] << \" :\n\n\"\n"
"    \"./waf --run \\\"scratch/ns3_imn_parser\"\n"
"    \" --topo=imn2ns3/imn_sample_files/sample1.imn\"\n"
"    \" --traceFile=imn2ns3/imn_sample_files/sample1.ns_movements\"\n"
"    //\" --logFile=ns2-mob.log\"\n"
"    \" --duration=27.0\\\" \n\n\";\n"
"\n"
"    return 0;\n"
"  }\n"
"\n"
"  //holds entire imn file in a list of node and list link containers  \n"
"  imnHelper imn_container(topo_name.c_str()); \n"
"  //imn_container.printAll();\n"
"\n"
"  // Create Ns2MobilityHelper with the specified trace log file as parameter\n"
"  Ns2MobilityHelper ns2 = Ns2MobilityHelper (traceFile);\n"
"\n"
"  //for ipv4 and ipv6, ipv6 will find mask, therefore use prefix for address\n"
"  regex addr(\"[0-9]+[.]{0,1}[0-9]+[.]{0,1}[0-9]+[.]{0,1}[0-9]+\");\n"
"  regex addrIpv6(\"[/]{1}[0-9]+\");\n"
"  smatch r_match;\n"
"  string peer, peer2, type;\n";

}
























/////////////////////////////////////////////////////////////////////
/*Sked read( std::istream & is )
{
    // populate tree structure pt
    using boost::property_tree::ptree;
    ptree pt;
    read_xml(is, pt);
 
    // traverse pt
    Sked ans;
    BOOST_FOREACH( ptree::value_type const& v, pt.get_child("sked") ) {
        if( v.first == "flight" ) {
            Flight f;
            f.carrier = v.second.get<std::string>("carrier");
            f.number = v.second.get<unsigned>("number");
            f.date = v.second.get<Date>("date");
            f.cancelled = v.second.get("<xmlattr>.cancelled", false);
            ans.push_back(f);
        }
    }
 
    return ans;
}*/
