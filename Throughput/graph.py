
import matplotlib.pyplot as plt

class ThroughputResult:
	def __init__(self, start_time, end_time, data_transfered, average_rate):
		self.start_time = start_time
		self.end_time = end_time
		self.data_transfered = data_transfered # In Giga bytes
		self.average_rate = average_rate # In Giga bits

def getThroughputData(filename):
	f = open(filename)

	# List of ThroughputResult
	data = []
	for line in f:
		if 'sec' in line:
			line_no_id = line[line.find(']') + 1:]
			time_full_str  = line_no_id[:line_no_id.find('sec')]
			time_no_ws = ''.join(time_full_str.split())
			start_time, end_time = map(float, time_no_ws.split('-'))
			dt_str, dt_units, ar_str, ar_units = line_no_id[line_no_id.find('sec') + 3:].split()
			data_transfered, average_rate = map(float, (dt_str, ar_str))

			if dt_units == 'MBytes':
				data_transfered /= 1000.0
			if ar_units == 'Mbits/sec':
				average_rate /= 1000.0

			data.append(ThroughputResult(start_time, end_time, data_transfered, average_rate))
	return data

def plotCDF(data, num_bins = 100):
	d = [tr.average_rate for tr in data if tr.end_time - tr.start_time <= 2.1]
	plt.hist(d, num_bins, normed = 1, histtype = 'step', cumulative = True)

	plt.xlabel('Throughput (Gbits/sec)')
	plt.ylabel('CDF')
	plt.xlim([0.0, 10.0])

	plt.title('CDF of Throughput without Tracking')

	plt.show()

if __name__ == '__main__':
	
	filename = 'data/static2.txt'

	data = getThroughputData(filename)

	plotCDF(data)