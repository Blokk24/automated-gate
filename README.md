The project is under development, but at the moment it performs the main functions: 
it is able to recognize car numbers, 
open the passage using a servo, 
send data to the application and receive commands from it.

- file structure:
module-cam.ino is C++ code for esp32-cam module
module-servo.ino is C++ code for esp32 module connected to servo
http_gates_server.py is python code for external server
recognition.py is additional python code for external server
best.pt is YOLOv11 neural network weights 
GateController.rar is archive with Java code for Android app
