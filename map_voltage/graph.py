
from mpl_toolkits.mplot3d import Axes3D
from matplotlib import cm
from matplotlib.ticker import LinearLocator, FormatStrFormatter
import matplotlib.pyplot as plt
import numpy as np
import argparse
import math

from tracking_simulator import TrackingSimulator

def tryConvert(v):
	try:
		v = int(v)
	except ValueError:
		try:
			v = float(v)
		except ValueError:
			pass
	return v

def readInData(filename):
	f = open(filename)

	params = None
	col_hdrs = None

	# Key is (h_gm, v_gm) and value is (ph, nh, pv, nv)
	data = {}
	for line in f:
		if line == '\n':
			continue
		spl = line.split()
		if spl[0] == 'PARAMS':
			params = {a:tryConvert(b) for a,b in map(lambda x:x.split(':'),spl[1:])}
		elif col_hdrs == None:
			col_hdrs = spl
		elif len(spl) == len(col_hdrs):
			this_line = {c: tryConvert(v) for c, v in zip(col_hdrs, spl)}
			data[(this_line['hd'], this_line['vd'])] = (this_line['ph'], this_line['nh'], this_line['pv'], this_line['nv'])
		else:
			print 'UNEXPECTED LINE:', spl
	return params, data

def plot3D(x, y, z, xy_units = '', xy_lim = None, title = '', zlabel = '', subplot = None, show = True, fig = None):
	if fig == None:
		fig = plt.figure()
	if subplot == None:
		ax = fig.gca(projection = '3d')
	else:
		ax = fig.add_subplot(*subplot, projection = '3d')
	surf = ax.plot_surface(x, y, z, rstride = 1, cstride = 1, cmap = cm.coolwarm,
	                       linewidth = 0, antialiased = False)
	
	min_z = min(v for r in z for v in r)
	max_z = max(v for r in z for v in r)
	zlim = (math.floor(min_z * 100) / 100.0, math.ceil(max_z * 100) / 100)
	ax.set_zlim(*zlim)

	ax.zaxis.set_major_locator(LinearLocator(10))
	ax.zaxis.set_major_formatter(FormatStrFormatter('%.03f'))

	fig.colorbar(surf, shrink=0.5, aspect=5)
	ax.set_xlabel('Horizontal GM' + xy_units)
	ax.set_ylabel('Vertical GM' + xy_units)
	ax.set_zlabel(zlabel)
	plt.title(title)
	if xy_lim != None:
		ax.set_xlim(xy_lim)
		ax.set_ylim(xy_lim)

	if show:
		plt.show()

	return fig

def plotHeatMap(x, y, z, xy_units = '', xy_lim = None, title = '', zlabel = '', subplot = None, show = True, fig = None):
	if fig == None:
		fig = plt.figure()
	if subplot == None:
		ax = fig.gca()
	else:
		ax = fig.add_subplot(*subplot)
	heatmap = ax.pcolor(x, y, np.array(z), cmap = cm.BuPu)

	cbar = fig.colorbar(heatmap, shrink = .5, aspect = 5)
	ax.set_xlabel('Horizontal GM' + xy_units)
	ax.set_ylabel('Vertical GM' + xy_units)
	cbar.set_label(zlabel)
	ax.set_title(title)
	if xy_lim != None:
		ax.set_xlim(xy_lim)
		ax.set_ylim(xy_lim)
	if show:
		plt.show()

	return fig

def generateXY(params):
	if params['record_type'] != 'all':
		raise Exception('Unexpected data type')
	val_range = params['map_range']
	val_step = params['map_step']
	x = np.arange(-val_range, val_range, val_step)
	y = np.arange(-val_range, val_range, val_step)
	x, y = np.meshgrid(x, y)
	return x, y

voltage_index = {'ph':0, 'nh':1, 'pv':2, 'nv':3}
voltage_title = {'ph': 'Voltage of Positive Horizontal Diode',
					'nh': 'Voltage of Negative Horizontal Diode',
					'pv': 'Voltage of Positive Vertical Diode',
					'nv': 'Voltage of Negative Vertical Diode'}

