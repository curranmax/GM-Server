
import matplotlib.pyplot as plt
import math

def convertData(data, link_distance, sim_distance = None):
	if sim_distance == None:
		return [x / 100.0 * math.pi / 180.0 * 1000.0 for x, y in data], [y for x, y in data]
	else:
		return [math.atan(math.tan(x / 100.0 * math.pi / 180.0) * link_distance / sim_distance) * 1000.0 for x, y in data], [y for x, y in data]

def plotReceivePower(no_tracking_data, tracking_data, link_distance, sim_distance = None):
	no_tracking_x, no_tracking_y = convertData(no_tracking_data, link_distance, sim_distance)
	tracking_x, tracking_y = convertData(tracking_data, link_distance, sim_distance)

	no_tracking_handle, = plt.plot(no_tracking_x, no_tracking_y, 's-')
	tracking_handle, = plt.plot(tracking_x, tracking_y, 's-')

	plt.xlabel('Angular Movement of the Transmitter (mrad)')
	plt.ylabel('Received Power (dBm)')
	# plt.title('Receive Power with and without Tracking')

	plt.legend([no_tracking_handle, tracking_handle], ['No TP', 'TP'], loc = 4)
	plt.show()

def debug_main():
	# Data gathered on 1_22 with 77m link
	no_tracking_data = [(0,-11.50), (.5, -14.6), (1, -18.20), (1.5, -28.54), (2, -38.86), (2.5, -51.84), (3, -56.83), (3.5, -58.36), (4, -61.12), (4.5, -60.17), (5, -67.23), (5.5, -68.52), (6, -66.24), (6.5, -66.55)]
	tracking_data = [(0, -11.57),
						(1, -11.50),
						(2, -11.50),
						(3, -11.32),
						(4, -11.60),
						(5, -11.80),
						(6, -11.92),
						(7, -11.37),
						(8, -11.39),
						(9, -11.44),
						(10, -11.44),
						(11, -11.55),
						(12, -11.48),
						(13, -11.62),
						(14, -11.50),
						(15, -11.33),
						(16, -11.50),
						(17, -11.48),
						(18, -11.38),
						(19, -11.48),
						(20, -11.48),
						(21, -11.41),
						(22, -11.55),
						(23, -11.52),
						(24, -11.33),
						(25, -11.49),
						(26, -11.43),
						(27, -11.56),
						(28, -11.60),
						(29, -11.59),
						(30, -11.52),
						(31, -11.57),
						(32, -11.50),
						(33, -11.53),
						(34, -11.49),
						(35, -11.52),
						(36, -11.50),
						(37, -11.56),
						(38, -11.63),
						(39, -11.52),
						(40, -11.59),
						(41, -11.44),
						(42, -11.44),
						(43, -11.72),
						(44, -11.47),
						(45, -11.38),
						(46, -11.59),
						(47, -11.42),
						(48, -11.59),
						(49, -11.54),
						(50, -11.45)]

	link_distance = 77.0

	plotReceivePower(no_tracking_data, tracking_data, link_distance)

def long_link2_main():
	no_tracking_data = [(0, -7.52),
						(.5, -8.75),
						(1, -12.62),
						(1.5, -14.10),
						(2, -16.20),
						(2.5, -17.83),
						(3, -25.99),
						(3.5, -38.90),
						(4, -42.11),
						(4.5, -45.10),
						(5, -54.23),
						(5.5, -51.10),
						(6, -47.86),
						(6.5, -46.55),
						(7, -45.98),
						(7.5, -49.36),
						(8, -50.46),
						(8.5, -53.39),
						(9, -54.33),
						(9.5, -54.24),
						(10, -58.89),
						(10.5, -62.90),
						(11, -61.50),
						(11.5, -61.50),
						(12, -62.29),
						(12.5, -61.86),
						(13, -65.51),
						(13.5, -66.86),
						(14, -68.54),
						(14.5, -62.68),
						(15, -63.51),
						(15.5, -63.52),
						(16, -64.64),
						(16.5, -63.58),
						(17, -62.90),
						(17.5, -63.17),
						(18, -63.85),
						(18.5, -65.89),
						(19, -67.98),
						(19.5, -65.61),
						(20, -61.89),
						(20.5, -61.80),
						(21, -63.30),
						(21.5, -68.90),
						(22, -66.62),
						(22.5, -68.92),
						(23, -66.88),
						(23.5, -65.45),
						(24, -67.77)]
	tracking_data = [(0, -6.83),
					(1, -7.45),
					(2, -7.40),
					(3, -7.69),
					(4, -8.38),
					(5, -7.27),
					(6, -7.12),
					(7, -7.79),
					(8, -8.01),
					(9, -8.76),
					(10, -9.35),
					(11, -7.56),
					(12, -7.26),
					(13, -7.34),
					(14, -7.27),
					(15, -7.40),
					(16, -7.65),
					(17, -7.76),
					(18, -8.12),
					(19, -8.12),
					(20, -7.93),
					(21, -8.50),
					(22, -8.03),
					(23, -8.05),
					(24, -8.82),
					(25, -7.43),
					(26, -7.44),
					(27, -7.79),
					(28, -8.48),
					(29, -8.56),
					(30, -9.05),
					(31, -7.77),
					(32, -8.44),
					(33, -8.01),
					(34, -9.05),
					(35, -7.30),
					(36, -7.40),
					(37, -7.30),
					(38, -7.45),
					(39, -8.11),
					(40, -7.65),
					(41, -7.84),
					(42, -8.41),
					(43, -7.75),
					(44, -8.09),
					(45, -8.16),
					(46, -9.01),
					(47, -8.46),
					(48, -8.41),
					(49, -8.26),
					(50, -8.50)]

	link_distance = 50.0
	sim_distance = 100.0

	plotReceivePower(no_tracking_data, tracking_data, link_distance, sim_distance)

if __name__ == '__main__':
	long_link2_main()
