
from math import exp, sqrt
import matplotlib.pyplot as plt
from random import random

def calcGaussian(x, a, d, s):
	x, a, d, s = map(float, (x, a, d, s))
	return a * exp(-pow(x - d, 2) / (2 * pow(s, 2)))

def simulatePIDController(simulation_range, pos_data, neg_data, show_plot = False):
	for k_proportional in xrange(1,101,10):
		k_proportional = float(k_proportional)
		paths = {}
		for start_x in xrange(-simulation_range, simulation_range + 1, 1):

			cur_x = start_x
			path = [cur_x]
			while path[-1] not in path[:-1]:
				response = k_proportional * (neg_data[cur_x - 100] - pos_data[cur_x - 100])
				cur_x += int(round(response))
				path.append(cur_x)
			paths[start_x] = path

		sum_iters = 0.0
		sum_dist = 0.0
		num_used = 0
		max_affected = None
		min_affected = None
		for start_x in paths:
			if len(paths[start_x]) == 2:
				continue
			sum_iters += len(paths[start_x]) - 1
			sum_dist += abs(paths[start_x][-1])
			num_used += 1
			if max_affected == None or max_affected < start_x:
				max_affected = start_x
			if min_affected == None or min_affected > start_x:
				min_affected = start_x
		print k_proportional, sum_iters / num_used, sum_dist / num_used, max_affected, min_affected
		if show_plot:
			plt.plot([start_x for start_x in paths], [paths[start_x][-1] for start_x in paths])
			plt.show()

def simulateDataDrivenController(simulation_range, pos_model_data, neg_model_data, pos_sim_data, neg_sim_data, show_plot = False, epsilon = 0.001):
	model = {x:(pos_model_data[x - 100], neg_model_data[x - 100]) for x in xrange(-simulation_range, simulation_range + 1, 1)}

	paths = {}
	for start_x in xrange(-simulation_range, simulation_range + 1, 1):
		cur_x = start_x
		path = [cur_x]
		while path[-1] not in path[:-1]:
			if cur_x > simulation_range or cur_x < -simulation_range:
				path.append(path[-1])
				continue
			this_volt = (pos_sim_data[cur_x - 100], neg_sim_data[cur_x - 100])
			best_k = None
			best_volt_dist = None
			for k in model:
				this_volt_dist = sqrt(sum(map(lambda x: pow(x[0] - x[1], 2), zip(this_volt, model[k]))))
				if best_k == None or \
						(best_volt_dist > epsilon and this_volt_dist > epsilon and
								(this_volt_dist < best_volt_dist or (this_volt_dist == best_volt_dist and abs(k) < abs(best_k)))) or \
						(best_volt_dist > epsilon and this_volt_dist <= epsilon) or \
						(best_volt_dist <= epsilon and this_volt_dist <= epsilon and abs(k) < abs(best_k)):
					best_k = k
					best_volt_dist = this_volt_dist
			cur_x += -best_k
			path.append(cur_x)
		paths[start_x] = path
	
	sum_iters = 0.0
	sum_dist = 0.0
	num_used = 0
	max_affected = None
	min_affected = None
	for start_x in paths:
		if len(paths[start_x]) == 2 or paths[start_x][-1] > simulation_range or paths[start_x][-1] < -simulation_range:
			continue
		sum_iters += len(paths[start_x]) - 1
		sum_dist += abs(paths[start_x][-1])
		num_used += 1
		if max_affected == None or max_affected < start_x:
			max_affected = start_x
		if min_affected == None or min_affected > start_x:
			min_affected = start_x
	print 'Avg iters | Avg dist | Num stable | Max & Min affected'
	print sum_iters / num_used, sum_dist / num_used, num_used, max_affected, min_affected
	if show_plot:
		plt.plot([start_x for start_x in paths], [paths[start_x][-1] for start_x in paths])
		plt.show()


def main(distance, sigma, amplitude, simulation_range):
	data = [calcGaussian(i, amplitude, distance, sigma) + calcGaussian(i, amplitude, -distance, sigma) for i in xrange(-simulation_range, simulation_range + 1, 1)]
	pos_data = [calcGaussian(i, amplitude, distance, sigma) for i in xrange(-simulation_range, simulation_range + 1, 1)]
	neg_data = [calcGaussian(i, amplitude, -distance, sigma) for i in xrange(-simulation_range, simulation_range + 1, 1)]

	plt.plot([i for i in xrange(-simulation_range, simulation_range + 1, 1)], pos_data)
	plt.plot([i for i in xrange(-simulation_range, simulation_range + 1, 1)], neg_data)
	plt.show()

	# Simulate Using PID controller
	simulatePIDController(simulation_range, pos_data, neg_data)

	model_noise_fact = 0.1
	pos_model_data = [x * (1 + random() * 2 * model_noise_fact - model_noise_fact) for x in pos_data]
	neg_model_data = [x * (1 + random() * 2 * model_noise_fact - model_noise_fact) for x in neg_data]
	
	plt.plot([i for i in xrange(-simulation_range, simulation_range + 1, 1)], pos_model_data)
	plt.plot([i for i in xrange(-simulation_range, simulation_range + 1, 1)], neg_model_data)
	plt.show()

	sim_noise_fact = 0.5
	pos_sim_data = [x * (1 + random() * 2 * sim_noise_fact - model_noise_fact) for x in pos_data]
	neg_sim_data = [x * (1 + random() * 2 * sim_noise_fact - model_noise_fact) for x in neg_data]

	plt.plot([i for i in xrange(-simulation_range, simulation_range + 1, 1)], pos_sim_data)
	plt.plot([i for i in xrange(-simulation_range, simulation_range + 1, 1)], neg_sim_data)
	plt.show()

	simulateDataDrivenController(simulation_range, pos_model_data, neg_model_data, pos_sim_data, neg_sim_data)

if __name__ == '__main__':
	main(20, 7.5, 1, 100)