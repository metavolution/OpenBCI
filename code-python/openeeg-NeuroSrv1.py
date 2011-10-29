import serial
import string
import sys
import time
import socket




c = 0
dataIndex = 0




ser = serial.Serial('COM12', 57600, timeout=1)
try:
     # parse modeeg P2 format until user interrupts
     state = 1
     while 1:
         if state == 1:
             # find sync0 (0xa5)
             x = ord(ser.read())
             if x == 0xa5:
                 state = 2
         elif state == 2:
             # find sync1 (0x5a)
             x = ord(ser.read())
             if x == 0x5a:
                 state = 3
         elif state == 3:
             # read data
 #            print 'reading data'
             version = ord(ser.read())
             count = ord(ser.read())
             s = ser.read(12)
             # add high and low bytes for each channel
             data = [ord(s[i])*256+ord(s[i+1]) for i in range(0,len(s),2)]
             switches = ord(ser.read())
             #print 'version',version
             #print 'count',count
             #print 'data',data
             #print 'switches',switches
             #send(data)
             
             print str(data[0]) + " | " + str(data[1])
             
             sendbuf = str("! "+str(dataIndex)+" "+str(2)+" "+str(data[0])+" "+str(data[1]))

             #sendbuf = ''.join(lineIn)
             print sendbuf
            
             #sock.send(sendbuf+"""
             #""")
            
             #conn.send(sendbuf)
             c += 1
            
             dataIndex += 1
            
             if dataIndex > 20000:
                 dataIndex = 0
                
             
             
             
             state = 1
except KeyboardInterrupt, e:
     print 'closing serial port'
     ser.close()
