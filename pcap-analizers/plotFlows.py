import matplotlib.pyplot as plt
import numpy as np
import sys, os, getopt
from matplotlib import pyplot


def r_squared(actual, ideal):
    actual_mean = np.mean(actual)
    ideal_dev = np.sum([(val - actual_mean)**2 for val in ideal])
    actual_dev = np.sum([(val - actual_mean)**2 for val in actual])

    return ideal_dev / actual_dev


def read_delta(file_name):
    dtypes = np.dtype({'names': ('epoch time', 'max delta', 'mean delta', 'min delta'),
                       'formats': [np.int, np.float, np.float, np.float]})
    data = np.loadtxt(file_name, delimiter=',', skiprows=2, dtype=dtypes)

    return data


def read_length(file_name):
    dtypes = np.dtype({'names': ('epoch time', 'max length', 'mean length', 'min length'),
                       'formats': [np.int, np.int, np.int, np.int]})
    data = np.loadtxt(file_name, delimiter=',', skiprows=2, dtype=dtypes)

    return data


def read_count(file_name):
    dtypes = np.dtype({'names': ('epoch time', 'packet count'),
                       'formats': [np.int, np.int]})
    data = np.loadtxt(file_name, delimiter=',', skiprows=2, dtype=dtypes)

    return data


def count_plot(data):
    epoch = data['epoch time']
    count = data['packet count']

    fig = pyplot.figure()
    pyplot.title('Flow packet count')
    pyplot.ylabel('Packet Count')
    pyplot.xlabel('Epoch Time')

    pyplot.plot(epoch, count, marker='o')

    return fig


def delta_plot(data):
    epoch = data['epoch time']
    min_delta = data['min delta']
    mean_delta = data['mean delta']
    max_delta = data['max delta']

    fig = pyplot.figure()
    pyplot.title('Delta Time of the Previous Packet(max/min)')
    pyplot.ylabel('Delta Time')
    pyplot.xlabel('Epoch Time')

    delta_err = np.row_stack((mean_delta - min_delta, max_delta - mean_delta))

    pyplot.errorbar(epoch, mean_delta, marker='o', yerr=delta_err)

    #slope, intercept = np.polyfit(epoch, mean_delta, 1)
    #ideal_delta = intercept + (slope * epoch)
    #r_sq = r_squared(mean_delta, ideal_delta)

    #fit_label = 'Linear fit ({0:2f})'.format(slope)
    #pyplot.plot(epoch,ideal_delta, color='red', linestyle='--', label=fit_label)
    #pyplot.annotate('r^2 = {0:2f}'.format(r_sq), (0.05, 0.9), xycoords='axes fraction')
    #pyplot.legend(loc='lower right')

    return fig


def length_plot(data):
    epoch = data['epoch time']
    min_length = data['min length']
    mean_length = data['mean length']
    max_length = data['max length']

    fig = pyplot.figure()
    pyplot.title('Packet Length per Second(max/min)')
    pyplot.ylabel('Packet Length')
    pyplot.xlabel('Epoch Time')

    length_err = np.row_stack((mean_length - min_length, max_length - mean_length))

    pyplot.errorbar(epoch, mean_length, marker='o', yerr=length_err)

    return fig


def main(argv):
    delta_data = None
    length_data = None
    count_data = None

    try:
        opts, args = getopt.getopt(argv, "hd:l:c:", ["delta=", "length=", "count="])
    except getopt.GetoptError:
        print "plotFlows.py -d <delta/file> -l <length/file> -c <count/file>"
        sys.exit(2)
    for opt, arg in opts:
        if opt == "-h":
            print "plotFlows.py -d <delta/file> -l <length/file> -c <count/file>"
            sys.exit(0)
        elif opt in ("-d", "--delta"):
            delta_data = read_delta(arg)
        elif opt in ("-l", "--length"):
            length_data = read_length(arg)
        elif opt in ("-c", "--count"):
            count_data = read_count(arg)

    if not os.path.exists('plots'):
        os.mkdir('plots')

    print "Plotting...\n"

    if delta_data is not None:
        delta_fig = delta_plot(delta_data)
        delta_fig.savefig('plots/delta_per_sec.png')

    if length_data is not None:
        length_fig = length_plot(length_data)
        length_fig.savefig('plots/length_per_sec.png')

    if count_data is not None:
        count_fig = count_plot(count_data)
        count_fig.savefig('plots/count_per_sec.png')

    print "Done."


if __name__ == '__main__':
    main(sys.argv[1:])

# print delta_data
# print length_data
# print count_data
