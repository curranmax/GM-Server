
import argparse
import sfp_graph as GPH
from math import pi, exp, tan, log, sqrt

import scipy.optimize as opt
import numpy as np


def gmValsToRadian(v):
	return v * (40.0 / 180.0 * pi / pow(2.0, 16))

def convertGMVals(vs, dist):
	hgm_rad = gmValsToRadian(vs[0])
	vgm_rad = gmValsToRadian(vs[1])

	return (tan(hgm_rad) * dist, tan(vgm_rad) * dist)

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

def getGaussianParameters(data):
	# data is key (x_i, y_i) and value of z_i
	x = np.array(sorted(list(set(x for x, y in data))))
	y = np.array(sorted(list(set(y for x, y in data))))
	new_data = np.array([[data[(xv,yv)] for xv in x] for yv in y])
	x, y = np.meshgrid(x, y)

	# Goal is to find values of A, x_0, y_0, sig_x, and sig_y, such that sum i=1 to n (z_i - A * exp(-((x_i - x_0)^2 / (2 * sig_x^2) + (y_i - y_0)^2 / (2 * sig_y^2))))^2 is minimized
	# Find better initial values.
	a = 10000.0
	x_0 = 0.0
	y_0 = 0.0
	sig_x = -1.0
	sig_y = -1.0

	popt, pcov = opt.curve_fit(gaussian, (x.ravel(), y.ravel()), new_data.ravel(), p0 = (a, x_0, y_0, sig_x, sig_y), maxfev = 20000)
	a, x_0, y_0, sig_x, sig_y = popt
	error = calcError(data, a, x_0, y_0, sig_x, sig_y)
	print a, x_0, y_0, sig_x, sig_y, calcError(data, a, x_0, y_0, sig_x, sig_y)

	return a, x_0, y_0, sig_x, sig_y

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
	parser = argparse.ArgumentParser(description = 'Estimates the size of the beam using the recorded RSSI values')

	parser.add_argument('-df', '--data_file_name', metavar = 'FILE_NAME', type = str, nargs = 1, default = [None], help = 'File to get Data from')
	parser.add_argument('-dist', '--distance_between_gm_and_diode', metavar = 'DISTANCE', type = float, nargs = 1, default = [0.67945], help = 'Distance between the GM and Diodes, in meters')

	args = parser.parse_args()

	params, data = GPH.readInData(args.data_file_name[0])

	real_unit_data = {convertGMVals(k, args.distance_between_gm_and_diode[0]):data[k] for k in data}
	
	# calculateBeamSize(real_unit_data)

	calculateSimpBeamSize(real_unit_data)
