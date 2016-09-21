#ifndef XML_PARSER_H
#define XML_PARSER_H

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <vector>
//#include <regex>

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <exception>
#include <set>

using boost::property_tree::ptree;
using namespace std;

  void xmlParser(string&, string&, string&);
//  virtual ~xmlParser();
  void parse_xml(string&);
  void to_ns3();
  void ns3_header(ptree&, ofstream&);

  static string file_name, output_file;



#endif
