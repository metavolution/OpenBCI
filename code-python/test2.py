from win32 import *


ser = SerialPort("COM16", 1000, 9600) 


while True:
  c = ser.read()
  print c