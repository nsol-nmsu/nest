
#ifndef TOPO_BUILDER_H
#define TOPO_BUILDER_H

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
#include "ns3/trace-helper.h"

#include <regex>
#include <sys/stat.h>

#include "ns3/LatLong-UTMconversion.h"
#include "ns3/core-to-ns3-helper.h"

#include <boost/optional/optional.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <exception>
#include <iostream>
#include <climits>
#include <set>

//things from namespace std

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

void p2pBuilder(ptree pt, const ptree& child, NodeContainer nodes, NetDeviceContainer nd, bool pcap);

#endif
