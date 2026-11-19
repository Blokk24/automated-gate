<h2>Automated gate</h2>
This project is a software for an automatic barrier gate system. The system has two esp32 microcontrollers, an external server and a client android smartphone.
The project was created as an alternative to expensive automation systems and is an affordable and convenient solution for small and medium-sized organizations.

The project is under development, but at the moment it performs the main functions. it is able to:
- recognize car numbers
- open the passage using a servo
- send data to the application and receive commands from it

File structure:
- module-cam.ino is C++ code for esp32-cam module
- module-servo.ino is C++ code for esp32 module connected to servo
- http_gates_server.py is python code for external server
- recognition.py is additional python code for external server
- best.pt is YOLOv11 neural network weights 
- GateController.rar is archive with Java code for Android app
