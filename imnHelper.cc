
#include "imnHelper.h"

/*Here are the default node types and their services:
• router - zebra, OSFPv2, OSPFv3, vtysh, and IPForward services for IGP link-state routing.
• host - DefaultRoute and SSH services, representing an SSH server having a default route when connected
directly to a router.
• PC - DefaultRoute service for having a default route when connected directly to a router.
• mdr - zebra, OSPFv3MDR, vtysh, and IPForward services for wireless-optimized MANET Designated Router
routing.
• prouter - a physical router, having the same default services as the router node type; for incorporating Linux
testbed machines into an emulation, the Machine Types is set to physical.
• xen - a Xen-based router, having the same default services as the router node type; for incorporating Xen domUs
into an emulation, the Machine Types is set to xen, and different profiles are available.
*/

//credit for split methods stackoverflow: http://stackoverflow.com/questions/236129/split-a-string-in-c
namespace ns3{

NS_OBJECT_ENSURE_REGISTERED (imnHelper);

TypeId imnHelper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::imnHelper")
    .SetParent<Object> ()
    .SetGroupName ("topology-read")
    ;
  return tid;
}

//constructor
imnHelper::imnHelper(string file_name)
{
  bracket_count = 0;
  node_count = 0;
  other_count = 0;
  p2p_count = 0;
  wifi_count = 0;
  LAN_count = 0;
  link_count = 0;
  wlan_device_count = 0;
  hub_count = 0;
  lanswitch_count=0;
  fname = file_name;
  
  //read the file to set up imnHelper
  readFile();
  total = node_count + wlan_device_count + hub_count + lanswitch_count;
  print_file_stats();

}

imnHelper::~imnHelper ()
{

}

void imnHelper::print_file_stats()
{
  cout << "There are " << node_count << " routers." << endl;
  cout << "There are " << other_count << " other nodes." << endl;
  cout << "There are " << p2p_count << " point-to-point links " << endl;
  cout << "There are " << wifi_count << " wireless interfaces." << endl;
  cout << "There are " << LAN_count << " LAN interfaces." << endl;
  cout << "There are " << link_count << " links." << endl;
  cout << "There are " << wlan_device_count << " wlan devices." << endl;
  cout << "There are " << hub_count << " hubs." << endl;
  cout << "There are " << lanswitch_count << " LAN switches." << endl;

}

void imnHelper::printInfo(imnNode n){
  cout << "Node name: " << n.name << endl;
  cout << "Node type: " << n.type << endl;
  cout << "Node model: " << n.model << endl;
  
  for(vector<interface>::size_type i = 0; i != n.interface_list.size(); i++){
    cout << "interface name: " << n.interface_list[i].interface_name << endl;
    cout << "ip addr: " << n.interface_list[i].ipv4_addr << endl;
    cout << "ipv6 addr: " << n.interface_list[i].ipv6_addr << endl;
    cout << "mac addr: " << n.interface_list[i].mac_addr << endl;
    cout << "peer connected to interface: " << n.interface_list[i].peer << endl;
  }
  cout << "POSITION: " << endl;
  cout << "x: " << n.coordinates.x << " y: " << n.coordinates.y << endl;
}

