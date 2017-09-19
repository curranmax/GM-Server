
import graph as GRAPH
from math import pi, sqrt
import matplotlib.pyplot as plt

def millisecondToSecond(v):
	return v / 1000.0

def secondToMillisecond(v):
	return v * 1000.0

def milliradianToRadian(v):
	return v / 1000.0

def radianToMilliradian(v):
	return v * 1000.0

def mradPerMsToMradPerS(v):
	return v * 1000.0

def convertMilliradianToGMVal(mrad):
	# To simulate longer/shorter links this is where the change needs to be made!!!!
	approx_gm_val = milliradianToRadian(mrad) * 180.0 / pi * 65536.0 / 40.0
	return int(round(approx_gm_val))

def convertGMValToMilliradian(gm_val):
	return radianToMilliradian(gm_val * 40.0 / 65536.0 * pi / 180.0)

# This the tracking system algorithm
def computeResponse(cur_location, data, k_p):
	gm_val = tuple(map(convertMilliradianToGMVal, cur_location))
	
	# Assume that data has a granularity of 1
	if gm_val not in data:
		return None
	ph, nh, pv, nv = data[gm_val]
	hr = k_p * (nh - ph)
	vr = k_p * (nv - pv)

	# Convert responses to rotation in radians
	response = (convertGMValToMilliradian(hr), convertGMValToMilliradian(vr))
	return response

# Data is dict with key of (GMVal, GMVal) and value of (ph, nh, pv, nv) (all in volts)
# k_p is proportional constant
# angular_tolerance is the angular tolerance of the link in mrad
# latency_per_iteration is the length of each iteration of the tracking system in milliseconds
# rotation_rate is the rate (in mrad/ms) that the transmitter is rotating. The rotation is assumed to be in the positive horizontal direction.
def isRateStable(data, k_p, angular_tolerance, latency_per_iteration, num_iters_to_test, rotation_rate):
	cur_location = [0., 0.] # Current localtion (in mrad away from origin)
	origin = (0.0792090699153, 0.19703097663)

	# Should normalize to magnitude of 1.0
	rotation_direction = (-1., 0.)
	mag = sqrt(sum(x * x for x in rotation_direction))
	rotation_direction = tuple(map(lambda x: x / mag, rotation_direction))

	sum_h = 0.0
	sum_v = 0.0
	buffer_iters = 10

	cur_time = 0.0
	num_good = 0
	num_bad = 0
	for i in range(num_iters_to_test):
		# TODO check if cur_location is within angular_tolerance of "center"

		# In ms
		cur_time += latency_per_iteration

		# Calculate how far the system rotated in "latency_per_iteration" time
		rot_x = rotation_direction[0] * rotation_rate * latency_per_iteration
		rot_y = rotation_direction[1] * rotation_rate * latency_per_iteration
		# print rot_x, rot_y
		
		# Rotate cur_location in opposite direction of overall rotation
		cur_location[0] -= rot_x
		cur_location[1] -= rot_y

		if i >= buffer_iters:
			angle_between_cur_and_origin = sqrt(sum((c - o) * (c - o) for c, o in zip(cur_location, origin)))
			if angle_between_cur_and_origin > angular_tolerance:
				num_bad += 1
			else:
				num_good += 1
			# if angle_between_cur_and_origin > angular_tolerance:
			# 	print 'Tolerance'
			# 	return False

		# Compute what the tracking will do
		response = computeResponse(cur_location, data, k_p)
		if response == None:
			print 'Lost'
			num_bad += num_iters_to_test - i
			return float(num_good) / float(num_good + num_bad)

		# Adjust cur_location with tracking adjustments
		cur_location[0] += response[0]
		cur_location[1] += response[1]

		sum_h += cur_location[0]
		sum_v += cur_location[1]
	
	# print sum_h / num_iters_to_test, sum_v / num_iters_to_test
	print num_good, num_bad, float(num_good) / float(num_good + num_bad)
	return float(num_good) / float(num_good + num_bad)

if __name__ == '__main__':
	# Parameters
	k_p = 10.0
	angular_tolerance = .2 # in mrad
	latency_per_iteration = 2 + 4.3 + .1 # in ms
	num_iters_to_test = 100000

	# Data key is (GMVal, GMVal), value is (ph, nh, pv, nv)
	params, data = GRAPH.readInData('data/long_link1/map_100_1.txt')

	print 'Test length:', millisecondToSecond(num_iters_to_test * latency_per_iteration), 'seconds'
	x = []
	y = []
	for k in [.1, .2, .3, .4, .5, .6, .7, .8, .9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0]:
		percent_good = isRateStable(data, k_p, angular_tolerance, latency_per_iteration, num_iters_to_test, k * angular_tolerance / latency_per_iteration)
		print percent_good
		if percent_good != None:
			x.append(mradPerMsToMradPerS(k * angular_tolerance / latency_per_iteration))
			y.append(percent_good)

	plt.plot(x, y, 's-')
	plt.xlabel('Rate of Angular Movement (mrad/sec)')
	plt.ylim([0,1.05])
	plt.ylabel('Fraction of Successful TP Iterations')
	plt.show()