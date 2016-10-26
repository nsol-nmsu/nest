
#ifndef CORE_TO_NS3_HELPER_H
#define CORE_TO_NS3_HELPER_H


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/ns2-mobility-helper.h"
#include "ns3/flow-monitor-helper.h"

#include "ns3/netanim-module.h"

#include <regex>
#include <sys/stat.h>

#include "LatLong-UTMconversion.h"

#include <boost/optional/optional.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <exception>
#include <set>

//things from namespace std

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

// globals for splitting strings
regex addr("[0-9]+[.]{0,1}[0-9]+[.]{0,1}[0-9]+[.]{0,1}[0-9]+");
regex addrIpv6("[/]{1}[0-9]+");
regex name("[a-zA-Z0-9]+");
smatch r_match;

enum AppType{
  UDP,
  TCP,
  BURST,
  BULK,
  SINK,
  DEFUALT
};

void getXYPosition(const double Lat, const double Lon, double &rx, double &ry);
void getAddresses(ptree pt, string sourceNode, string peerNode);
void assignDeviceAddress(string type, const Ptr<NetDevice> device);
void udpDSTApp(ptree pt, double d);
void tcpDSTApp(ptree pt, double d);
void sinkDSTApp(ptree pt, double d);
void burstApp(ptree pt, double d);
void bulkApp(ptree pt, double d);
void createApp(AppType type, ptree pt, double duration);

// globals for position conversion
extern double refLat, refLon, refAlt, refScale, refLocx, refLocy;
extern double x;
extern double y;
extern double refX; // in orginal calculations but uneeded
extern double refY; // in orginal calculations but uneeded
extern int refZoneNum;
extern char refUTMZone;

// globals for get/set addresses
extern string mac_addr;// some may not exists, skip them
extern string ipv4_addr;
extern string ipv6_addr;


#endif



