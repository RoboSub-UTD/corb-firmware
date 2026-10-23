# Per-thruster command. Fixed-size so it needs no dynamic allocation on the VCU.
uint32 seq              # strictly increasing (wraparound-safe); stale/duplicate seq is rejected
uint8 count             # number of valid entries in values; must equal the VCU's thruster count
float32[8] values       # normalized [-1, 1], 0 = neutral
