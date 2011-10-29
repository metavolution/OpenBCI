import socket
import sys
import time
import serial
import string


dhost = 'localhost'
dport = 8336



ser = serial.Serial('COM26', 115200, timeout=1)


ser.write("""SR 256
""")



try:
 sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
 
except socket.error, msg:
 sys.stderr.write("[ERROR] %s\n" % msg[1])
 sys.exit(1)

try:
 sock.connect((dhost, dport))
except socket.error, msg:
 sys.stderr.write("[ERROR] %s\n" % msg[1])
 sys.exit(2)

sock.send("""eeg
""")


#sock.send("""setheader 0       Mr. Default Patient                                                             Ms. Default Recorder                                                            07.06.1005.35.39768     manufacturer                                 1      1       2   electrode 0     electrode 1     metal electrode                                                                 metal electrode                                                                 uV      uV      -512    -512    512     512     0       0       1023    1023    LP:000Hz                                                                         LP:000Hz                                                                       100     100                                                                     """)

sock.send("setheader 0")

c = 0
dataIndex = 0

ser.read(7)


while True:

 #time.sleep(0.01)
 
 sline = ser.read(16).replace('\r','')
 print sline

 lineIn = sline.split("\t")
 if "sr" in ''.join(lineIn):
     continue
 if "ov" in ''.join(lineIn):
    time.sleep(0.2)
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


 sendbuf = str("! "+str(dataIndex)+" "+str(2)+" "+str(ch1)+" "+str(ch2)+"\n")
 print sendbuf

 sock.send(sendbuf+"""
""")


 c += 1
 dataIndex += 1

 if dataIndex > 20000:
     dataIndex = 0

 print '\n'

sock.close()

time.sleep(1)

sys.exit(0)
