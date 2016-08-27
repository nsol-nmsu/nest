#ifndef IMN_HELPER_H
#define IMN_HELPER_H

#include "ns3/object.h"
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <vector>
#include <regex>

#include "imnNode.h"
#include "imnLink.h"




using namespace std;

namespace ns3 {

class imnHelper
{
  private:
    
  public:
    static TypeId GetTypeId (void); //recommended by ns-3 tutorial
    
    imnHelper(string file_name);
    virtual ~imnHelper(); //destructor
    void readFile();
    vector<string> &split(string &s, string delim, vector<string> &elems);
    vector<string> split(string &s, string delim);
    string removeLeadSpaces(string s);
    void print_file_stats();
    void printInfo(imnNode n);
    void printInfo(imnLink l);
    void printAll();
    interface get_interface_info(string n1, string n2);
    
    ifstream imunes_stream;
    int bracket_count;
    int node_count;
    int other_count;
    int p2p_count;
    int wifi_count;
    int LAN_count;
    int link_count;
    int wlan_device_count;
    int hub_count;
    int lanswitch_count;
    int total;
    string fname;
    
    vector<imnNode> imn_nodes;
    vector<imnLink> imn_links;

};

}


#endif
