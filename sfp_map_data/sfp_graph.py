
import argparse
import numpy as np
import math

from mpl_toolkits.mplot3d import Axes3D
from matplotlib import cm
from matplotlib.ticker import LinearLocator, FormatStrFormatter
import matplotlib.pyplot as plt

def tryConvert(v):
	if '.' in v:
		return float(v)
	else:
		return int(v)

def readInData(filename):
	f = open(filename)

	params = None
	col_hdrs = None

	# Key is (h_gm, v_gm) and value is RSSI
	data = {}

	for line in f:
		if line == '\n':
			continue

		spl = line.split()

		if spl[0]  == 'PARAMS':
			params = {a: int(b) for a, b in map(lambda x: x.split('='), spl[1:])}
		elif col_hdrs == None:
			col_hdrs = spl
		elif len(spl) == len(col_hdrs):
			this_line = {c: tryConvert(v) for c, v in zip(col_hdrs, spl)}
			data[(this_line['H_Delta'], this_line['V_Delta'])] = this_line['RSSI']
		else:
			print 'UNEXPECTED LINE:', spl

	return params, data

def generateXY(params):
	val_range = params['map_range']
	val_step = params['map_step']
	x = np.arange(-val_range, val_range, val_step)
	y = np.arange(-val_range, val_range, val_step)
	x, y = np.meshgrid(x, y)
	return x, y

def plotRSSI(params, data, plot_func = None, xy_func = None):
	x, y = generateXY(params)

	z = [[data[(xv, yv)] for xv, yv in zip(x_row, y_row)] for x_row, y_row in zip(x, y)]

	xy_units = ' GM units'
	if xy_func != None:
		x = np.array([[xy_func(xv) for xv in x_row] for x_row in x])
		y = np.array([[xy_func(yv) for yv in y_row] for y_row in y])
		xy_units = ' mrad'

	xy_lim = (min(np.amin(x), np.amin(y)), max(np.amax(x), np.amax(y)))
	
	if plot_func != None:
		plot_func(x, y, z, xy_units = xy_units, xy_lim = xy_lim, title = 'Plot of RSSI', zlabel = 'RSSI')

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


if __name__ == '__main__':
	parser = argparse.ArgumentParser(description = 'Graph SFP map output files')

	parser.add_argument('-df', '--data_file_name', metavar = 'FILE_NAME', type = str, nargs = 1, default = [None], help = 'File to plot')

	args = parser.parse_args()

	params, data = readInData(args.data_file_name[0])

	plotRSSI(params, data, plot_func = plot3D, xy_func = None)
