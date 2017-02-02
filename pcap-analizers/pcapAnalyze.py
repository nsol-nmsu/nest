import sys
import os, getopt
import pyshark


def entry_exit(f):
    def new_f(obj):
        print "Entering", f.__name__
        f(obj)
        print "Exited", f.__name__

    new_f.__name__ = f.__name__
    return new_f


# @entry_exit
def sum_lengths(capture):
    total_length = sum(int(packet.length) for packet in capture)
    return total_length


# @entry_exit
def check_flow(flow, list2):
    result = 0
    for list1 in flow:
        result = cmp(list1, list2)
        if result == 0:
            return flow.index(list2)

    return 0


# @entry_exit
def get_ip_version(packet):
    for layer in packet.layers:
        if layer._layer_name == 'ip':
            return 4
        elif layer._layer_name == 'ipv6':
            return 6


def index_flows(flow, capture):
    count = 0
    ip = None
    list2 = []
    result = 0

    for packet in capture:
        ip_version = get_ip_version(packet)
        if ip_version == 4:
            ip = packet.ip
        elif ip_version == 6:
            ip = packet.ipv6

        for list1 in flow:
            if packet.transport_layer == 'TCP':
                list2 = [ip.src, packet.tcp.srcport, ip.dst, packet.tcp.dstport, 'tcp']
            elif packet.transport_layer == 'UDP':
                list2 = [ip.src, packet.udp.srcport, ip.dst, packet.udp.dstport, 'udp']

            result = cmp(list1, list2)
            if result == 0:
                break

        if result != 0:
            flow.append(list2)
            count += 1

    return count


# @entry_exit
def index_packets(index, flow, capture):
    ip = None
    max_len, max_delta, min_len, min_delta, tot_len, tot_delta, count = 0, 0, 999999999, 999999999, 0, 0, 0
    epoch, ciel = 0, 1
    list2 = []

    length_file = open('flows/flow{}-length.csv'.format(index), 'w')
    count_file = open('flows/flow{}-count.csv'.format(index), 'w')
    delta_file = open('flows/flow{}-delta.csv'.format(index), 'w')

    length_file.write("\"{}:{}->{}:{}\",\"{}\"".format(flow[index][0], flow[index][1], flow[index][2], flow[index][3],
                                                       flow[index][4]))
    count_file.write("\"{}:{}->{}:{}\",\"{}\"".format(flow[index][0], flow[index][1], flow[index][2], flow[index][3],
                                                      flow[index][4]))
    delta_file.write("\"{}:{}->{}:{}\",\"{}\"".format(flow[index][0], flow[index][1], flow[index][2], flow[index][3],
                                                      flow[index][4]))

    length_file.write("\n\"epoch time\",\"max\",\"mean\",\"min\"\n")
    count_file.write("\n\"epoch time\",\"packet count\"\n")
    delta_file.write("\n\"epoch time\",\"max\",\"mean\",\"min\"\n")

    for packet in capture:
        ip_version = get_ip_version(packet)
        if ip_version == 4:
            ip = packet.ip
        elif ip_version == 6:
            ip = packet.ipv6

        epoch = int(float(packet.frame_info.time_relative))

        check = epoch + 1.0

        if packet.transport_layer == 'TCP':
            list2 = [ip.src, packet.tcp.srcport, ip.dst, packet.tcp.dstport, 'tcp']
        elif packet.transport_layer == 'UDP':
            list2 = [ip.src, packet.udp.srcport, ip.dst, packet.udp.dstport, 'udp']

        if index == check_flow(flow, list2):
            count += 1
            min_len = min(min_len, int(packet.length))
            max_len = max(max_len, int(packet.length))
            tot_len += int(packet.length)
            min_delta = min(min_delta, float(packet.frame_info.time_delta))
            max_delta = max(max_delta, float(packet.frame_info.time_delta))
            tot_delta += float(packet.frame_info.time_delta)

        if check >= ciel:
            if index == check_flow(flow, list2):
                ciel = check + 1

                try:
                    mean_len = tot_len / count
                    mean_delta = tot_delta / count

                    length_file.write("{},{},{},{}\n".format(epoch,max_len,mean_len,min_len))
                    count_file.write("{},{}\n".format(epoch,count))
                    delta_file.write("{},{},{},{}\n".format(epoch,max_delta,mean_delta,min_delta))

                    tot_delta, tot_len = 0, 0
                    count, max_len, max_delta, min_len, min_delta = 0, 0, 0, 999999999, 999999999
                except:
                    pass

    length_file.close()
    count_file.close()
    delta_file.close()

    return


def main(argv):
    pcap_file = None
    filter_type = None
    flow = [[]]

    try:
        opts, args = getopt.getopt(argv, 'hp:f:', ["pcap", "filter"])
    except getopt.GetoptError:
        print "pcapAnalyze.py -p <file/path/to/pcap> -f <filter>"
        sys.exit(2)
    for opt, arg in opts:
        if opt == "-h":
            print "pcapAnalyze.py -p <file/path/to/pcap> -f <filter>"
            sys.exit(0)
        elif opt in ("-p", "--pcap"):
            pcap_file = arg
        elif opt in ("-f", "--filter"):
            filter_type = arg

    if pcap_file is None:
        print "You must specify a packet capture file"
        sys.exit(1)

    if filter_type is None:
        cap = pyshark.FileCapture(pcap_file)
    else:
        cap = pyshark.FileCapture(pcap_file, display_filter=filter_type)

    if not os.path.exists('flows'):
        os.mkdir('flows')

    # print cap[7]

    print "Identifying flows...\n"

    count = index_flows(flow, cap)

    print("Number of flows found: {0:d}".format(count))

    for x in range(1, count + 1):
        print flow[x]

    print "\nGathering statistics..."
    if count > 0:
        for x in range(1, count + 1):
            print("flow {0:d}...".format(x))
            index_packets(x, flow, cap)

    print "\nDone."
    return


if __name__ == '__main__':
    main(sys.argv[1:])

