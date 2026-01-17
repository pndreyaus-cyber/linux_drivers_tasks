# Network Ethernet Loopback driver

## What is the goal of the driver?
This driver implements a virtual loopback Ethernet interface that feeds transmitted packets back into the Linux RX networking stack. It is intended for learning, testing, and demonstrating Linux network driver internals such as TX/RX separation, skb ownership, queue backpressure, and NAPI-based packet processing.