# Network driver
Purpose: Virtual Loopback Ethernet device
Number of interfaces: 1 (vnet0)
TX -> RX model: 
    - TX enqueues packer
    - RX happens via NAPI
RX-queue: per-device
NAPI: 
    - Proccess packets in batches
    - Avoid re-entrancy problems
    - Model real NIC RX behavior
Who schedules NAPI:
    - ndo_start_xmit() schedules NAPI. because TX simulates “hardware produced packet”
Packet processing:
    - Lives in a separate helper function
    - Is called from RX path
    - Filtering by protocol
Concurrency:
    - Both TX and NAPI access RX queue
RX-full behavior:
    - Drop packet
    - Increment rx_dropped
    - Optionally stop TX queue
Testing:
    - ping
    - ip -s link
Github:
    - Architecture diagram
    - Performance test
