// Authors:
// Armando
// John
// Andres
//
// Project description: to create an intermediate program that can take CORE
// XML format to create an NS3 scenario with a patch file equivalent to MGEN
// for program validation.

**Short notes:**

- imn folder must be placed inside folder ns-3.25
- Libraries for c++11 and boost must be installed
- We are currently using NetAnim to visualize the scenario output, therefore
  NetAnim should be installed:

  Qt4 (4.8 and over) is required to build NetAnim. This can be obtained using
  the following ways:

  For Debian/Ubuntu Linux distributions:
$ apt-get install qt4-dev-tools

  For Red Hat/Fedora based distribution:
$ yum install qt4
$ yum install qt4-devel

  To build NetAnim use the following commands:

$ cd netanim
$ make clean
$ qmake NetAnim.pro  (For MAC Users: qmake -spec macx-g++ NetAnim.pro)
$ make

  Note: qmake could be “qmake-qt4” in some systems
  This should create an executable named “NetAnim” in the same directory
  To run NetAnim:
$ ./NetAnim

- to use our program, enter the imn2ns3 folder and run the 'make configure'
  to compile waf with needed flags.
- Once waf has finished, run 'make', this will place files where they need to be.
  here is a makefile short description:
 
  typing 'make configure' Copies files to their destination, configures flags and
                          compiles/links NS3 files.
  typing 'make'           Copies files to their destination, and runs ./waf to
                          compile/link NS3 files.
  typing 'make clean'     Removes <some of> the added files.
  typing 'make cleanLogs' Removes all files and directories inside core2ns3_Logs folder

- Once all files have been placed and compiled, return to ns3.25 directory and run
  the following sample for confirmation:

$ ./waf --run "scratch/core_to_ns3_scenario --topo=imn2ns3/CORE-XML-files/sample1.xml"

- If no errors appear, program has been correctly installed.
- Program syntax uses the following:

  "topo" Path to intermediate topology file
    " --topo=imn2ns3/CORE-XML-files/sample1.xml"

  "apps" Path to application generator file
    " --apps=imn2ns3/apps-files/sample1.xml"

  "ns2" Ns2 mobility script file
    " --ns2=imn2ns3/NS2-mobility-files/sample1.ns_movements"

  "duration" Duration of Simulation
    " --duration=27.0\" \n\n";

  "pcap" Enable pcap files"
    " --pcap=true"

  "traceDir" Directory in which to store trace files
    " --traceDir=core2ns3_Logs/"



//***************************BUGS/ERRORS/TO-DO******************************//

//**IN CORE TO NS3 SCENERIO
- Network error when setting wireless nodes with submask of all ones
reason: Current implemetation assumes nodes are in an ad-hoc network, trying to
        get subnet-directed broadcast address with an all-ones netmask will
        cause errors.

- Currently implements all hubs and switches as bridge devices
reason: NS3 doesn't not have a representation for hub/switches, currently
        using bridges.

- Currently has limited application settings enabled.
reason: Routing protocols available in CORE may not be available in NS3.25.

        GlobalRoutingProtocol commonly used in NS3 is not suitable for wireless
        nodes therefore cannot be used to link all nodes correctly.

        Reading script files to build a route is not supported.



