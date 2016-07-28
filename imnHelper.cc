
#include "imnHelper.h"


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
  }
  
  
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
  //TODO print out hub and lanswitch links seperatly?
  cout << "Link bandwidth: " << l.bandwidth << endl;
  cout << "Link range: " << l.range << endl;
  cout << "Link jitter: " << l.jitter << endl;
  cout << "Link delay: " << l.delay << endl;
  cout << "Link error: " << l.error << endl;
  cout << "Link duplicate: " << l.duplicate << endl;
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

//***********************
//READ FILE
//***********************
void imnHelper::readFile(){

  regex node_name("[{]*n[0-9]+[}]*"); //regex to match node names of type n + number ie n1,n2,..ect
  regex n_name("n[0-9]+");
  regex number("[0-9]+");
  regex interface_name("eth[0-9]+");
  regex i_exact("interface eth[0-9]+");
  regex eq("[[:alpha:]]+[=]*[[:space:]]*[0-9]+");
  regex eq2("[[:alpha:]]+[=]*[[:space:]]*");

  smatch r_match;
  string current_node_name = "";
  string current_type = "";
  int track_curly_brackets = 0;
  int size = 0;
  int icurrent = 0;
  int inside_iblock = 0;
  int inside_link = 0;
  int link_already_set = 0;
  
  int index = 0;
  int p_link = 0;
  int h = 0;
  int ls = 0;
  //int create_flag = 0;
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
        
        if(s.find("node") != string::npos && s[s.length() - 1] == 123 ){
          regex_search(s,r_match,node_name);
          current_node_name.assign(r_match[0]);
          current_type.assign("node");
        }
        
        if(s.find("link") != string::npos && s[s.length() - 1] == 123 ){
          regex_search(s,r_match,node_name);
          current_node_name.assign(r_match[0]);
          current_type.assign("link");
          inside_link = 1;
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
            regex_search(s,r_match,n_name);
            imn_links.at(size).peer_list.push_back(r_match[0]);
            continue;
          }
          if(inside_link == 1){
            if(s.find("nodes") != string::npos){
              string t,t2;
              regex_search(s,r_match,n_name);
              t = r_match[0];
              string temp = r_match.suffix().str();
              regex_search(temp,r_match,n_name);
              t2 = r_match[0];
              
              for(vector<imnLink>::size_type i = 0; i != imn_links.size(); i++){
                if(t.compare(imn_links[i].name) == 0 || t2.compare(imn_links[i].name) == 0){
                  link_already_set = 1;
                  if(imn_links[i].type.compare("wlan") == 0)
                    break;
                  if(imn_links[i].type.compare("hub") == 0){
                    h = 1;
                  }
                  if(imn_links[i].type.compare("lanswitch") == 0){
                    ls = 1;
                  }
                  index = i;
                  break;
                }
              }
              if(link_already_set == 0){ //process as p2p link
                 imnLink l;
                 size = int(imn_links.size());
                 imn_links.push_back(l);
                 imn_links.at(size).name = current_node_name; //TODO: Might want to name something else?
                 imn_links.at(size).type = "p2p";
                 imn_links.at(size).peer_list.push_back(t);
                 imn_links.at(size).peer_list.push_back(t2);
                 p2p_count++;
                 current_type.assign("link");
                 link_already_set = 1;
                 p_link = 1;
              }
            }//end of if(s.find("nodes")
          }
        }
        //TODO consider asymetric links, sperated by {} in imn file
        //TODO convert to ns3 readable strings
        if(s.find("bandwidth") != string::npos){
          if(regex_search(s,r_match,eq)){
            regex_search(s,r_match,eq2);
            string temp = r_match.suffix().str();
            imn_links.at(size).bandwidth = temp;
          }
        }
        if(s.find("jitter") != string::npos){
          if(regex_search(s,r_match,eq)){
            regex_search(s,r_match,eq2);
            string temp = r_match.suffix().str();
            imn_links.at(size).jitter = temp;
          }
        }
        if(s.find("delay") != string::npos){
          if(regex_search(s,r_match,eq)){
            regex_search(s,r_match,eq2);
            string temp = r_match.suffix().str();
            imn_links.at(size).delay = temp;
          }
        }
        if(s.find("error") != string::npos){
          if(regex_search(s,r_match,eq)){
            regex_search(s,r_match,eq2);
            string temp = r_match.suffix().str();
            imn_links.at(size).error = temp;
          }
        }
        if(s.find("ber") != string::npos){
          if(regex_search(s,r_match,eq)){
            regex_search(s,r_match,eq2);
            string temp = r_match.suffix().str();
            imn_links.at(size).error = temp;
          }
        }
        if(s.find("range") != string::npos){//only in wifi
          if(regex_search(s,r_match,eq)){
            regex_search(s,r_match,eq2);
            string temp = r_match.suffix().str();
            imn_links.at(size).range = temp;
          }
        }
        if(s.find("duplicate") != string::npos){//only in p2p link
          if(regex_search(s,r_match,eq)){
            regex_search(s,r_match,eq2);
            string temp = r_match.suffix().str();
            imn_links.at(size).duplicate = temp;
          }
        }
           
        
        
        
      }else{
        inside_link = 0;
        link_already_set = 0;
        p_link = 0;
        h = 0;
        ls = 0;
      }
    }
  }
  printAll();
  //cout << "Done processing..." << endl;
  imunes_stream.close();
  //cout << "File closed" << endl;
}




} //end namespace ns3
