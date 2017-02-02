Simple scripts to help aggregate data from a pcap file.

**pcapAnalyze.py**
pcapAnalyze.py takes in a pcap file and outputs 3 csv files per flow.
One that aggregates packet count per flow per second,
one that aggregates  max, mean and min packet lengths per flow per second,
and one that aggregates max, mean, and min delta time (time between packets)
per flow per second.

The filter input is a TShark display filter command. Example of such commands would be:
"tcp"
"udp"
"udp.dstport == 80"

To find more examples, you can go to https://wiki.wireshark.org/DisplayFilters 

It can also be left empty.

**plotFlows.py**
plotFlows.py takes in those csv files and outputs graphs of the aggregated data.


input would something like:

python pcapAnalyze.py -p <flie/path/to/pcap> -f <filter>

plotFlows.py -d <delta/file> -l <length/file> -c <count/file>

Both will create seperate output directories to store the files and png graphs.
