
from math import tan, exp, sqrt, log, atan
from random import random
import argparse

def updatePosition(cur_h, cur_v, rotation_speed, rotation_direction, time_change, angular_tolerance):
	# Calculate the radian rotation of this iteration
	horizontal_rotation_rad = rotation_speed * -rotation_direction[0] * time_change * 0.001 * 0.001
	vertical_rotation_rad   = rotation_speed * -rotation_direction[1] * time_change * 0.001 * 0.001

	# Calculate the change in gm values
	gm_horizontal_change = horizontal_rotation_rad * 180.0 / (2.0 * 3.14159) * pow(2, 16) / 40.0
	gm_vertical_change   = vertical_rotation_rad   * 180.0 / (2.0 * 3.14159) * pow(2, 16) / 40.0

	cur_h += gm_horizontal_change
	cur_v += gm_vertical_change

	# Calcluate the beam displacement at the receiver (in cm)
	horizontal_displacement = tan(cur_h * 40.0 / pow(2, 16) * 2.0 * 3.14159 / 180.0) * link_distance * 100.0
	vertical_displacement   = tan(cur_v * 40.0 / pow(2, 16) * 2.0 * 3.14159 / 180.0) * link_distance * 100.0

	# Compute Overall Angular displacement (in mrad)
	overall_displacement = sqrt(pow(horizontal_displacement, 2.0) + pow(vertical_displacement, 2.0))
	angular_displacement = atan(overall_displacement / (link_distance * 100.0)) * 1000.0

	return cur_h, cur_v, (angular_displacement <= angular_tolerance)

def getVolts(cur_h, cur_v, diode_amplitude, beam_diameter, diode_distance, link_distance, minimum_diode_reading, noise_factor):
	hd = tan(cur_h * 40.0 / pow(2, 16) * 2.0 * 3.14159 / 180.0) * link_distance * 100.0
	vd = tan(cur_v * 40.0 / pow(2, 16) * 2.0 * 3.14159 / 180.0) * link_distance * 100.0

	sigma = beam_diameter / 4.0

	ph_volt = diode_amplitude * exp(-(pow(hd - diode_distance, 2.0) + pow(vd, 2.0)) / (2.0 * sigma * sigma)) * (1.0 + random() * 2.0 * noise_factor - noise_factor)
	nh_volt = diode_amplitude * exp(-(pow(hd + diode_distance, 2.0) + pow(vd, 2.0)) / (2.0 * sigma * sigma)) * (1.0 + random() * 2.0 * noise_factor - noise_factor)
	pv_volt = diode_amplitude * exp(-(pow(hd, 2.0) + pow(vd - diode_distance, 2.0)) / (2.0 * sigma * sigma)) * (1.0 + random() * 2.0 * noise_factor - noise_factor)
	nv_volt = diode_amplitude * exp(-(pow(hd, 2.0) + pow(vd + diode_distance, 2.0)) / (2.0 * sigma * sigma)) * (1.0 + random() * 2.0 * noise_factor - noise_factor)

	return map(lambda x: 0.0 if x <= minimum_diode_reading else x, (ph_volt, nh_volt, pv_volt, nv_volt))

def run_simulation(link_distance, beam_diameter,
					diode_distance, diode_amplitude, minimum_diode_reading,
					angular_tolerance, rotation_speed,
					k_proportional, noise_factor):
	# Current GM settings
	cur_h, cur_v = 0, 0

	rotation_direction = (1.0, 0.0)

	iterations_to_test = 10000

	iters_working = 0
	iters_not_working = 0

	for num_iter in range(iterations_to_test):
		# T-0 send message to receiver controller
		# T-1.5 receiver gets volts
		cur_h, cur_v, within_angle_tolerance_1 = updatePosition(cur_h, cur_v, rotation_speed, rotation_direction, 1.5, angular_tolerance)

		ph_volt, nh_volt, pv_volt, nv_volt = getVolts(cur_h, cur_v,
														diode_amplitude, beam_diameter, diode_distance, link_distance,
														minimum_diode_reading, noise_factor)

		# T-5.5 receiver finishes getting volts
		# T-7.0 receiver sends volts back to transmitter
		# T-9.0 transmitter finishes updating the GM
		cur_h, cur_v, within_angle_tolerance_2 = updatePosition(cur_h, cur_v, rotation_speed, rotation_direction, 7.5, angular_tolerance)

		horizontal_response = int(round(k_proportional * (nh_volt - ph_volt)))
		vertical_response   = int(round(k_proportional * (nv_volt - pv_volt)))

		cur_h += horizontal_response
		cur_v += vertical_response

		if within_angle_tolerance_1 and within_angle_tolerance_2:
			iters_working += 1
		else:
			iters_not_working += 1

	print 'Tracking Works:', float(iters_working) / float(iters_working + iters_not_working)

if __name__ == '__main__':
	parser = argparse.ArgumentParser(description = 'Simulates tracking on a long link')

	parser.add_argument('-ld', '--link_distance', metavar = 'DISTANCE', type = float, nargs = 1, default = [100.0], help = 'Distance of the link to simulate (in meters)')
	parser.add_argument('-bd', '--beam_diameter', metavar = 'DIAMETER', type = float, nargs = 1, default = [3.0], help = 'Beam diameter at the receiver (in cm)')
	parser.add_argument('-dd', '--diode_distance', metavar = 'DIODE_DISTANCE', type = float, nargs = 1, default = [1.75], help = 'Distance from the center of the lens to the diode (in cm)')
	parser.add_argument('-da', '--diode_amplitude', metavar = 'AMPLITUDE', type = float, nargs = 1, default = [2.5], help = 'Maximum output from a diode')
	parser.add_argument('-mdr', '--minimum_diode_reading', metavar = 'MINIMUM_READING', type = float, nargs = 1, default = [.05], help = 'Minimum diode reading, any values less are set to 0')
	parser.add_argument('-at', '--angular_tolerance', metavar = 'ANGULAR_TOLERANCE', type = float, nargs = 1, default = [.3], help = 'Angular tolerance of the link (in mrad)')
	parser.add_argument('-rs', '--rotation_speed', metavar = 'ROTATION_SPEED', type = float, nargs = 1, default = [1.0], help = 'Rotation speed of the system (in mrad/sec)')
	parser.add_argument('-kp', '--k_proportional', metavar = 'K_PROPORTIONAL', type = float, nargs = 1, default = [1.0], help = 'Proportional constant used in tracking')
	parser.add_argument('-nf', '--noise_factor', metavar = 'NOISE_FACTOR', type = float, nargs = 1, default = [0.0], help = 'Noise factor of diode readings')

	args = parser.parse_args()

	# Distance from transmitter to receiver in meters
	link_distance = args.link_distance[0]

	# Beam diameter in cm (can be calculated based on link distance)
	beam_diameter = args.beam_diameter[0]

	# Distance between center of the lens and diode in cm
	diode_distance = args.diode_distance[0]

	# Amplitude of diode
	diode_amplitude = args.diode_amplitude[0]

	# Min diode reading (anything less counts as 0)
	minimum_diode_reading = args.minimum_diode_reading[0]

	# Angular tolerance in mrad
	angular_tolerance = args.angular_tolerance[0]

	# Rotation speed in mrad/sec
	rotation_speed = args.rotation_speed[0]

	# Proportional Constant for tracking system
	k_proportional = args.k_proportional[0]

	# Noise factor
	noise_factor = args.noise_factor[0]

	run_simulation(link_distance, beam_diameter, diode_distance,
						diode_amplitude, minimum_diode_reading,
						angular_tolerance, rotation_speed,
						k_proportional, noise_factor)
