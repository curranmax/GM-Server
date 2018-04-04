
import sfp_graph as GPH

import socket
from datetime import datetime
import math

import argparse
import math

import random
import time

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

def getRSSIGaus(req, center, sigma, amplitude):
	return amplitude * math.exp(-((req.h - center.h) ** 2 / sigma + (req.v - center.v) ** 2 / sigma))

def getVal(x, data):
	if x not in data:
		return 0.0
	else:
		return data[x]

def getRSSIData(req, center, data, params):
	err = Vec(req.h - center.h, req.v - center.v)
	hl, hh, vl, vh = err.nearestGrid(params['map_step'])
	p1, p2, p3, p4 = (hl, vl), (hl, vh), (hh, vl), (hh, vh)
	v1, v2, v3, v4 = map(lambda x: getVal(x, data), (p1, p2, p3, p4))

	if vh == vl:
		v12, v34 = v1, v3
	else:
		v12 = v1 + (v2 - v1) / float(vh - vl) * (err.v - vl)
		v12 = v1 + (v4 - v3) / float(vh - vl) * (err.v - vl)

	if hh == hl:
		return v12
	else:
		return v12 + (v34 - v12) / float(hh - hl) * (err.h - hl)

# center - the center of the beam at time 0
# speed - is the speed the center is moving in gm units/sec. The beam doesn't start moving until the first request is received.
def run(center, speed, sigma = None, amplitude = None, filename = None, noise = 0.0, wait_time = 6.0):
	if (sigma == None or amplitude == None) and filename == None:
		raise Exception('Invalid input: must supply either sigma and amplitude, or a filename')

	if filename != None:
		params, rssi_data = GPH.readInData(filename)

	UDP_IP = "127.0.0.1"
	UDP_PORT = 8888

	sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

	sock.bind((UDP_IP, UDP_PORT))

	start_time = None
	now = None

	while True:
		data, addr = sock.recvfrom(1024)
		print "From: ", addr,
		print "\tRecv: ", data,

		spl = data.split()
		if len(spl) == 1:
			msg = "102.3"
		if len(spl) == 3 or len(spl) == 4:
			req_h = int(spl[1])
			req_v = int(spl[2])

			if not(now != None and len(spl) == 4 and spl[3] == 'no_update'):
				now = datetime.now()
				if start_time == None:
					start_time = now

			t = (now - start_time).total_seconds()
			t -= 2
			if t < 0:
				t = 0.0

			if filename == None:
				rssi = getRSSIGaus(Vec(req_h, req_v), center.move(speed, t), sigma, amplitude)
			else:
				rssi = getRSSIData(Vec(req_h, req_v), center.move(speed, t), rssi_data, params)
			
			if noise > 0.001:
				rssi = (1.0 + (random.random() * 2.0 * noise - noise)) * rssi

			msg = '%.2f' % rssi

		time.sleep(wait_time / 1000.0)

		print "\tSent: ", msg

		sock.sendto(msg, addr)

if __name__ == '__main__':
	parser = argparse.ArgumentParser(description = 'A UDP server that can be used to test the SFP Auto Alignment Algorithm')

	parser.add_argument('-s', '--speed', metavar = 'SPEED', type = float, nargs = 1, default = [0.0], help = 'Speed (in mrad/sec) to simulate')
	
	parser.add_argument('-sig', '--sigma', metavar = 'SIGMA', type = float, nargs = 1, default = [1447.6482730108394], help = 'Sigma to use in the gaussian curve approximation of the beam')
	parser.add_argument('-a', '--amplitude', metavar = 'AMPLITUDE', type = float, nargs = 1, default = [10000.0], help = 'Amplitude of the gaussain curve approximation of the beam')
	
	parser.add_argument('-df', '--data_file_name', metavar = 'FILENAME', type = str, nargs = 1, default = [None], help = 'If a file is given, then returned RSSI mimics the data in the file')

	parser.add_argument('-noise', '--noise_factor', metavar = 'NOISE', type = float, nargs = 1, default = [0.0], help = 'Noise Factor to add to returned RSSI')

	parser.add_argument('-wt', '--wait_time', metavar = 'TIME_IN_MILLISECONDS', type = float, nargs = 1, default = [6.0], help = 'Time (in milliseconds) to wait before sending a response')

	args = parser.parse_args()

	center = Vec(10000, 10000)

	d = Vec(-1.0, 0.0)
	s = args.speed[0] # In mrad
	s = s / 1000.0 * 180.0 / math.pi * (2.0 ** 16) / 40.0 # Converts to gm_units
	v = Vec(d.h * s, d.v * s)

	sigma = args.sigma[0]
	amplitude = args.amplitude[0]

	run(center, v, sigma = sigma, amplitude = amplitude, filename = args.data_file_name[0], noise = args.noise_factor[0], wait_time = args.wait_time[0])