void imnHelper::printInfo(imnLink l){
  cout << "Link name: " << l.name << endl;
  cout << "Link type: " << l.type << endl;
  
  for(vector<interface>::size_type i = 0; i != l.interface_list.size(); i++){
    cout << "interface name: " << l.interface_list[i].interface_name << endl;
    cout << "ip addr: " << l.interface_list[i].ipv4_addr << endl;
    cout << "ipv6 addr: " << l.interface_list[i].ipv6_addr << endl;
    cout << "mac addr: " << l.interface_list[i].mac_addr << endl;
  }
  cout << "PEER LIST: " << endl;
  for(vector<string>::size_type i = 0; i != l.peer_list.size(); i++){
    cout << l.peer_list[i] << " " << endl;
  }
  if(l.type.compare("wlan") == 0 || l.type.compare("p2p") == 0){
    cout << "Link bandwidth: " << l.bandwidth << endl;
    cout << "Link range: " << l.range << endl;
    cout << "Link jitter: " << l.jitter << endl;
    cout << "Link delay: " << l.delay << endl;
    cout << "Link error: " << l.error << endl;
    cout << "Link duplicate: " << l.duplicate << endl;
  }else{
    for(vector<imnLink>::size_type i = 0; i != l.extra_links.size(); i++){
      cout << "connection to " << l.extra_links[i].name << endl;
      cout << "Link bandwidth: " << l.extra_links[i].bandwidth << endl;
      cout << "Link jitter: " << l.extra_links[i].jitter << endl;
      cout << "Link delay: " << l.extra_links[i].delay << endl;
      cout << "Link error: " << l.extra_links[i].error << endl;
      cout << "Link duplicate: " << l.extra_links[i].duplicate << endl;
      for(vector<string>::size_type x = 0; x != l.extra_links[i].peer_list.size(); x++){
        cout << "peer: " << l.extra_links[i].peer_list[x] << " " << endl;
      }
    }
  }
  
  cout << "POSITION: " << endl;
  cout << "x: " << l.coordinates.x << " y: " << l.coordinates.y << endl;
}

void imnHelper::printAll(){

  
  cout << "NODES: " << endl;
  for(vector<imnNode>::size_type i = 0; i != imn_nodes.size(); i++){
    printInfo(imn_nodes[i]);
    cout << endl;
  }
  cout << "\nLINKS: " << endl;
  for(vector<imnLink>::size_type i = 0; i != imn_links.size(); i++){
    printInfo(imn_links[i]);
    cout << endl;
  }

}

//Method to remove leading spaces from string
string imnHelper:: removeLeadSpaces(string s){
  string new_s;
  regex leading_spaces("[[:space:]]*(.+)");
  smatch result;
  regex_search(s,result,leading_spaces);  //search for leading spaces
  if(result[1].str().length()>0) {          //ignore empty lines
    new_s = result[1];            //write on disk the cleaned line
  }
  
  return new_s;
}

//method to split string by given delimiter and store in vector
vector<string>& imnHelper:: split(string &s, string delim, std::vector<string> &elems) {
  string item;
  size_t pos = 0;

  while ((pos = s.find(delim)) != string::npos) {
    item = s.substr(0, pos);
    if(item.compare("") != 0 )
      elems.push_back(item);
    s.erase(0, pos + delim.length());
  }
  elems.push_back(s);
  return elems;
}


vector<string> imnHelper:: split(string &s, string delim) {
  vector<string> elems;
  split(s, delim, elems);
  return elems;
}

//confirm correct interface and addresses, return interface information for n1 (first parameter)
interface imnHelper:: get_interface_info(string n1, string n2){

  imnNode temp_n1, temp_n2;
  interface n1_inter;
  
  for(vector<imnNode>::size_type i = 0; i != imn_nodes.size(); i++){
    if(imn_nodes.at(i).name.compare(n1) == 0){ //match to n1
      temp_n1 = imn_nodes.at(i);
      break;
    }
  }
  
  for(vector<interface>::size_type i = 0; i != temp_n1.interface_list.size(); i++){
    if(temp_n1.interface_list.at(i).peer.compare(n2) == 0){
      n1_inter = temp_n1.interface_list.at(i);
      break;
    }
  }
  
  return n1_inter;
  
}