def plotVoltage(params, data, diode_to_plot, plot_func = plot3D, xy_func = None):
	x, y = generateXY(params)
	z = [[data[(xv, yv)][voltage_index[diode_to_plot]] for xv, yv in zip(x_row, y_row)] for x_row, y_row in zip(x, y)]

	xy_units = ''
	if xy_func != None:
		x = np.array([[xy_func(xv) for xv in x_row] for x_row in x])
		y = np.array([[xy_func(yv) for yv in y_row] for y_row in y])
		xy_units = ' (mrad)'
		
	xy_lim = (min(np.amin(x), np.amin(y)), max(np.amax(x), np.amax(y)))
	print xy_lim
	plot_func(x, y, z, xy_units = xy_units, xy_lim = xy_lim, title = voltage_title[diode_to_plot], zlabel = 'Voltage')

def computeResponse(pos_val, neg_val, kp):
	return kp * (neg_val - pos_val)

response_title = {'h': 'Response of Tracking System for Horizontal GM',
					'v': 'Response of Tracking Sytem for Vertical GM'}

def plotResponse(params, data, dimension, kp = None, plot_func = plot3D):
	pos = 'p' + dimension
	neg = 'n' + dimension
	x, y = generateXY(params)

	kp = (params['kp'] if kp == None else kp)
	z = [[computeResponse(data[(xv, yv)][voltage_index[pos]], data[(xv, yv)][voltage_index[neg]], kp) for xv, yv in zip(x_row, y_row)] for x_row, y_row in zip(x, y)]

	plot_func(x, y, z, title = response_title[dimension] + ' with kp of ' + str(kp), zlabel = 'Response')

def plotTracking(params, data, kp = None, plot_func = plot3D):
	kp = (params['kp'] if kp == None else kp)
	map_step = params['map_step']
	map_range = params['map_range']

	x, y = generateXY(params)
	tracking_sims = [[TrackingSimulator(xv, yv, kp, data, map_step, map_range) for xv, yv in zip(x_row, y_row)] for x_row, y_row in zip(x, y)]
	z = map(lambda row: map(lambda x: x.numIters(), row), tracking_sims)

	fig = plot_func(x, y, z, title =  'Number of Iterations till Steady State with kp of ' + str(kp), zlabel = 'Number of iterations', subplot = (1, 3, 1), show = False)

	z = map(lambda row: map(lambda x: x.distanceFromCenter(0, 0), row), tracking_sims)
	plot_func(x, y, z, title = 'Distance between Steady State and (0, 0)', zlabel = 'Distance from (0,0)', subplot = (1, 3, 2), show = False, fig = fig)

	num_steady_loc = {(xv, yv):0 for x_row, y_row in zip(x, y) for xv, yv in zip(x_row, y_row)}
	steady_loc = [v.steadyLocation(0, 0) for row in tracking_sims for v in row if v.steadyLocation(0, 0) in num_steady_loc]
	for v in steady_loc:
		num_steady_loc[v] += 1.0 / len(steady_loc)

	z = [[num_steady_loc[(xv, yv)] for xv, yv in zip(x_row, y_row)] for x_row, y_row in zip(x, y)]
	plot_func(x, y, z, title = 'Percent ended at steady state', zlabel = 'Percent ended on', subplot = (1, 3, 3), show = True, fig = fig)

def plotAllDiodes(params, data, plot_func = plotHeatMap):
	x, y = generateXY(params)

	# combine_func = max
	combine_func = lambda x: max(x) if max(x) >= 0 else -1
	# combine_func = lambda x: x.index(max(x)) if max(x) >= 0 else -1
	print 'Num non-null-locs:', len([1 for x_row, y_row in zip(x, y) for xv, yv in zip(x_row, y_row) if any(v >= 0.0 for v in data[(xv, yv)])])

	z = [[combine_func(data[(xv, yv)]) for xv, yv in zip(x_row, y_row)] for x_row, y_row in zip(x, y)]

	plot_func(x, y, z, title = 'Maximum voltage of any diode', zlabel = 'Max power output')

