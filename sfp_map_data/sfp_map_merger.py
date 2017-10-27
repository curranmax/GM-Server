
import sfp_graph as GPH

import argparse


if __name__ == '__main__':
	parser = argparse.ArgumentParser(description = 'Merges multiple SFP Map output files into one file')

	parser.add_argument('-fs', '--files', metavar = 'FILE_NAME', type = str, nargs = '+', default = [], help = 'File to plot')
	parser.add_argument('-out', '--out_file', metavar = 'FILE_NAME', type = str, nargs = '+', default = [], help = 'File to plot')


	args = parser.parse_args()

	files = args.files

	# List of the output of GPH.readInData for each file in files.
	all_data = []

	for filename in files:
		all_data.append(GPH.readInData(filename))

	# TODO allow for non-standard params
	if any(p1['map_range'] != p2['map_range'] or p1['map_step'] != p2['map_step'] for p1, _ in all_data for p2, _ in all_data):
		raise Exception('Not all files have the same map_range and map_step')

	total_params = all_data[0][0]
	total_params['files'] = ','.join(files)
	total_data = {(h, v):[] for h in xrange(-total_params['map_range'], total_params['map_range'] + 1, total_params['map_step'])
							for v in xrange(-total_params['map_range'], total_params['map_range'] + 1, total_params['map_step'])}

	# Merge all data
	for _, data in all_data:
		for x, y in data:
			if (x, y) not in total_data:
				raise Exception('Didn\'t find points in total_data')

			total_data[(x, y)].append(data[(x, y)])

	# Write data to a single file
	f = open(args.out_file[0], 'w')

	f.write('PARAMS ' + ' '.join([str(k) + '=' + str(v) for k, v in total_params.iteritems()]) + '\n')
	f.write('H_Delta V_Delta ' + ' '.join(['RSSI'] * len(all_data)) + '\n')

	for x, y in sorted(total_data):
		f.write(str(x) + ' ' + str(y) + ' ' + ' '.join(map(str, total_data[(x, y)])) + '\n')
