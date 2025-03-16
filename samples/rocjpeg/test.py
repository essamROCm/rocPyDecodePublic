import pyRocJpegDecode.decoder as jdec
import pyRocJpegDecode.utils as jutils
import pyRocJpegDecode.types as jpegt
import datetime
import sys
import argparse
import os.path
import ctypes


# JPEG decode & utils instance
jpegdecode = jdec.decoder()
jpegutils = jutils.utils()

jpegutils.test()
