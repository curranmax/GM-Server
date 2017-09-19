
import argparse
import graph as GPH
from math import pi, exp, tan, log, sqrt

import scipy.optimize as opt
import numpy as np

def gaussian(k, a, x_0, y_0, sig_x, sig_y):
	x, y = k
	return (a * np.exp(-((x - x_0)**2 / (2 * sig_x * sig_x) + (y - y_0)**2 / (2 * sig_y * sig_y)))).ravel()

def simpGaussian(k, a, x_0, y_0, sig_x, sig_y):
	x, y = k
	return a * exp(-((x - x_0)**2 / (2 * sig_x * sig_x) + (y - y_0)**2 / (2 * sig_y * sig_y)))

def calcError(data, a, x_0, y_0, sig_x, sig_y):
	sum_err = 0.0
	for x_i, y_i in data:
		z_i = data[(x_i, y_i)]
		sum_err += pow(z_i - simpGaussian((x_i, y_i), a, x_0, y_0, sig_x, sig_y), 2)
	return sum_err

def getGaussianParameters(data, gm_settings = False, init_a = None, init_x_0 = None, init_y_0 = None, init_sig_x = None, init_sig_y = None):
	# data is key (x_i, y_i) and value of z_i
	x = np.array(sorted(list(set(x for x, y in data))))
	y = np.array(sorted(list(set(y for x, y in data))))
	new_data = np.array([[data[(xv,yv)] for xv in x] for yv in y])
	x, y = np.meshgrid(x, y)

	# Goal is to find values of A, x_0, y_0, sig_x, and sig_y, such that sum i=1 to n (z_i - A * exp(-((x_i - x_0)^2 / (2 * sig_x^2) + (y_i - y_0)^2 / (2 * sig_y^2))))^2 is minimized
	if gm_settings:
		a = 0.6 if init_a == None else init_a
		x_0 = -10.0 if init_x_0 == None else init_x_0
		y_0 = -10.0 if init_y_0 == None else init_y_0
		sig_x = 10.0 if init_sig_x == None else init_sig_x
		sig_y = 10.0 if init_sig_y == None else init_sig_y
	else:
		a = 0.6
		x_0 = 0.0
		y_0 = 0.0
		sig_x = -.027
		sig_y = -.027
	# best_error = calcError(data, a, x_0, y_0, sig_x, sig_y)

	popt, pcov = opt.curve_fit(gaussian, (x.ravel(), y.ravel()), new_data.ravel(), p0 = (a, x_0, y_0, sig_x, sig_y), maxfev = 20000)
	# print popt
	# print a, x_0, y_0, sig_x, sig_y, calcError(data, a, x_0, y_0, sig_x, sig_y)
	a, x_0, y_0, sig_x, sig_y = popt
	error = calcError(data, a, x_0, y_0, sig_x, sig_y)
	print a, x_0, y_0, sig_x, sig_y, calcError(data, a, x_0, y_0, sig_x, sig_y)

	if gm_settings:
		return (a, x_0, y_0, sig_x, sig_y), error
	else:
		return a, x_0, y_0, sig_x, sig_y

def gmValsToRadian(v):
	return v * (40.0 / 180.0 * pi / pow(2.0, 16))

def convertGMVals(vs, dist):
	hgm_rad = gmValsToRadian(vs[0])
	vgm_rad = gmValsToRadian(vs[1])

	return (tan(hgm_rad) * dist, tan(vgm_rad) * dist)

def calculateBeamSize(data):
	a, x_0, y_0, sig_x, sig_y = getGaussianParameters(data)
	k = 1 / exp(2.0)
	fwkm_x = 2 * sqrt(2 * log(1.0 / k)) * abs(sig_x)
	fwkm_y = 2 * sqrt(2 * log(1.0 / k)) * abs(sig_y)
	print 'Beam is between:', fwkm_x * 100, 'cm and', fwkm_y * 100, 'cm' 

def calculateSimpBeamSize(data):
	# Find peak power
	max_x, max_y = max(data, key = data.get)
	max_volt = data[(max_x, max_y)]

	# Filter out points that have a power that is less than max_volt/e^2
	filter_volt = max_volt / exp(2.0)

	beam_data = {k:data[k] for k in data if data[k] >= filter_volt}

	max_dist = max(sqrt(pow(max_x - x, 2) + pow(max_y - y, 2)) for x, y in beam_data)

	print 'Simple Calc puts beam at about:', max_dist * 2 * 100, 'cm'

if __name__ == '__main__':
	parser = argparse.ArgumentParser(description = 'Graph Diode map output files')

	parser.add_argument('-file', '--file_name', metavar = 'FILE_NAME', type = str, nargs = 1, default = [None], help = 'File to get Data from')

	parser.add_argument('-ph','--ph_diode',help = 'Estimate size of beam using positive horizontal diode',action = 'store_true')
	parser.add_argument('-nh','--nh_diode',help = 'Estimate size of beam using negative horizontal diode',action = 'store_true')
	parser.add_argument('-pv','--pv_diode',help = 'Estimate size of beam using positive vertical diode',action = 'store_true')
	parser.add_argument('-nv','--nv_diode',help = 'Estimate size of beam using negative vertical diode',action = 'store_true')

	parser.add_argument('-dist', '--distance_between_gm_and_diode', metavar = 'DISTANCE', type = float, nargs = 1, default = [0.67945], help = 'Distance between the GM and Diodes, in meters')

	args = parser.parse_args()

	params, data = GPH.readInData(args.file_name[0])

	ind = -1
	diode_to_plot = None
	if args.ph_diode:
		ind = 0
		diode_to_plot = 'ph'
	elif args.nh_diode:
		ind = 1
		diode_to_plot = 'nh'
	elif args.pv_diode:
		ind = 2
		diode_to_plot = 'pv'
	elif args.nv_diode:
		ind = 3
		diode_to_plot = 'nv'

	real_unit_data = {convertGMVals(k, args.distance_between_gm_and_diode[0]):data[k][ind] for k in data}
	calculateBeamSize(real_unit_data)

	calculateSimpBeamSize(real_unit_data)
