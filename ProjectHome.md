BioloidCControl is an alternative firmware for the Robotis Bioloid Premium Kit humanoid Type A/B/C robots. Its aim is to replicate the functionality of the original Robotis firmware (which is not open source), giving the user more options in terms of behaviour control. It is based on:
  1. Robotis Embedded C toolkit v1.01 (for serial and Dynamixel control)
  1. Robotis sample task and motion files for humanoid Type A/B/C robots (for motion pages and walking code)
  1. Pololu robotics library (for ADC and Buzzer functions)

In contrast to the Robotis firmware it is:
  * open source
  * much less memory hungry than Robotis (70KB vs. 170KB for firmware + motion file)
  * supports sending commands to the robot via Zig2Serial from the PC (so you can implement behaviour based code on the PC if you like)

What's implemented to date:
  * support for humanoid Type A/B/C robots (only Type A has been tested - included motion.h file is for Type A robot)
  * support for executing motions based on a RoboPlus Motion file
  * support for walking and shifting between walk commands
  * support for all Dynamixel commands incl sync\_write
  * support for gyro, DMS sensor and the other ADC channels
  * support for LED's, buttons, buzzer (including melodies)
  * support for serial communication via cable or ZigBee & Zig2Serial
  * command interpreter
  * basic control loop and finite state machine for executing motions without waiting for completion
  * static balancing using gyro and accelerometer (based on Extended Kalman Filter)

The main control loop implements a finite state machine which can receive commands via the serial port. Commands can be issued using RoboPlus Terminal either using a Zig2Serial or the serial cable. A complete command reference is provided in the User's Guide.