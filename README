Chibios C++ notifications framework
===================================

A C++ templated class for creating and passing messages between processes.

Features
-----

* Many clients (Listeners) can listen to each server (Notifier)
* Each client brings its own statically allocated memory
* A client can take as long as it wants to handle its received data without impeeding other clients
* Messages are past via the Chibios Mailbox API

Demo
----

A demo for the STM32F4 Discovery is included. It has a sample message LEDData that specifies the LED states. One listener prints this information over serial, another turns on or off the LEDs