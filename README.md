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

- DSA integration with per-port netdevices
- VLAN filtering and PVID handling (802.1Q tag-based)
- FDB and MDB offload with automatic VID translation
- STP state management
- CPU port tagging via tag_8021q
- Per-port statistics using hardware counters
- Hardware LED support with netdev trigger offload
- RX queue steering via SDM integration
- Ingress policing (drop-based) and egress shaping (TBF) via TC offload

VLAN handling:

The driver uses private VLAN IDs provided by tag_8021q to represent
forwarding domains in hardware.

- Standalone ports use a per-port private VID
- Bridged ports use a per-bridge private VID for untagged traffic
- User-configured VLANs are programmed for tagged traffic as-is

For VLAN-aware bridges, the user-configured PVID is stored in software
and reported to userspace, but hardware uses a private bridge VID for
untagged traffic classification.

FDB and MDB entries are automatically translated between user-visible
VLAN IDs and hardware forwarding domains.

The driver supports both VLAN-unaware (port-based) and VLAN-aware modes.

Limitations:

- VLAN-aware bridges cannot share overlapping VLAN IDs. Each VID may
  only belong to a single offloaded bridge domain.
- QoS is limited to hardware CoS/WRR capabilities and is not exposed as
  full mqprio offload
- Ingress rate limiting is implemented as drop policing only (no
  pause/backpressure)
- Egress shaping approximates rates based on hardware token/tick
  granularity

Signed-off-by: Richard van Schagen <richard@routerwrt.org>
