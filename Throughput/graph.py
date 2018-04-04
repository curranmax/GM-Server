
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
		if 'sec' in line and not line.startswith('Average'):
			line_no_id = line[line.find(']') + 1:]
			time_full_str  = line_no_id[:line_no_id.find('sec')]
			time_no_ws = ''.join(time_full_str.split())
			start_time, end_time = map(float, time_no_ws.split('-'))
			dt_str, dt_units, ar_str, ar_units, retry, cwnd, cwnd_units = line_no_id[line_no_id.find('sec') + 3:].split()
			data_transfered, average_rate = map(float, (dt_str, ar_str))

			if dt_units == 'GBytes':
				pass
			elif dt_units == 'MBytes':
				data_transfered /= 1000.0
			elif dt_units == 'Bytes':
				data_transfered /= 1000.0 * 1000.0 * 1000.0
			else:
				raise Exception('Unknown dt_units value: ' + dt_units)

			if ar_units == 'Gbits/sec':
				pass
			elif ar_units == 'Mbits/sec':
				average_rate /= 1000.0
			elif ar_units == 'bits/sec':
				average_rate /= 1000.0 * 1000.0 * 1000.0
			else:
				raise Exception('Unknown ar_units value: ' + ar_units)

			data.append(ThroughputResult(start_time, end_time, data_transfered, average_rate))
	return data

def plotCDF(datas, num_bins = 7):
	for data in datas:
		d = [tr.average_rate for tr in data if tr.end_time - tr.start_time <= 2.1]
		n, bins, patches = plt.hist(d, num_bins, normed = 1, histtype = 'step', cumulative = True)

		for x, y in zip(bins, n):
			print x, y

	plt.xlabel('Throughput (Gbits/sec)')
	plt.ylabel('CDF')
	plt.xlim([0.0, 10.0])

	plt.title('CDF of Throughput without Tracking')

	plt.show()

def writeDataOut(datas, fname, first_line):
	f = open(fname, 'w')
	f.write(first_line)

	k = max(map(len, datas))
	for x in datas:
		x += [None] * (k - len(x))
	
	for row in zip(*datas):
		f.write('\t'.join([str(tr.average_rate if tr != None else '') for tr in row]) + '\n')

if __name__ == '__main__':
	# d25_s = getThroughputData('11-13-throughput/11:13_static.txt')
	# d25_0 = getThroughputData('11-13-throughput/11:27_0mrad.txt')
	# d25_5 = getThroughputData('11-13-throughput/11:30_0.5mrad.txt')
	# d25_1 = getThroughputData('11-13-throughput/12:27_1mrad.txt')

	# writeDataOut([d25_s, d25_0, d25_5, d25_1], '25m_throughput.txt', 'static\t0 mrad/s\t0.5 mrad/s\t1 mrad/s\n')

	# d2_s = getThroughputData('1-6-throughput/1:43_static.txt')
	# d2_0 = getThroughputData('1-6-throughput/2:28_0mrad.txt')
	# d2_1 = getThroughputData('1-6-throughput/2:46_1mrad.txt')
	# d2_5 = getThroughputData('1-6-throughput/3:02_5mrad.txt')
	# d2_7 = getThroughputData('1-6-throughput/5:09_7mrad.txt')

	# writeDataOut([d2_s, d2_0, d2_1, d2_5, d2_7], '2m_throughput.txt', 'static\t0 mrad/s\t1 mrad/s\t5 mrad/s\t7 mrad/s\n')

	# d10_0  = getThroughputData('1-7-throughput/5:16_0mrad.txt')
	# d10_1  = getThroughputData('1-7-throughput/5:28_1mrad.txt')
	# d10_25 = getThroughputData('1-7-throughput/5:40_2.5mrad.txt')
	# d10_4  = getThroughputData('1-7-throughput/5:50_4mrad.txt')
	# d10_5  = getThroughputData('1-7-throughput/6:53_5mrad.txt')
	# d10_6  = getThroughputData('1-7-throughput/6:26_6mrad.txt')
	# d10_7  = getThroughputData('1-26-throughput/5:38_7mrad_sfp2_filter.txt')

	# writeDataOut([d10_0, d10_1, d10_25, d10_4, d10_5, d10_6, d10_7],
	# 				'10m_throughput.txt',
	# 				'0 mrad/s\t1 mrad/s\t2.5 mrad/s\t4 mrad/s\t5 mrad/s\t6 mrad/s\t7 mrad/s\n')

	dNew_s = getThroughputData('3-3-throughput/4:50_static.txt')
	dNew_0 = getThroughputData('3-3-throughput/5:37_0mrad.txt')
	dNew_1 = getThroughputData('3-3-throughput/5:39_1mrad.txt')
	dNew_25 = getThroughputData('3-3-throughput/5:41_2.5mrad.txt')
	dNew_5 = getThroughputData('3-3-throughput/5:42_5mrad.txt')
	dNew_7 = getThroughputData('3-3-throughput/5:44_7mrad.txt')
	writeDataOut([dNew_s, dNew_0, dNew_1, dNew_25, dNew_5, dNew_7],
					'new_sfp_throughput.txt',
					'Static\t0 mrad/s\t1 mrad/s\t2.5 mrad/s\t5 mrad/s\t7 mrad/s\n')

