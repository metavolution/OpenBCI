import socket
import sys
import time
import serial
import string


HOST = 'localhost'
PORT = 8336

dataIndex = 1

#fp = open("COM13","r");
#fp = open("testinput.dat","r");
#ser = serial.Serial('COM16', 115200, bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, rtscts=1, timeout=1)
ser = serial.Serial('COM26', 115200, timeout=1)


ser.write("""SR 256
""")


s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind((HOST, PORT))
s.listen(1)
conn, addr = s.accept()

print 'Connected by', addr

data = conn.recv(1024)
print data

while True:
  if "display" in ''.join(data):
     break

conn.send("200 OK\n\r")

#time.sleep(1)

data = conn.recv(1024)
print data

while True:
  if "status" in ''.join(data):
     break

conn.send("""
200 OK
2 clients connected
0:EEG
1:Display
""")

data = conn.recv(1024)
print data

while True:
  if "getheader" in ''.join(data):
     break

#0       Mr. Default Patient                                                             Ms. Default Recorder                                                            01.05.1001.09.00768     manufacturer                                -1      1       2   electrode 0     electrode 1     metal electrode                                                                 metal electrode                                                                 uV      uV      -512    -512    512     512     0       0       1023    1023    LP:1Hz                                                                         LP:1Hz                                                                         1     1
conn.send("""200 OK
0        Default Patient                                                                 MetaMind                                                                        01.05.1001.09.00768     metamind                                    -1      1       2   electrode 0     electrode 1     metal electrode                                                                 metal electrode                                                                 uV      uV      -512    -512    512     512     0       0       1023    1023    LP:0Hz                                                                         LP:0Hz                                                                         100   100                                                                 
""")


data = conn.recv(1024)
print data

while True:
  if "watch" in ''.join(data):
     break

conn.send("200 OK")

c = 0
dataIndex = 1


ser.read(6)

while True:

 sline = ser.read(16).replace('\r','')
 print sline

 lineIn = sline.split("\t")
 if "sr" in ''.join(lineIn):
     continue
 if "ov" in ''.join(lineIn):
    time.sleep(0.12)
    cstr = ''.join(lineIn)
    total = len(cstr)
    ovlen = total-6
    print "ovlen:"+str(ovlen)
    ser.read(ovlen+1)
    ovsize = cstr[3:ovlen]
    print "-------OVERRUN "+ovsize+"-------\n"
    continue


 print "ch1:"+str(lineIn[0])+" | ch2:"+str(lineIn[1])
 ch1 = int("0x"+lineIn[0],16)
 ch2 = int("0x"+lineIn[1],16)

 sendbuf = str("! "+"0 "+str(dataIndex)+" "+str(2)+" "+str(ch1)+" "+str(ch2)+"\r\n")
 print sendbuf

 conn.send(sendbuf)
 c += 1

 dataIndex += 1
 
 if dataIndex > 20000:
   dataIndex = 0

 #data = conn.recv(1024)
 #print data
 #print '\n' 

conn.close()

time.sleep(1)

sys.exit(0)
