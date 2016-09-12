
#include "ns3/xmlGenerator.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"


//file to test xmlGeneration during coding
using namespace ns3;

int main(int argc, char* argv[]){

  string topo_name = "/dev/null";


  // read command-line parameters
  CommandLine cmd;
  cmd.AddValue("topo", "Path to intermediate topology file", topo_name);
  cmd.Parse (argc, argv);
  
  xmlGenerator xml_g(0 , topo_name.c_str()); //genrate xml



  return 0;
}
