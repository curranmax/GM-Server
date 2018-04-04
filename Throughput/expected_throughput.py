
from graph import getThroughputData


def getAngularSpeedData(filename):
	f = open(filename)

	speeds = []

	for line in f:
		spl = line.split()
		if spl[0] == 'time':
			continue

		for v in spl[1:3]:
			v = float(v) * 1000.0
			speeds.append(v)

	return speeds

def computePercentFull(tput_data, speed, full_throughput):
	return (float(len([v for v in tput_data if v.average_rate >= full_throughput])) / float(len(tput_data)), speed)

if __name__ == '__main__':
	full_throughput = 9.4

	d10_0  = computePercentFull(getThroughputData('1-7-throughput/5:16_0mrad.txt'), 0, full_throughput)
	d10_1  = computePercentFull(getThroughputData('1-7-throughput/5:28_1mrad.txt'), 1, full_throughput)
	d10_25 = computePercentFull(getThroughputData('1-7-throughput/5:40_2.5mrad.txt'), 2.5, full_throughput)
	d10_4  = computePercentFull(getThroughputData('1-7-throughput/5:50_4mrad.txt'), 4, full_throughput)
	d10_5  = computePercentFull(getThroughputData('1-7-throughput/6:53_5mrad.txt'), 5, full_throughput)
	d10_6  = computePercentFull(getThroughputData('1-7-throughput/6:26_6mrad.txt'), 6, full_throughput)
	d10_7  = computePercentFull(getThroughputData('1-26-throughput/5:38_7mrad_sfp2_filter.txt'), 7, full_throughput)

	tput_data = [d10_0, d10_1, d10_25, d10_4, d10_5, d10_6, d10_7]

	j_data = getAngularSpeedData("../vibration_analysis/data/All Data - J.tsv")
	s10_data = getAngularSpeedData("../vibration_analysis/data/All Data - S10.tsv")
	s60_data = getAngularSpeedData("../vibration_analysis/data/All Data - S60.tsv")
	top1_data = getAngularSpeedData("../vibration_analysis/data/All Data - test1top.tsv")
	top2_data = getAngularSpeedData("../vibration_analysis/data/All Data - test2top.tsv")
	top3_data = getAngularSpeedData("../vibration_analysis/data/All Data - test3top.tsv")

	all_speeds = j_data + s10_data + s60_data + top1_data + top2_data + top3_data

	sum_tput = 0.0
	for v in all_speeds:
		for p, s in tput_data:
			if s >= abs(v):
				break
		sum_tput += p

	print "Expected full percent:", sum_tput / len(all_speeds)
	print "Above 6 mrad/s:", float(len([v for v in all_speeds if v >= 5.0])) / float(len(all_speeds))