//***********************
//READ FILE
//***********************
void imnHelper::readFile(){


  regex link_name("l[0-9]+");
  regex node_name("n[0-9]+");
  regex number("[0-9]+");
  regex num_dec("[0-9]+[.]{0,1}[0-9]*");
  regex interface_name("eth[0-9]+");
  regex i_exact("interface eth[0-9]+");
  regex eq("[[:alpha:]]+[=]*[[:space:]]*[0-9]+");
  regex eq2("[[:alpha:]]+[=]*[[:space:]]*");
  smatch r_match;
  
  string current_node_name = "";
  string current_type = "";
  int track_curly_brackets = 0;
  int size = 0;          //
  int inside_iblock = 0; //interface block
  int inside_link = 0;   //inside link block
  int link_already_set = 0;
  int wlan_flag = 0;
  int cust_post_config_flag = 0;
  
  string temp_bandwidth = "0";
  string temp_jitter = "0";
  string temp_delay = "0";
  string temp_error = "0";
  string temp_range = "0";
  string temp_duplicate = "0";
  string temp_peer1 = "";
  string temp_peer2 = "";
  
  // { = 123 , } = 125
  
  
  //cout << "opening file..."<< endl;
  imunes_stream.open(fname.c_str(), ifstream::in);
  //cout << "file opened, now processing..."<< endl;
  
  if(!imunes_stream.is_open()){
    cerr << "Error: Could not open IMUNES file" << endl;
    return;
  }
  else{
    while(imunes_stream.good()){

      string s;
      getline(imunes_stream, s);
      s = removeLeadSpaces(s);
			
			if(s.find("{") != string::npos){
				track_curly_brackets++;
			}
			if(s.find("}") != string::npos){
				track_curly_brackets--;
			}
      
      if(track_curly_brackets != 0){ 
        if(s.find("custom-post-config-commands") != string::npos || cust_post_config_flag == 1){
          if(track_curly_brackets == 1){
            cust_post_config_flag = 0;
          }
          else{
            cust_post_config_flag = 1;
            continue;
          }
        }     
  
        if(s.find("node") != string::npos && s[s.length() - 1] == 123 ){ //compared with ascii of {
          regex_search(s,r_match,node_name);
          current_node_name.assign(r_match[0]);
          current_type.assign("node");
        }
        
        if(s.find("link") != string::npos && s[s.length() - 1] == 123 ){
          regex_search(s,r_match,link_name);
          current_node_name.assign(r_match[0]);
          current_type.assign("link");
          inside_link = 1;
          link_count++;
        }
       
        //create link or node object based on type
        if(s.find("type") != string::npos){
        
          if(s.find("router") != string::npos){ 
            imnNode n;
            size = int(imn_nodes.size());
            imn_nodes.push_back(n);
            imn_nodes.at(size).name = current_node_name;
            imn_nodes.at(size).type = "router";
            node_count++;
            current_type.assign("node");
          
          }else if(s.find("wlan") != string::npos){
            imnLink l;
            size = int(imn_links.size());
            imn_links.push_back(l);
            imn_links.at(size).name = current_node_name;
            imn_links.at(size).type = "wlan";
            wlan_device_count++;
            current_type.assign("link");
            inside_link = 1;
            wlan_flag = 1;
            
          }else if(s.find("hub") != string::npos){
            imnLink l;
            size = int(imn_links.size());
            imn_links.push_back(l);
            imn_links.at(size).name = current_node_name;
            imn_links.at(size).type = "hub";
            hub_count++;
            current_type.assign("link");
          
          }else if(s.find("lanswitch") != string::npos){
            imnLink l;
            size = int(imn_links.size());
            imn_links.push_back(l);
            imn_links.at(size).name = current_node_name;
            imn_links.at(size).type = "lanswitch";
            lanswitch_count++;
            current_type.assign("link");
          
          }
        }
        //assign model to node type, only node type has model
        if(current_type.compare("node") == 0 && s.find("model") != string::npos){ 
          if(s.find("router") != string::npos) //TODO check for more model types
            imn_nodes.at(size).model = "router";
          else
            other_count++;
        }
        //finished with interface definition block
        if(inside_iblock != 0 && s.find("}") != string::npos){
          inside_iblock = 0;
          continue;
        }
        //check if interface name, setup interface list
        if(regex_search(s,r_match,i_exact) || s.find("interface wireless") != string::npos){ 
          string temp = "test";
          if(regex_search(s,r_match,interface_name))
            temp.assign(r_match[0]);
          else
            temp.assign("wireless");
          
          interface inter;
          inter.interface_name = temp;
          if(current_type.compare("node") == 0){
            imn_nodes.at(size).interface_list.push_back(inter);
          }
          if(current_type.compare("link") == 0){
            imn_links.at(size).interface_list.push_back(inter);
          }
          inside_iblock++;
        }
        
        //get all interface information
        if(inside_iblock != 0){
          string temp_line = s;
          vector<string> tokens = split(temp_line," "); //tokenize current string
          
          if(current_type.compare("node") == 0){
            if(s.find("ip address") != string::npos){
              imn_nodes.at(size).interface_list.at(inside_iblock-1).ipv4_addr = tokens[tokens.size() - 1];
            }
            if(s.find("ipv6") != string::npos){
              imn_nodes.at(size).interface_list.at(inside_iblock-1).ipv6_addr = tokens[tokens.size() - 1];
            }
            if(s.find("mac") != string::npos){
              imn_nodes.at(size).interface_list.at(inside_iblock-1).mac_addr = tokens[tokens.size() - 1];
            }
          }else{
            if(s.find("ip address") != string::npos){
              imn_links.at(size).interface_list.at(inside_iblock-1).ipv4_addr = tokens[tokens.size() - 1];
            }
            if(s.find("ipv6") != string::npos){
              imn_links.at(size).interface_list.at(inside_iblock-1).ipv6_addr = tokens[tokens.size() - 1];
            }
            if(s.find("mac") != string::npos){
              imn_links.at(size).interface_list.at(inside_iblock-1).mac_addr = tokens[tokens.size() - 1];
            }
          }
        }
        
       
        
        //save all links corresponding to this link type( i.e all links to wlan device ect..)
        if(current_type.compare("link") == 0){ 
          if(s.find("interface-peer") != string::npos){
            regex_search(s,r_match,node_name);
            imn_links.at(size).peer_list.push_back(r_match[0]);
            continue;
          }
        }
        
        //save name of peer to interface definition for use in setting up ipv4 and ipv6
        if(current_type.compare("link") != 0 && s.find("interface-peer") != string::npos){
          regex_search(s,r_match,node_name);
          string temp_n = r_match[0];
          regex_search(s,r_match,interface_name);
          for(vector<interface>::size_type i = 0; i != imn_nodes.at(size).interface_list.size(); i++){
            if(r_match[0].compare(imn_nodes.at(size).interface_list.at(i).interface_name) == 0){
              imn_nodes.at(size).interface_list.at(i).peer = temp_n;
              break;
            }
          }
        }
        
        if(inside_link == 1 && s.compare("}") != 0){ //process entire link into temp variables
          
          //TODO consider asymetric links, sperated by {} in imn file
          if(s.find("bandwidth") != string::npos){
            if(regex_search(s,r_match,eq)){
              regex_search(s,r_match,eq2);
              temp_bandwidth.assign(r_match.suffix().str());
            }
          }
          if(s.find("jitter") != string::npos){
            if(regex_search(s,r_match,eq)){
              regex_search(s,r_match,eq2);
              temp_jitter.assign(r_match.suffix().str());
            }
          }
          if(s.find("delay") != string::npos){
            if(regex_search(s,r_match,eq)){
              regex_search(s,r_match,eq2);
              temp_delay.assign(r_match.suffix().str());
            }
          }
          if(s.find("error") != string::npos){
            if(regex_search(s,r_match,eq)){
              regex_search(s,r_match,eq2);
              temp_error.assign(r_match.suffix().str());
            }
          }
          if(s.find("ber") != string::npos){
            if(regex_search(s,r_match,eq)){
              regex_search(s,r_match,eq2);
              temp_error.assign(r_match.suffix().str());
            }
          }
          if(s.find("range") != string::npos){//only in wifi
            if(regex_search(s,r_match,eq)){
              regex_search(s,r_match,eq2);
              temp_range.assign(r_match.suffix().str());
            }
          }
          if(s.find("duplicate") != string::npos){//only in p2p link
            if(regex_search(s,r_match,eq)){
              regex_search(s,r_match,eq2);
              temp_duplicate.assign(r_match.suffix().str());
            }
          }
          if(s.find("nodes") != string::npos){//only in p2p link
            regex_search(s,r_match,node_name);
            temp_peer1.assign(r_match[0]);
            string temp = r_match.suffix().str();
            regex_search(temp,r_match,node_name);
            temp_peer2.assign(r_match[0]);
          } 
        }
        
        //save coordinates, might not exactly correspond to ns3 coordinates
        if(s.find("iconcoords") != string::npos){
          regex_search(s,r_match,num_dec);
          float t = stoi(r_match[0]);
          string temp = r_match.suffix().str();
          regex_search(temp,r_match,num_dec);
          float t2 = stoi(r_match[0]);

          if(current_type.compare("node") == 0){
            imn_nodes.at(size).coordinates.x = t;
            imn_nodes.at(size).coordinates.y = t2;
            imn_nodes.at(size).coordinates.type = "cartesian";
          }
          else{
            imn_links.at(size).coordinates.x = t;
            imn_links.at(size).coordinates.y = t2;
            imn_links.at(size).coordinates.type = "cartesian";
          }
        }
      }else{ //track_curly_brackets was 0
        
        if(inside_link == 1){ //create and store link
          if(wlan_flag == 1){
            cout << "begin wlan" << endl;
            imn_links.at(size).bandwidth = temp_bandwidth;
            imn_links.at(size).jitter = temp_jitter;
            imn_links.at(size).delay = temp_delay;
            imn_links.at(size).error = temp_error;
            imn_links.at(size).range = temp_range;
            imn_links.at(size).duplicate = temp_duplicate;
            link_already_set = 1;
            cout << "end wlan" << endl;
          }
          for(vector<imnLink>::size_type i = 0; i != imn_links.size(); i++){
            if(temp_peer1.compare(imn_links[i].name) == 0 || temp_peer2.compare(imn_links[i].name) == 0){
              link_already_set = 1;
              if(imn_links[i].type.compare("wlan") == 0) //wlan, already set up
                break;
                   
              if(imn_links[i].type.compare("hub") == 0 || imn_links[i].type.compare("lanswitch") == 0){
                imnLink l;
                l.name = current_node_name;
                l.type = "connection";
                l.bandwidth = temp_bandwidth;
                l.jitter = temp_jitter;
                l.delay = temp_delay;
                l.error = temp_error;
                l.range = temp_range;
                l.duplicate = temp_duplicate;
                
                int lsize = int(imn_links.at(i).extra_links.size());
                imn_links.at(i).extra_links.push_back(l);
                
                if(temp_peer1.compare(imn_links[i].name) == 0){ //get correct name of device and peer
                  imn_links.at(i).extra_links.at(lsize).peer_list.push_back(temp_peer2);
                }else{
                  imn_links.at(i).extra_links.at(lsize).peer_list.push_back(temp_peer1);
                }
              }
              break;
            }
          }
          if(link_already_set == 0){ //process as p2p link
            imnLink l;
            l.name = current_node_name;
            l.type = "p2p";
            l.bandwidth = temp_bandwidth;
            l.jitter = temp_jitter;
            l.delay = temp_delay;
            l.error = temp_error;
            l.duplicate = temp_duplicate;
            
            size = int(imn_links.size());
            imn_links.push_back(l);
            imn_links.at(size).peer_list.push_back(temp_peer1);
            imn_links.at(size).peer_list.push_back(temp_peer2);
            p2p_count++;
          }
        }
        
        inside_link = 0;
        link_already_set = 0;
        wlan_flag = 0;
        cust_post_config_flag = 0;
        
        temp_bandwidth.assign("0");
        temp_jitter.assign("0");
        temp_delay.assign("0");
        temp_error.assign("0");
        temp_range.assign("0");
        temp_duplicate.assign("0");
        temp_peer1.assign("");
        temp_peer2.assign("");
      }
    }
  }
  
  //printAll();
  //cout << "Done processing..." << endl;
  imunes_stream.close();
  //cout << "File closed" << endl;
}




} //end namespace ns3
