// Authors:
// Armando
// Andres
//
// Project description: to create an intermediate program that can take CORE
// IMUNES files and parse them into an XML format which in turn can be used
// to create an NS3 scenario or an IMN CORE file. The XML will have a schema
// to allow human input for traffic flow, routing and logging.

Short notes:

imn folder must be placed inside folder ns-3.25
makefile added, 
typing 'make'           Copies files to their destination, and runs ./waf to
                        compile and link items.
typing 'make clean'     Removes <some of> the added files.
typing 'make configure' Copies files to their destination, configures settings.

**to generate a scenario, an example syntax would be:**
./waf --run "scratch/ns3_imn_parser --topo=imn2ns3/imn_sample_files/third.imn"

or

./waf --run "scratch/ns3_imn_parser --topo=imn2ns3/imn_sample_files/sample4-nrlsmf.imn --traceFile=imn2ns3/imn_sample_files/sample4.ns_movements --duration=250.0"

or

./waf --run "scratch/xml_tons3_scenario --topo=imn2ns3/imn_sample_files/sample1.xml --traceFile=imn2ns3/imn_sample_files/sample1.ns_movements --duration=27.0"


**to Generate an XML file:**
./waf --run "scratch/xml_tester --topo=imn2ns3/imn_sample_files/third.imn"