if __name__ == '__main__':
	parser = argparse.ArgumentParser(description = 'Graph Diode map output files')

	parser.add_argument('-df', '--data_file_name', metavar = 'FILE_NAME', type = str, nargs = 1, default = [None], help = 'File to plot')

	parser.add_argument('-kp', '--k_proportional', metavar = 'KP', type = float, nargs = 1, default = [None], help = 'If supplied over writes the tests kp')

	parser.add_argument('-3d','--three_dimensional_plot',help = 'Plot data as a 3d surface',action = 'store_true')
	parser.add_argument('-hm','--heat_map_plot',help = 'Plot data as a heatmap',action = 'store_true')	

	parser.add_argument('-ph','--ph_diode',help = 'Graph 3d plot of the outputted volage of the positive horizontal diode',action = 'store_true')
	parser.add_argument('-nh','--nh_diode',help = 'Graph 3d plot of the outputted volage of the negative horizontal diode',action = 'store_true')
	parser.add_argument('-pv','--pv_diode',help = 'Graph 3d plot of the outputted volage of the positive vertical diode',action = 'store_true')
	parser.add_argument('-nv','--nv_diode',help = 'Graph 3d plot of the outputted volage of the negative vertical diode',action = 'store_true')
	parser.add_argument('-md','--max_diodes', help = 'Graph the maximum outputted volage of the diodes', action = 'store_true')
	parser.add_argument('-hr','--horizontal_response',help = 'Plot the horiozontal response',action = 'store_true')
	parser.add_argument('-vr','--vertical_response',help = 'Plot the vertical response',action = 'store_true')
	parser.add_argument('-ts','--tracking_simulation',help = 'Plot the number of iterations the tracking system would take to reach a steady state',action = 'store_true')
	
	parser.add_argument('-ad','--actual_distance', metavar = 'AD', type = float, nargs = 1, default = [None], help = 'The distance of the link. If given, shows X+Y dimensions in mrad')
	parser.add_argument('-sd','--simulated_distance', metavar = 'SD', type = float, nargs = 1, default = [None], help = 'The distance to simulate the data for. If given, shows X+Y dimensions in mrad at sd m that have equivalent displacement as ad m')

	args = parser.parse_args()

	if not args.three_dimensional_plot and args.heat_map_plot:
		plot_func = plotHeatMap
	else:
		plot_func = plot3D		

	params, data = readInData(args.data_file_name[0])

	actual_distance = args.actual_distance[0]
	simulated_distance = args.simulated_distance[0]
	if actual_distance == None:
		xy_func = None
	if actual_distance != None and simulated_distance == None:
		# Converts GMVal to mrad
		xy_func = lambda v: float(v) * 40.0 / 65536.0 * np.pi / 180.0 * 1000
	if actual_distance != None and simulated_distance != None:
		# Converts GMVal to mrad at ad meters
		mrad_at_ad = lambda v: float(v) * 40.0 / 65536.0 * np.pi / 180.0 * 1000
		# Converts GMVal to displacement at ad meters
		displacement_at_ad = lambda v: math.tan(mrad_at_ad(v) / 1000.0) * actual_distance
		# Converts GMVal to equivalent mrad at sd meters
		mrad_at_sd = lambda v: math.atan(displacement_at_ad(v) / simulated_distance) * 1000.0
		xy_func = mrad_at_sd

	if args.ph_diode:
		plotVoltage(params, data, 'ph', plot_func = plot_func, xy_func = xy_func)
	if args.nh_diode:
		plotVoltage(params, data, 'nh', plot_func = plot_func, xy_func = xy_func)
	if args.pv_diode:
		plotVoltage(params, data, 'pv', plot_func = plot_func, xy_func = xy_func)
	if args.nv_diode:
		plotVoltage(params, data, 'nv', plot_func = plot_func, xy_func = xy_func)
	if args.max_diodes:
		plotAllDiodes(params, data, plot_func = plot_func)
	if args.horizontal_response:
		plotResponse(params, data, 'h', kp = args.k_proportional[0], plot_func = plot_func)
	if args.vertical_response:
		plotResponse(params, data, 'v', kp = args.k_proportional[0], plot_func = plot_func)
	if args.tracking_simulation:
		plotTracking(params, data, kp = args.k_proportional[0], plot_func = plot_func)
