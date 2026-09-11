# retriever_protocol

The wire protocol, generated from `firmware/protocol/protocol.yaml`.

**Nothing here is written by hand.** Edit the YAML, then run
`python3 firmware/protocol/generate.py`. CI fails if the two drift apart.

The C header in this package is byte for byte the one compiled into the ESP32
firmware, so a serialisation disagreement between the robot and the computer is
structurally impossible rather than merely unlikely. The C++ header is a thin
C++17 wrapper over it — it adds types, not logic.

Each frame carries a 32 bit hash of the protocol's *meaning*, not of the file's
text: fixing a typo in a comment does not force a reflash, changing a scale
factor does. Every heartbeat reports it, and a mismatch is a refusal to arm.
