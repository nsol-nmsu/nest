#include "xmlParser.h"

int main (int argc, char* argv[]){
  // Test for correct number of parameters
  if(argc < 4 || argc > 4){
    fprintf (stderr, "Usage:  %s <OUTPUT FORMAT> <XML FILE> <OUTPUT FILE NAME>\n", argv[0]);
    exit(-1);
  }

  // Get operation to perform and file names
  string format = argv[1];
  string xml_file = argv[2];
  string out_name = argv[3];

  xmlParser(format, xml_file, out_name);
}
