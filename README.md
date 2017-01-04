# CORE To NS-3 Translator #
The CORE to NS-3 translator aims to provide a quick and easy way to validate a CORE emulation with an NS-3 simulation. 
It is an intermediate program that can take a CORE XML file and dynamically create an NS3 scenario.

## 1) Build / Install ##

You need BOOST and C++11 libraries to compile and use core to ns-3 translator.
Tested in ns-3.25 and CORE version 4.8.

The program folder must exist inside the ns-3.25 folder to properly install using only the makefile.

We do encourage using NetAnim to visualize the scenario output. Build instructions can be found in
[https://www.nsnam.org/wiki/NetAnim_3.107](Link URL)

To use our program, enter the imn2ns3 folder and run the `make configure`.

```
#!terminal

cd imn2ns3/
make configure
```


Once waf has finished, run `make`, this will place files where they need to be.
```
#!terminal

make
```

Here is a makefile short description:
 
* typing `make configure` Copies files to their destination, configures flags and compiles/links NS3 files.
* typing `make`           Copies files to their destination, and runs ./waf to compile/link NS3 files.
* typing `make clean`     Removes <some of> the added files.
* typing `make cleanLogs` Removes all files and directories inside core2ns3_Logs folder

## 2) Execute ##

Once all files have been placed and compiled, return to ns3.25 directory and run
the following sample for confirmation:

```
#!terminal

cd ..
./waf --run "scratch/core_to_ns3_scenario --topo=imn2ns3/CORE-XML-files/sample1.xml"
```

This program supports generating traffic flow using a simple XML Schema.
Examples of the schema exist inside the `*/ns-3.25/imn2ns3/apps-files/` directory.

This program also supports NS2 mobility scripts.
*Note:* Node ID for NS3 nodes will differ as they are assigned at the time  they are
created. The NS2 script used for CORE will have be to altered to mirror this change.
A map is given every time the topology finishes being created to help identify the changes.
Currently this can only be done by the user.

The following is a list of the commands supported by the core to ns-3 translator:

"topo" Path to intermediate topology file

```
#!terminal

--topo=<path/to/topology-file.xml>
```

"apps" Path to application generator file

```
#!terminal

--apps=<path/to/file-apps.xml>
```

"ns2" Ns2 mobility script file

```
#!terminal

--ns2=<path/to/file.ns_movements>
```

"duration" Duration of Simulation

```
#!terminal

--duration=[float]
```

"pcap" Enable pcap files

```
#!terminal

--pcap=[true|false]
```

"traceDir" Directory in which to store trace files. Note: always end with '/'

```
#!terminal

--traceDir=<path/to/directory/>
```

"infra" Required only when a wireless networks is in infrastructure mode.
        Declare the name of the WLAN in this mode.
```
#!terminal

--infra=<wlan1:wlan2:...>
```

"ap" Required only when a wireless networks is in infrastructure mode.
     Declare the name of the node acting as the access point / gateway.
```
#!terminal

--ap=<accessPoint1:accessPoint2:...>
```


An example of this will look is as follows
```
#!terminal

./waf --run "scratch/core_to_ns3_scenario --topo=imn2ns3/CORE-XML-files/sample1.xml --apps=imn2ns3/apps-files/sample1-apps.xml --ns2=imn2ns3/NS2-mobility-files/sample1.ns_movements --duration=27.0 --pcap=true --traceDir=core2ns3_Logs/"
```

When pcap is enabled, all pcap files will be placed in `*/ns-3.25/core2ns3Logs/` directory along with a trace file 
or at the directory given through command line `--traceDir=path/to/directory/`.

If pcap is activated through command line, pcap will be enabled for all nodes.
If pcap is enabled through an application patch file, only named nodes will output a pcap file.

## 3) NetAnim ##

Every scenario will output an XML file for NetAnim use named *NetAnim-core-to-ns3.xml* in the ns-3.25 directory.

Instructions on how to use NetAnim and its correlating files can be found in 
[https://www.nsnam.org/wiki/NetAnim_3.107](Link URL)

## ***LIMITATIONS/TO-DOS*** ##

* **Network error when setting wireless nodes with sub-mask of all ones.**
    - Current implementation assumes nodes are in an ad-hoc network.
    - Trying to get subnet-directed broadcast address with an all-ones net-mask will
      cause errors.

* **Implements all hubs and switches as bridge devices.**
    - NS3 doesn't not have a representation for hub/switches, currently
      all hubs and switches are declared as bridges.

* **Has limited application settings enabled.**
    - Routing protocols available in CORE may not be available in NS-3.25.
    - GlobalRoutingProtocol which behaves as OSPF in NS3 is not suitable for wireless
      nodes. If there is a wireless network in your topology, it is suggested to use OLSR
      versus OSPF.
    - Reading script files to build a route is not supported in this version.

* **Wireless nodes may not correctly translate from CORE to NS-3.**
    - CORE makes no distinction between ad-hoc or infrastructure nodes where as
      NS-3 does, making it difficult to build an infrastructure wireless topology correctly.
      User will need to declare the WLAN and access points for an infrastructure wireless 
      network through command line.
    - Some EMANE and CORE attributes for wireless links do not translate well to NS-3
      attributes. These may include multiple attributes for queue, contention window,
      arbitration inter frame, txop, and retry limits among others. Currently, only the 
      first attribute will be taken from each.