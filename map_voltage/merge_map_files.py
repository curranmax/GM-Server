
import graph
from beam_size_estimator import getGaussianParameters, simpGaussian

import argparse
from collections import defaultdict

def computeAverage(list_of_volts):
	if len(list_of_volts) == 0:
		raise Exception('Can\'t get average of empty list')
	
	ph = round(sum(a for a, b, c, d in list_of_volts) / len(list_of_volts), 6)
	nh = round(sum(b for a, b, c, d in list_of_volts) / len(list_of_volts), 6)
	pv = round(sum(c for a, b, c, d in list_of_volts) / len(list_of_volts), 6)
	nv = round(sum(d for a, b, c, d in list_of_volts) / len(list_of_volts), 6)
	return (ph, nh, pv, nv)

def mergeData(in_files):
	total_params = {'record_type': 'all', 'kp': 1, 'map_step': None, 'map_range': None}
	total_data = defaultdict(list)

	for file_name in in_files:
		params, data = graph.readInData(file_name)
		print params
		if params['record_type'] != total_params['record_type']:
			raise Exception('Invalid "record_type"')

		if total_params['map_step'] == None:
			total_params['map_step'] = params['map_step']
		if total_params['map_step'] != None and params['record_type'] != total_params['record_type']:
			raise Exception('Invalid "map_step"')

		if total_params['map_range'] == None or params['map_range'] < total_params['map_range']:
			total_params['map_range'] = int(params['map_range'])

		for h, v in data:
			total_data[(h, v)].append(data[(h, v)])

	mergeData = {(h, v): computeAverage(total_data[(h, v)])
						for h in xrange(-total_params['map_range'], total_params['map_range'] + 1, total_params['map_step'])
							for v in xrange(-total_params['map_range'], total_params['map_range'] + 1, total_params['map_step'])}

	return total_params, mergeData

def non_neg(v):
	if v <= 0:
		return 0.0
	return v

def low_val_cutoff(v, low_val):
	if v <= low_val:
		return -.1
	return v

def fitGaussian(params, data):
	ph_data = {(float(x), float(y)):  non_neg(data[(x,y)][0]) for x, y in data}
	nh_data = {(float(x), float(y)):  non_neg(data[(x,y)][1]) for x, y in data}
	pv_data = {(float(x), float(y)):  non_neg(data[(x,y)][2]) for x, y in data}
	nv_data = {(float(x), float(y)):  non_neg(data[(x,y)][3]) for x, y in data}

	ph_gaus_params, ph_error = getGaussianParameters(ph_data, gm_settings = True, init_a = 3.0, init_x_0 = 40.0, init_y_0 = 0.0, init_sig_x = 10.0, init_sig_y = 10.0)
	nh_gaus_params, nh_error = getGaussianParameters(nh_data, gm_settings = True, init_a = 3.0, init_x_0 = -30.0, init_y_0 = 0.0, init_sig_x = 10.0, init_sig_y = 10.0)
	pv_gaus_params, pv_error = getGaussianParameters(pv_data, gm_settings = True, init_a = 3.0, init_x_0 = -10.0, init_y_0 = 30.0, init_sig_x = 10.0, init_sig_y = 10.0)
	nv_gaus_params, nv_error = getGaussianParameters(nv_data, gm_settings = True, init_a = 3.0, init_x_0 = -10.0, init_y_0 = -30.0, init_sig_x = 10.0, init_sig_y = 10.0)
	
	diode_error = False
	if ph_error >= 300.0:
		print 'High gaus error for ph diode'
		diode_error = True
	if nh_error >= 300.0:
		print 'High gaus error for nh diode'
		diode_error = True
	if pv_error >= 300.0:
		print 'High gaus error for pv diode'
		diode_error = True
	if nv_error >= 300.0:
		print 'High gaus error for nv diode'
		diode_error = True
	if diode_error:
		raise Exception('High diode error')

	low_val = .005
	return {k:(low_val_cutoff(simpGaussian(k, *ph_gaus_params), low_val),
				low_val_cutoff(simpGaussian(k, *nh_gaus_params), low_val),
				low_val_cutoff(simpGaussian(k, *pv_gaus_params), low_val),
				low_val_cutoff(simpGaussian(k, *nv_gaus_params), low_val))
						for k in data}

def writeData(out_file, params, data):
	if out_file != '':
		f = open(out_file, 'w')
		f.write('\nPARAMS ' + ' '.join(map(lambda k: str(k) + ':' + str(params[k]), params)) + '\nhd vd ph nh pv nv hr vr\n')
		for h in xrange(-params['map_range'], params['map_range'] + 1, params['map_step']):
			for v in xrange(-params['map_range'], params['map_range'] + 1, params['map_step']):
				f.write(str(h) + ' ' + str(v) + ' ' + ' '.join(map(str, data[(h, v)])) + ' 0 0\n')

if __name__ == '__main__':
	parser = argparse.ArgumentParser(description = 'Creates a new map file, by either averaging a bunch or fitting a gaussian function')

	parser.add_argument('-in_files', '--input_file_names', metavar = 'FILE_NAME', type = str, nargs = '+', default = [], help = 'Files to merge')
	parser.add_argument('-out_file', '--output_file_name', metavar = 'FILE_NAME', type = str, nargs = 1, default = [''], help = 'Files to write data to')
	
	parser.add_argument('-gaus','--fit_gaussian_function', help = 'Fits a gaussian curve to the data, and outputs the values', action = 'store_true')

	args = parser.parse_args()

	in_files = args.input_file_names
	out_file = args.output_file_name[0]
	fit_gaussian = args.fit_gaussian_function

	params, data = mergeData(in_files)

	if fit_gaussian:
		data = fitGaussian(params, data)

	writeData(out_file, params, data)
