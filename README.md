# InfantryDL contact

- lowermechine -> uppermachine 0xA5

```c
  typedef struct ProjectileRx {
    uint8_t SOF;              // 0
    float INS_quat_vision[4]; // 1 - 16
    uint8_t vision_mode;      // 17
    uint8_t reserved_1;       // 18 0xFF
    uint8_t is_self_team_red; // bool. 19, 1: red, 0: blue
    float buller_speed;       // 20 - 23  NOTE: buller NOT bullet
    uint8_t hp_data[11];      // 24 - 38
    uint32_t system_time;     // 39 - 42
    float pitch;              // 40 - 43
    float yaw;                // 44 - 47
    uint8_t crc8_check_sum;   // 48
  } __attribute__((packed)) ProjectileRx;

```

- uppermechine -> lowermechine 0x5A

```c
  typedef struct ProjectileTx {
    uint8_t SOF;          // 0 // NOTE: 0x5A
    uint8_t target_found; // 1
    float pitch_angle;    // 2 - 5
    float yaw_angle;      // 6 - 9
    uint8_t checksum;     // 10
  } __attribute__((packed)) ProjectileTx;

```
