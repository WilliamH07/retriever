# Battement de cœur d'un microcontrôleur, tel que vu par le calculateur.

std_msgs/Header header

uint8 NODE_HOST         = 0
uint8 NODE_SAFETY       = 1
uint8 NODE_MOTION_FRONT = 2
uint8 NODE_MOTION_REAR  = 3

uint8 STATE_BOOT     = 0
uint8 STATE_SELFTEST = 1
uint8 STATE_READY    = 2
uint8 STATE_ACTIVE   = 3
uint8 STATE_DEGRADED = 4
uint8 STATE_FAULT    = 5

uint8 node_id
uint8 state
uint16 uptime_s
uint8 error_count
uint32 protocol_hash

# Âge du dernier battement reçu. Au-delà de 0,3 s, le nœud est considéré perdu
# (§J.2, sorties automatiques de LIVE_ARMED).
float32 heartbeat_age_s
