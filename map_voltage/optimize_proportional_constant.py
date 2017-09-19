
from tracking_simulator import TrackingSimulator
from graph import readInData, generateXY
import numpy as np
import argparse
from math import sqrt
from scipy.cluster.hierarchy import linkage, fcluster

def optimizeProportionalConstant(data_file, gm_range, min_kp, max_kp, step_kp, iters_factor, distance_factor):
	params, data = readInData(data_file)
	x, y = generateXY(params)

	map_step = params['map_step']
	map_range = params['map_range']

	non_null_locs = [(xv, yv) for x_row, y_row in zip(x, y) for xv, yv in zip(x_row, y_row)
							if any(v > 0.0 for v in data[(xv, yv)])
								and (gm_range == None or (abs(xv) <= gm_range and abs(yv) <= gm_range))]

	best_kp = None
	min_val = None

	print 'Kp, Avg iters, Avg dist, Total score'
	try:
		for kp in np.arange(min_kp, max_kp, step_kp):
			tracking_sims = [TrackingSimulator(xv, yv, kp, data, map_step, map_range) for xv, yv in non_null_locs]
			if any(ts.is_unstable() for ts in tracking_sims):
				break

			avg_num_iters = sum(ts.numIters() for ts in tracking_sims) / float(len(tracking_sims))

			steady_locs = [ts.steadyLocation(0,0) for ts in tracking_sims]
			avg_x = sum(x for x, y in steady_locs) / float(len(steady_locs))
			avg_y = sum(y for x, y in steady_locs) / float(len(steady_locs))

			Z = linkage(steady_locs)
			max_d = 4
			clusters = fcluster(Z, max_d)
			print clusters

			avg_distance_to_center = sum(ts.distanceFromCenter(0,0) for ts in tracking_sims) / float(len(tracking_sims))
			this_val = iters_factor * avg_num_iters + distance_factor * avg_distance_to_center
			print kp, avg_num_iters, avg_distance_to_center, this_val

			if min_val == None or min_val > this_val:
				min_val = this_val
				best_kp = kp
	except KeyboardInterrupt:
		print 'Ended prematurely!'

	print 'Optimal kp', best_kp, '  (with val of', min_val, ')'

if __name__ == '__main__':
	parser = argparse.ArgumentParser(description = 'Graph Diode map output files')

	parser.add_argument('-file', '--file_name', metavar = 'FILE_NAME', type = str, nargs = 1, default = [None], help = 'File to plot')

	# Range to try search parameters for
	parser.add_argument('-gm_range', '--gm_range_to_test_on', metavar = 'GM_RANGE', type = int, nargs = 1, default = [None], help = 'Only simulates tracking for values with a magnitude less than the supplied value')

	# Range to search kp over
	parser.add_argument('-min_kp', '--min_k_proportional', metavar = 'KP', type = float, nargs = 1, default = [0], help = 'Min Kp to try')
	parser.add_argument('-max_kp', '--max_k_proportional', metavar = 'KP', type = float, nargs = 1, default = [0], help = 'Max Kp to try')
	parser.add_argument('-step_kp', '--step_k_proportional', metavar = 'KP', type = float, nargs = 1, default = [0], help = 'Increment to get new Kps')
	
	# Optimization function
	parser.add_argument('-if', '--iters_factor', metavar = 'IF', type = float, nargs = 1, default = [1.0], help = 'Factor for iter element of optimization function')
	parser.add_argument('-df', '--distance_factor', metavar = 'DF', type = float, nargs = 1, default = [1.0], help = 'Factor for distance element of optimization function')
	
	args = parser.parse_args()

	optimizeProportionalConstant(args.file_name[0],
									args.gm_range_to_test_on[0],
									args.min_k_proportional[0], args.max_k_proportional[0], args.step_k_proportional[0],
									args.iters_factor[0], args.distance_factor[0])