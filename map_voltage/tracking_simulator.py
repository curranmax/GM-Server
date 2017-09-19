
from math import floor, ceil
import argparse

class TrackingSimulator:
	def __init__(self, hv, vv, kp, data, map_step, map_range):
		self.init_h_val = hv
		self.init_v_val = vv
		self.kp = kp
		self.data = data
		self.map_step = map_step
		self.map_range = map_range

		self.path = None

		self.run()

	def run(self):
		cur_h = int(self.init_h_val)
		cur_v = int(self.init_v_val)
		self.path = [(cur_h, cur_v)]
		while not self.is_steady() and not self.is_unstable():
			hr, vr, raw_volts = self.calcResponse(cur_h, cur_v)
			cur_h += int(hr)
			cur_v += int(vr)
			self.path.append((cur_h, cur_v))

	def numIters(self):
		if self.is_unstable():
			return float('inf')
		else:
			return len(self.path) - 2

	def lenSteadyLoop(self):
		if self.is_unstable():
			return float('inf')
		else:
			return len(self.path[self.path[:-1].index(self.path[-1]):-1])

	def distanceFromCenter(self, hc, vc):
		if self.is_unstable():
			return float('inf')
		else:
			return max(pow(pow(hc - hv, 2) + pow(vc - vv, 2) , .5) for hv, vv in self.path[self.path[:-1].index(self.path[-1]):-1])

	def steadyLocation(self, hc, vc):
		if self.is_unstable():
			print 'a'
			return (float('inf'), float('inf'))
		else:
			d, h, v = min((pow(pow(hc - hv, 2) + pow(vc - vv, 2) , .5), hv, vv) for hv, vv in self.path[self.path[:-1].index(self.path[-1]):-1])
			h = int(round(float(h) / float(self.map_step)) * self.map_step)
			v = int(round(float(v) / float(self.map_step)) * self.map_step)
			return (h, v)

	# Right now just check if last element completes a cycle. Not a foolproof way to check for cycle, but should work give how path is build up
	def is_steady(self):
		return self.path[-1] in self.path[:-1]

	def is_unstable(self):
		a, b = self.path[-1]
		return a > self.map_range or a < -self.map_range or b > self.map_range or b < -self.map_range

	def calcResponse(self, cur_h, cur_v):
		raw_volts = self.getVolts(cur_h, cur_v) # bilinear interpolate
		volts = map(lambda x: (x if x > 0.0 else 0.0), raw_volts)
		hr = self.kp * (volts[1] - volts[0])
		vr = self.kp * (volts[3] - volts[2])
		return hr, vr, raw_volts

	# Uses bi-linear approximation
	def getVolts(self, cur_h, cur_v):
		floor_h, ceil_h = self.roundGmVal(cur_h)
		floor_v, ceil_v = self.roundGmVal(cur_v)

		fh_fv = self.data[(floor_h, floor_v)]
		fh_cv = self.data[(floor_h, ceil_v)]
		ch_fv = self.data[(ceil_h, floor_v)]
		ch_cv = self.data[(ceil_h, ceil_v)]

		if ceil_v == floor_v:
			# In this case ceil_v == floor_v == cur_v
			fh_v_intercept = fh_fv
			ch_v_intercept = ch_fv
		else:
			fh_v_intercept = tuple(float(a - b) / float(ceil_v - floor_v) * (cur_v - floor_v) + b for a, b in zip(fh_cv, fh_fv))
			ch_v_intercept = tuple(float(a - b) / float(ceil_v - floor_v) * (cur_v - floor_v) + b for a, b in zip(ch_cv, ch_fv))

		if ceil_h == floor_h:
			volt = fh_v_intercept
		else:
			volt = tuple(float(a - b) / float(ceil_h - floor_h) * (cur_h - floor_h) + b for a, b in zip(ch_v_intercept, fh_v_intercept))
		
		return volt

	def roundGmVal(self, val):
		k = float(val) / float(self.map_step)
		return int(floor(k) * self.map_step), int(ceil(k) * self.map_step)

		
