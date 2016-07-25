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



using namespace std;

namespace ns3 {

class imnHelper
{
  private:
    
  public:
    static TypeId GetTypeId (void); //recommended by ns-3 tutorial
    
    imnHelper(string file_name);
    virtual ~imnHelper(); //destructor
    void readNode();
    vector<string> &split(string &s, string delim, vector<string> &elems);
    vector<string> split(string &s, string delim);
    string removeLeadSpaces(string s);
    
    ifstream imunes_stream;
    vector<int> wlan_devices;
    vector<int> csma_devices;
    int bracket_count;
    int node_count;
    int other_count;
    int p2p_count;
    int wifi_count;
    int LAN_count;
    int link_count;
    string fname;

};

}


#endif
