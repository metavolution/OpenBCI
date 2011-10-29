import socket
import sys
import time
import serial
import string


HOST = 'localhost'
PORT = 8336

dataIndex = 1




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

conn.send("""200 OK
0       Mr. Default Patient                                                             Ms. Default Recorder                                                            01.05.1001.09.00768     manufacturer                                -1      1       2   electrode 0     electrode 1     metal electrode                                                                 metal electrode                                                                 uV      uV      -512    -512    512     512     0       0       1023    1023    LP:1Hz                                                                         LP:1Hz                                                                         1     1
                                                                  
""")


data = conn.recv(1024)
print data

while True:
  if "watch" in ''.join(data):
     break

conn.send("200 OK")

c = 0
dataIndex = 1



while True:

 


 #print "line:"+str(lineIn[0])
# ch1 = int("0x"+lineIn[0],16)
# ch2 = int("0x"+lineIn[1],16)
 
 ch1 = 100
 ch2 = 100

 sendbuf = str("! "+"0 "+str(dataIndex)+" "+str(2)+" "+str(ch1)+" "+str(ch2)+"\r\n")

 #sendbuf = ''.join(lineIn)
 print sendbuf+"\r"
 #sock.send(sendbuf)
 
 time.sleep(0.003)
 
 conn.send(sendbuf)
 c += 1

 dataIndex += 1
 
 if dataIndex > 20000:
   dataIndex = 0

 #data = conn.recv(1024)
 #print data

conn.close()

time.sleep(1)

sys.exit(0)
