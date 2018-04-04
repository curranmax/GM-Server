
import sfp_graph as GPH
import matplotlib.pyplot as plt

def getVal(data, x, y):
	if (x, y) in data:
		return data[(x, y)]
	else:
		return 0.0

if __name__ == '__main__':
	params, data = GPH.readInData('data/1-23/map_60_1.txt')

	search_delta = 10

	x = []
	north_gradient = []
	south_gradient = []
	east_gradient = []
	west_gradient = []
	for i in xrange(-params['map_range'], params['map_range'] + 1):
		center = getVal(data, 0, i)
		north = getVal(data, 0, i + search_delta)
		south = getVal(data, 0, i - search_delta)
		east = getVal(data, search_delta, i)
		west = getVal(data, -search_delta, i)

		x.append(i)
		north_gradient.append((center - north) / search_delta)
		south_gradient.append((center - south) / search_delta)
		east_gradient.append((center - east) / search_delta)
		west_gradient.append((center - west) / search_delta)

	plt.plot(x, north_gradient, label = 'north')
	plt.plot(x, south_gradient, label = 'south')
	plt.plot(x, east_gradient, label = 'east')
	plt.plot(x, west_gradient, label = 'west')
	plt.legend()
	plt.show()
