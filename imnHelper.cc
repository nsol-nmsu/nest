
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
//READNODE
//***********************
void imnHelper::readFile(){

  regex node_name("[{]*n[0-9]+[}]*"); //regex to match node names of type n + number ie n1,n2,..ect
  regex number("[0-9]+");
  regex interface_name("eth[0-9]+");
  regex i_exact("interface eth[0-9]+");

  smatch r_match;
  string current_node_name = "";
  string current_type_name = "";
  int track_curly_brackets;
  int size = 0;
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
          current_type_name.assign("node");
        }
        
        if(s.find("link") != string::npos && s[s.length() - 1] == 123 ){
          regex_search(s,r_match,node_name);
          current_node_name.assign(r_match[0]);
          current_type_name.assign("link");
        }
        
        //create link or node object based on type
        if(s.find("type") != string::npos){
        
          if(s.find("router") != string::npos){ 
            imnNode n;
            size = imn_nodes.size();
            imn_nodes.push_back(n);
            imn_nodes[size].name = current_node_name;
            imn_nodes[size].type = "router";
            node_count++;
            current_type_name.assign("node");
          
          }else if(s.find("wlan") != string::npos){
            imnLink l;
            size = imn_links.size();
            imn_links.push_back(l);
            imn_links[size].name = current_node_name;
            imn_links[size].type = "wlan";
            wlan_device_count++;
            current_type_name.assign("link");
            
          }else if(s.find("hub") != string::npos){
            imnLink l;
            size = imn_links.size();
            imn_links.push_back(l);
            imn_links[size].name = current_node_name;
            imn_links[size].type = "hub";
            hub_count++;
            current_type_name.assign("link");
          
          }else if(s.find("lanswitch") != string::npos){
            imnLink l;
            size = imn_links.size();
            imn_links.push_back(l);
            imn_links[size].name = current_node_name;
            imn_links[size].type = "lanswitch";
            lanswitch_count++;
            current_type_name.assign("link");
          
          }
        }
        //assign model to node type, only node type has model
        if(current_type_name.compare("node") == 0 && s.find("model") != string::npos){ 
          if(s.find("router") != string::npos) //TODO check for more model types
            imn_nodes[size].model = "router";
          else
            other_count++;
        }
        
        if(regex_search(s,r_match,i_exact)){ //check if interface
           cout << "exact line: " << s << endl;
           regex_search(s,r_match,interface_name);
           cout << "interface name: " << r_match[0] << endl;
        
        }
        if(s.find("interface-peer")){
          
          
        }
           
        
        
        
      }else{

      }
    }
  }
  
  cout << "NODES: " << endl;
  for(vector<imnNode>::size_type i = 0; i != imn_nodes.size(); i++){
    cout << imn_nodes[i].name << endl;
  }
  cout << "\nLINKS: " << endl;
  for(vector<imnLink>::size_type i = 0; i != imn_links.size(); i++){
    cout << imn_links[i].name << endl;
  }
  
  
  //cout << "Done processing..." << endl;
  imunes_stream.close();
  //cout << "File closed" << endl;
}




} //end namespace ns3
