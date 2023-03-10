"""
Define the C-variables and functions from the C-files that are needed in Python
"""
from ctypes import c_ubyte, c_ushort, c_uint, c_ulong, CDLL
import sys

lib_path = './rcr32_%s.so' % (sys.platform)
try:
    rcr32_lib = CDLL(lib_path)
except:
    print('OS %s not recognized' % (sys.platform))

rcr32_ks = rcr32_lib.ks
rcr32_ks.restype = None


def keystream(key, iv, k, v):
	c_key = (c_ubyte * int(k/8))(*key)
	c_iv = (c_ubyte * int(v/8))(*iv)
	c_ks = (c_ubyte * 4)()
	rcr32_ks(c_key, c_iv, c_uint(k), c_uint(v), c_ks)
	return c_ks[:]
	
	
