#Import libraries
import subprocess
import requests
import os
from io import BytesIO
from PIL import Image
from pyzbar.pyzbar import decode
import time
from flask import Flask, request, jsonify, redirect
import ssl
from flask_cors import CORS
import array
import fcntl
from ctypes import *
import re
from random_username.generate import generate_username
from PyGeneratePassword import PasswordGenerate

#Define the flask app
app = Flask(__name__)
CORS(app)

#Define custom command codes for driver functions
GPIO_SET_PIN = 1074028810
GPIO_SET_VALUE = 1074028811
GPIO_FREE_PIN = 2147770636

#Define the url to send the decoded QR code to
send_decode = ""

#Load the driver
command = "sudo insmod /home/pi/QR/gpio_driver.ko"
driver_open = subprocess.Popen(command, shell=True)

time.sleep(2)

#Create the device file
command = "sudo mknod /dev/QR c 64 0"
dev_open = subprocess.Popen(command, shell=True)

time.sleep(2)

#Open gpio driver
fd = os.open("/dev/QR", os.O_RDWR)

#Clean both gpio pins for use
pin_value = array.array("i", [18])
fcntl.ioctl(fd,GPIO_SET_PIN,pin_value,True)
fcntl.ioctl(fd,GPIO_FREE_PIN,pin_value,True)

pin_value = array.array("i", [24])
fcntl.ioctl(fd,GPIO_SET_PIN,pin_value,True)
fcntl.ioctl(fd,GPIO_FREE_PIN,pin_value,True)

@app.route('/activate', methods=['POST'])
def activate_camera():
	data = request.get_json()

	print("Collected data: " + str(data))
	
	if data is not None and 'test' in data and data['test'] == "test":
		
		#generate random username for mjpeg_streamer
		username = generate_username()
	
		#generate random password for mjpeg_streamer
		password = PasswordGenerate()
		
		try:
				
			#run the mjpeg-streamer
			command = f'mjpg_streamer -i "/usr/local/lib/mjpg-streamer/input_uvc.so -n -f 10 -r 1280x720" -o "output_http.so -w /usr/share/mjpeg-streamer/www -c {username}:{password}"'
			mjpg_streamer_process = subprocess.Popen(command, shell=True)
			time.sleep(5)

			#Setup the GPIO blue pin (18) to on
			pin_value = array.array('i', [18])
			fcntl.ioctl(fd, GPIO_SET_PIN, pin_value, True)
			setting = array.array('i', [1]) 
			fcntl.ioctl(fd, GPIO_SET_VALUE,setting,True)
				
			print("Camera running!\n")
			# Define the URL for the MJPEG stream
			mjpeg_url = f"http://{username}:{password}@10.167.200.151:8080/?action=stream"

			# Create a session to keep the connection open
			session = requests.Session()
			response = session.get(mjpeg_url, stream=True)

			# Initialize a frame buffer
			frame_buffer = bytearray()

			found = False

			# Loop over the MJPEG stream
			for chunk in response.iter_content(chunk_size=1024):
				frame_buffer += chunk
				start = frame_buffer.find(b'\xff\xd8')
				end = frame_buffer.find(b'\xff\xd9')
				if start != -1 and end != -1:
					jpg = frame_buffer[start:end+2]
					frame_buffer = frame_buffer[end+2:]
					image = Image.open(BytesIO(jpg))
			
					decoded_objects = decode(image)

					for obj in decoded_objects:
						if found == False:
							url_brackets = next(iter(obj))
							print("url brackets: " + str(url_brackets))
							url_string = url_brackets.decode("utf-8")
							clean_url = re.sub(r"^b'|'$", "", url_string)
							print("clean_url: " + str(clean_url))
							send_decode = clean_url

							subprocess.run(["killall", "mjpg_streamer"])
							found = True
							
							#Turn on the green LED (24
							pin_value = array.array("i", [24])
							fcntl.ioctl(fd,GPIO_SET_PIN,pin_value,True)
							setting = array.array('i', [1])
							fcntl.ioctl(fd, GPIO_SET_VALUE, setting,True)
								
							time.sleep(2)
							
							#Turn the pins off
							pin_value = array.array("i", [18])
							fcntl.ioctl(fd,GPIO_SET_PIN,pin_value,True)
							fcntl.ioctl(fd,GPIO_FREE_PIN,pin_value,True)

							pin_value = array.array("i", [24])
							fcntl.ioctl(fd,GPIO_SET_PIN,pin_value,True)
							fcntl.ioctl(fd,GPIO_FREE_PIN,pin_value,True)
							
							break

				if found:
					break
			session.close()
			
			command = "killall mjpg_streamer"
			driver_open = subprocess.Popen(command, shell=True)

			return jsonify({'message': send_decode}), 200
		except Exception as e:
			#If an error occurs with turning on the camera, turn it off
			
			#Turn off the blue light
			pin_value = array.array("i", [18])
			fcntl.ioctl(fd, GPIO_SET_PIN, pin_value, True)
			fcntl.ioctl(fd, GPIO_FREE_PIN, pin_value, True)


			command = "killall mjpg_streamer"
			driver_open = subprocess.Popen(command, shell=True)

			return jsonify({'error': 'An error occured with camera setup.'}), 400
	return jsonify({'error': 'invalid request'}), 400

#Define the route for the QR code
if __name__ == '__main__':
	#Load the certificates
	context = ssl.SSLContext(ssl.PROTOCOL_TLS)
	try:
		context.load_cert_chain(certfile='/home/pi/QR/fullchain.pem', keyfile='/home/pi/QR/privKey.pem')
		print("Loaded certs successfully\n")
	except Exception as e:
		print("Error loading certificates\n")
	#Run the app on port 5000
	app.run(host = '0.0.0.0', port = 5000, ssl_context=context)