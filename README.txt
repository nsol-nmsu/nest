// Authors:
// Armando
// Andres
//
// Project description: to create an internediate program that can take CORE
// IMUNES files and parse them into an NS3 scenerio.

Short notes:

imn folder must be placed inside folder ns-3.25
makefile added, 
typing 'make'           Copies files to their destination, and runs ./waf to
                        compile and link items.
typing 'make clean'     Removes <some of> the added files.
typing 'make configure' Copies files to their destination, configures settings.

to run this file, an example syntax would be:
 ./waf --run "scratch/ns3_imn_parser --topo=imn2ns3/imn_sample_files/third.imn"

or

 ./waf --run "scratch/ns3_imn_parser --topo=imn2ns3/imn_sample_files/sample4-nrlsmf.imn --traceFile=imn2ns3/imn_sample_files/sample4.ns_movements --duration=250.0"

to Generate an XML file:
 ./waf --run "scratch/xmlGenerator --topo=imn2ns3/imn_sample_files/third.imn"


