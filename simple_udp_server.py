
import socket
from datetime import datetime
import math

import argparse

class Vec:
	def __init__(self, h, v):
		self.h = float(h)
		self.v = float(v)

	def move(self, speed, secs):
		return Vec(self.h + speed.h * secs, self.v + speed.v * secs)

def getRSSI(req, center, sigma, amplitude):
	return amplitude * math.exp(-((req.h - center.h) ** 2 / sigma + (req.v - center.v) ** 2 / sigma))

# center - the center of the beam at time 0
# speed - is the speed the center is moving in gm units/sec. The beam doesn't start moving until the first request is received.
def run(center, speed, sigma, amplitude):
	UDP_IP = "127.0.0.1"
	UDP_PORT = 8888

	sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

	sock.bind((UDP_IP, UDP_PORT))

	start_time = None

	while True:
		data, addr = sock.recvfrom(1024)
		print "From: ", addr,
		print "\tRecv: ", data,

		spl = data.split()
		if len(spl) == 1:
			msg = "102.3"
		if len(spl) == 3:
			req_h = int(spl[1])
			req_v = int(spl[2])

			now = datetime.now()
			if start_time == None:
				start_time = now

			msg = '%.2f' % getRSSI(Vec(req_h, req_v), center.move(speed, (now - start_time).total_seconds()), sigma, amplitude)

		print "\tSent: ", msg

		sock.sendto(msg, addr)

if __name__ == '__main__':
	parser = argparse.ArgumentParser(description = 'A UDP server that can be used to test the SFP Auto Alignment Algorithm')

	parser.add_argument('-s', '--speed', metavar = 'SPEED', type = float, nargs = 1, default = [0.0], help = 'Speed (in mrad/sec) to simulate')
	
	parser.add_argument('-sig', '--sigma', metavar = 'SIGMA', type = float, nargs = 1, default = [1447.6482730108394], help = 'Sigma to use in the gaussian curve approximation of the beam')
	parser.add_argument('-a', '--amplitude', metavar = 'AMPLITUDe', type = float, nargs = 1, default = [10000.0], help = 'Amplitude of the gaussain curve approximation of the beam')
	
	args = parser.parse_args()

	center = Vec(10000, 10000)

	d = Vec(0.7071067811865475, 0.7071067811865475)
	s = args.speed[0] # In mrad
	s = s / 1000.0 * 180.0 / math.pi * (2.0 ** 16) / 40.0 # Converts to gm_units
	v = Vec(d.h * s, d.v * s)

	sigma = args.sigma[0]
	amplitude = args.amplitude[0]

	run(center, v, sigma, amplitude)
