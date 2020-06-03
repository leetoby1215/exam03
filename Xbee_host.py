import serial
import numpy as np
import paho.mqtt.client as paho
import time

s = serial.Serial('/dev/ttyUSB0', 9600)
u = serial.Serial( '/dev/ttyACM0', 9600)

time.sleep(1)
s.write("+++".encode())
time.sleep(1)
char = s.read(2)
print("Enter AT mode.")
print(char.decode())
s.write("ATMY 0x87\r\n".encode())
char = s.read(3)
print("Set MY 0x87.")
print(char.decode())

s.write("ATDL 0x88\r\n".encode())
char = s.read(3)
print("Set DL 0x88.")
print(char.decode())
s.write("ATID 0x1\r\n".encode())
char = s.read(3)
print("Set PAN ID 0x1.")
print(char.decode())

s.write("ATWR\r\n".encode())
char = s.read(3)
print("Write config.")
print(char.decode())

s.write("ATCN\r\n".encode())
char = s.read(3)
print("Exit AT mode.")
print(char.decode())

print("start sending RPC")

v = []

for i in range(200):
    s.write("/getAcc/run\r".encode())
    time.sleep(0.1)
    line = str(u.readline())
    v.append(line)

mqttc = paho.Client()
host = "172.16.241.153"
topic= "Mbed"
port = 1883

# Callbacks
def on_connect(self, mosq, obj, rc):
    print("Connected rc: " + str(rc))

def on_message(mosq, obj, msg):
    print("[Received] Topic: " + msg.topic + ", Message: " + str(msg.payload) + "\n");

def on_subscribe(mosq, obj, mid, granted_qos):
    print("Subscribed OK")
def on_unsubscribe(mosq, obj, mid, granted_qos):
    print("Unsubscribed OK")

# Set callbacks
mqttc.on_message = on_message
mqttc.on_connect = on_connect
mqttc.on_subscribe = on_subscribe
mqttc.on_unsubscribe = on_unsubscribe
# Connect and subscribe
print("Connecting to " + host + "/" + topic)
mqttc.connect(host, port=1883, keepalive=60)
mqttc.subscribe(topic, 0)

while(1):
    mesg = "Hello, world!"
    mqttc.publish(topic, mesg)
    time.sleep(1)

s.close()