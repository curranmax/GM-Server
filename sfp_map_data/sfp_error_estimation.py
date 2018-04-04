
import sfp_graph as GPH

import random
import argparse
import math

class Vec:
	def __init__(self, h, v):
		self.h = float(h)
		self.v = float(v)

	def move(self, speed, secs):
		if speed.mag() > 0.01:
			return Vec(self.h + speed.h * secs, self.v + speed.v * secs)
		else:
			return Vec(self.h, self.v)

	def nearestGrid(self, base):
		hl, hh = map(lambda x: int(x(base * round(self.h / base))), (math.floor, math.ceil))
		vl, vh = map(lambda x: int(x(base * round(self.v / base))), (math.floor, math.ceil))

		return hl, hh, vl, vh

	def mag(self):
		return math.sqrt(self.h * self.h + self.v * self.v)

def getVal(x, data):
	if x not in data:
		return 0.0
	else:
		return data[x]

def getRSSIData(loc, data, params):
	hl, hh, vl, vh = loc.nearestGrid(params['map_step'])
	p1, p2, p3, p4 = (hl, vl), (hl, vh), (hh, vl), (hh, vh)
	v1, v2, v3, v4 = map(lambda x: getVal(x, data), (p1, p2, p3, p4))

	if vh == vl:
		v12, v34 = v1, v3
	else:
		v12 = v1 + (v2 - v1) / float(vh - vl) * (loc.v - vl)
		v12 = v1 + (v4 - v3) / float(vh - vl) * (loc.v - vl)

	if hh == hl:
		return v12
	else:
		return v12 + (v34 - v12) / float(hh - hl) * (loc.h - hl)

def applyNoise(v, mult_noise, add_noise):
	v = v * (1.0 + random.random() * 2.0 * mult_noise - mult_noise)
	v = v + (random.random() * 2.0 * add_noise - add_noise)

	if v < 0:
		v = 0

	return v

def lookup(this_vals, table):
	min_dif = float('inf')
	best_k = None
	for k in table:
		vs = table[k]

		dif = sum((a - b)**2.0 for a, b in zip(this_vals, vs))
		if dif < min_dif or (abs(dif - min_dif) < 0.0001 and best_k.mag() > k.mag()):
			min_dif = dif
			best_k = k

	if best_k == None:
		raise Exception('No values found in table')

	return best_k


def run(params, rssi_data, iters, mult_noise, add_noise, search_delta):
	# Make Table
	table = dict()
	for x, y in rssi_data:
		table[Vec(x, y)] = (getRSSIData(Vec(x, y), rssi_data, params),
							getRSSIData(Vec(x + search_delta, y), rssi_data, params),
							getRSSIData(Vec(x, y + search_delta), rssi_data, params))

	errs_by_loc = dict()
	for k in table:
		print k.h, k.v
		errs = []
		for i in range(iters):
			# Get vals
			this_vals = (getRSSIData(Vec(k.h, k.v), rssi_data, params),
						 getRSSIData(Vec(k.h + search_delta, k.v), rssi_data, params),
						 getRSSIData(Vec(k.h, k.v + search_delta), rssi_data, params))

			# Add noise
			this_vals = tuple(map(lambda x: applyNoise(x, mult_noise, add_noise), this_vals))

			# Lookup this_vals
			lookup_loc = lookup(this_vals, table)

			# record the error
			err = Vec(k.h - lookup_loc.h, k.v - lookup_loc.v).mag()
			errs.append(err)

		errs_by_loc[k] = errs

	# Graph errs_by_loc
	plot_data = {(k.h, k.v): sum(errs_by_loc[k]) / float(len(errs_by_loc[k])) for k in errs_by_loc}

	GPH.plotRSSI(params, plot_data, plot_func = GPH.plot3D, xy_func = None)


if __name__ == '__main__':
	parser = argparse.ArgumentParser(description = 'Estimates the lookup error for given amounts of noise')

	parser.add_argument('-df', '--data_file_name', metavar = 'FILENAME', type = str, nargs = 1, default = [None], help = 'If a file is given, then returned RSSI mimics the data in the file')

	args = parser.parse_args()

	filename = args.data_file_name[0]
	params, rssi_data = GPH.readInData(filename)

	iters = 1

	mult_noise = 0.001
	add_noise = 0.0

	search_delta = 10

	run(params, rssi_data, iters, mult_noise, add_noise, search_delta)
