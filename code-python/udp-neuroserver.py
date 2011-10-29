import socket
import sys
import time


dhost = 'localhost'
dport = 8336

shost = '192.168.1.23'
sport = 2323


#fp = open("/dev/rfcomm0","Ur+b");
##fp = open("testinput.dat","r");
#
#fp.write("""SR 256
#""")


sin = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP
sin.bind((shost, sport))
 
#while True:
#    data, addr = sock.recvfrom( 16 ) # buffer size is 1024 bytes
#    print data





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




time.sleep(1)

sock.send("""setheader 0       Mr. Default Patient                                                             Ms. Default Recorder                                                            07.06.1005.35.39768     manufacturer                                -1      1       2   electrode 0     electrode 1     metal electrode                                                                 metal electrode                                                                 uV      uV      -512    -512    512     512     0       0       1023    1023    LP:01Hz                                                                         LP:01Hz                                                                         001     001                                                                     """)


time.sleep(1)



c = 0
dataIndex = 0

while True:
 

 #lineIn = fp.readline().split("\t");
 data, addr = sin.recvfrom( 16 )
 lineIn = data.split("\t")
 #print lineIn

 if "sr" in ''.join(lineIn):
     continue
 if "ov" in ''.join(lineIn):
    time.sleep(0.1)
    continue




  ##sock.send("GET %s HTTP/1.0\r\nHost: %s\r\n\r\n" % (GET, dhost))



 print "line:"+str(lineIn)
 ch1 = int("0x"+lineIn[0],16)
 ch2 = int("0x"+lineIn[1],16)

 sendbuf = str("! "+str(dataIndex)+" "+str(2)+" "+str(ch1)+" "+str(ch2))

 #sendbuf = ''.join(lineIn)
 print sendbuf

 sock.send(sendbuf+"""
""")

 #conn.send(sendbuf)
 c += 1

 dataIndex += 1

 if dataIndex > 20000:
     dataIndex = 0

sock.close()

time.sleep(1)

sys.exit(0)
