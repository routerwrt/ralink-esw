# Ralink-esw
net: dsa: ralink: add ESW switch driver

Add a Distributed Switch Architecture (DSA) driver for the
Ralink/MediaTek embedded Ethernet switch (ESW) found in SoCs such as
RT3050, RT5350, and MT7628.

The ESW provides a 5-port Fast Ethernet switch with an internal CPU
port connected to the Frame Engine (FE). This driver integrates the
switch with the Linux DSA framework and supports both standalone and
VLAN-aware bridge operation.

Features:

DSA integration with per-port netdevices
VLAN filtering and PVID handling (802.1Q tag-based)
FDB and MDB offload
STP state management
CPU port tagging via tag_8021q
Per-port statistics using hardware counters
Hardware LED support with netdev trigger offload
RX queue steering via SDM integration
Ingress policing (drop-based) and egress shaping (TBF) via TC offload

The driver supports both VLAN-unaware (port-based) and VLAN-aware modes.
In VLAN-aware configurations, a CPU PVID is required for correct handling
of untagged traffic.

Limitations:

QoS is limited to hardware CoS/WRR capabilities and is not exposed as
full mqprio offload
Ingress rate limiting is implemented as drop policing only (no
pause/backpressure)
Egress shaping approximates rates based on hardware token/tick granularity

Signed-off-by: Richard van Schagen <richard@routerwrt.org>
