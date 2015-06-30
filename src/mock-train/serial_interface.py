# Utilities for interacting with the train program at a higer level.
import socket, errno, select, telnetlib

# This doesn't work because something about \xFF codes or some shit
class MyTelnet():
	def __init__(self, host, port=23, timeout=socket._GLOBAL_DEFAULT_TIMEOUT):
		self.sock = socket.create_connection((host, port), timeout)
		self.wbuf = ''
		self.rbuf = ''
	def fileno(self): return self.sock.fileno()
	def read(self): buf = self.rbuf; self.rbuf = ''; return buf
	def write(self, buf): self.wbuf += buf
	def service(self):
		if len(self.wbuf) > 0 and select.select([], [self], [], 0) == ([], [self], []):
			sent = self.sock.send(self.wbuf)
			self.wbuf = self.wbuf[sent:]
		elif select.select([self], [], [], 0) == ([self], [], []):
			self.rbuf += self.sock.recv(50)

class MyTelnetWrapper():
	def __init__(self, host, port=23, timeout=socket._GLOBAL_DEFAULT_TIMEOUT):
		self.tn = telnetlib.Telnet(host, port, timeout)
	def read(self): return self.tn.read_eager()
	def write(self, buf): return self.tn.write(buf) # TODO: Non-blocking
	def service(self): pass

class FakeTelnet():
	def __init__(self): self.fakeB = True
	def read(self):
		if self.fakeB:
			self.fakeB = False
			return '\x00'
		else: return ''
	def write(self, buf): pass
	def service(self): pass

class Conn():
	def __init__(self, tn=None):
		self.tn = tn if tn is not None else FakeTelnet()
		self.s = ''
		self.sensors = ['\x00'*10]
		while True:
			self.s += self.tn.read()
			if len(self.s) > 0:
				assert(self.s[0] == '\x00')
				self.s = self.s[1:]
				break

	def next_cmd(self):
		self.tn.service()
		self.s += self.tn.read()
		self.s, typ, a1, a2 = self._parse_cmd(self.s)
		return (typ, a1, a2)

	def set_sensor_tripped(self, n, tripped):
		word_num = num / 8
		offset = 7 - num % 8
		bit = 0x1 << offset

		assert word_num < len(self.sensors)
		mask = self.sensors[word_num]
		mask = (mask | bit) if tripped else (mask & ~bit)
		self.sensors[word_num] = mask

	def _parse_cmd(self, s):
		if len(s) < 1: return (s, None, None, None)
		print "Got command %s"% s.encode('hex')
		f = ord(s[0])
		if 0 <= f <= 14:
			if len(s) < 2: return (s, None, None, None)
			print "Setting speed of %d to %d" % (ord(s[1]), f)
			return (s[2:], 'set_speed', f, ord(s[1]))
		elif f == 15:
			if len(s) < 2: return (s, None, None, None)
			print "Toggling reverse of %d" % ord(s[1])
			return (s[2:], 'toggle_reverse', ord(s[1]), None)
		elif f == 0x20:
			print "Disabling solenoid"
			return (s[1:], 'disable_solenoid', None, None)
		elif 0x21 <= f <= 0x22:
			if len(s) < 2: return (s, None, None, None)
			print "Switch command not supported %d %d" % (f, ord(s[1]))
			return (s[2:], 'switch', f - 0x21, ord(s[1]))
		elif f == 0x85:
			print "Got sensor poll"
			self.tn.write(self.sensors)
			return self._parse_cmd(s[1:])
		raise Exception("Unknown command %s" % s.encode('hex'))

def connect():
	tn = FakeTelnet()
	try:
		tn = MyTelnetWrapper("localhost", 1230)
	except socket.error, v:
		if v[0] == errno.ECONNREFUSED:
			# TODO: We probably don't want to catch this normally, just when
			# working on the train simulator.
			print "Warning: Telnet connection refused!"
	return Conn(tn)

if __name__ == '__main__':
	# TODO: More tests!
	cp = Conn(FakeTelnet())
	print cp._parse_cmd('\x00\x35')
	print cp._parse_cmd('\x21\x22')
