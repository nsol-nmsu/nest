#ifndef XML_GENERATOR_H
#define XML_GENERATOR_H

#include "imnHelper.h"
#include "ns3/object.h"

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <vector>
#include <regex>

//boost includes
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
#include <set>
#include <exception>

namespace pt = boost::property_tree;



#define TYPE_IMN 0

namespace ns3 {
using namespace std;

class xmlGenerator
{
  
  private:
  
  public:
  static TypeId GetTypeId (void); //recommended by ns-3 tutorial
  
  xmlGenerator(int parse_mode, string fname);
  virtual ~xmlGenerator(); //destructor
  void generate_xml(int parse_mode);
  void generate_from_imn();
  void generateNetworkPlan(pt::ptree& current_tree, imnHelper& imn_c); 
  void generateMotionPlan(pt::ptree& current_tree, imnHelper& imn_c);
  
  string file_name;


};

}









#endif
