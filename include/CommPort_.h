#ifndef ROBO_CV_COMMPORT_H
#define ROBO_CV_COMMPORT_H
#include <Checksum.h>
#include <serial/serial.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

constexpr size_t packet_size = 15;

struct TxPacket {
  unsigned char cache[packet_size];

  unsigned char &operator[](int p) { return cache[p]; }

  unsigned char operator[](int p) const { return cache[p]; }

  unsigned char *operator&() { return cache; }

  /*Constructor*/
  TxPacket() { memset(cache, 0, sizeof(0)); }
};

class CommPort {
private:
  // typedef struct ProjectileRx {
  //   uint8_t header;
  //   float q[4];
  //
  //   // the data from the c board
  //   float yaw;
  //   float pitch;
  //
  //   // TODO: WHAT IS THIS?
  //   uint32_t timestamp;
  //
  //   // NOTE: the mode can be set as any value you want
  //   // 0 -> CLASSIC
  //   uint8_t vision_mode;
  //
  //   // the gurgement part (coming from c borad)
  //   uint8_t shoot_remote;
  //   uint8_t armor_color;
  //   // TODO: WTF is the two things above
  //   float bullet_speed;
  //   // NOTE: Now the bullet_speed is added through the shoot_data and
  //   // _bullet_speed_pub_
  //
  //   // the current side color (which team we are)
  //   // 0 -> red
  //   // 1 -> blue
  //   uint8_t current_side_color;
  //   // the enemt_hp
  //   uint16_t enemy_hp[6];
  //
  //   // the bullet_remaining
  //   uint16_t bullet_remaining_info[3];
  //   // 0 -> uint16_t remaining_17mm_num;
  //   // 1 -> uint16_t remaining_42mm_num;
  //   // 2 -> uint16_t remaining_coin_num;
  //
  //   // field events
  //   uint8_t field_events[11];
  //   // 0 -> uint8_t supplier_1_occupation;
  //   // 1 -> uint8_t supplier_2_occupation;
  //   // 2 -> uint8_t supplier_3_occupation;
  //   // 3 -> uint8_t power_rune_activation_point_occupation;
  //   // 4 -> uint8_t small_power_rune_activation_status;
  //   // 5 -> uint8_t big_power_rune_activation_status;
  //   // 6 -> uint8_t r2b2_ground_occupation;
  //   // 7 -> uint8_t r3b3_ground_occupation;
  //   // 8 -> uint8_t r4b4_ground_occupation;
  //   // 9 -> uint8_t base_has_shield;
  //   // 10 -> uint8_t outpost_alive;
  //
  //   // game status
  //   uint8_t game_type;
  //   uint8_t game_progress;
  //   uint16_t state_remain_time;
  //   uint64_t sync_time_stamp;
  //
  //   // game result
  //   uint8_t winner;
  //
  //   // robot buffs
  //   uint8_t robot_buffs[4];
  //   // 0 -> uint8_t robot_replenishing_blood;
  //   // 1 -> uint8_t shooter_cooling_acceleration;
  //   // 2 -> uint8_t robot_defense_bonus;
  //   // 3 -> uint8_t robot_attack_bonus;
  //
  //   // robot position
  //   float robot_position[4];
  //   // 0 -> x = 0.0f;
  //   // 1 -> y = 0.0f;
  //   // 2 -> z = 0.0f;
  //   // 3 -> yaw = 0.0f;
  //
  //   // robot status
  //   uint8_t robot_status_0[2];
  //   // 0 -> uint8_t robot_id;
  //   // 1 -> uint8_t robot_level;
  //   uint16_t robot_status_1[15];
  //   // 0 -> uint16_t remain_hp;
  //   // 1 -> uint16_t max_hp;
  //   // 2 -> uint16_t shooter_17mm_id1_cooling_rate;
  //   // 3 -> uint16_t shooter_17mm_id1_cooling_limit;
  //   // 4 -> uint16_t shooter_17mm_id1_speed_limit;
  //   // 5 -> uint16_t shooter_17mm_id2_cooling_rate;
  //   // 6 -> uint16_t shooter_17mm_id2_cooling_limit;
  //   // 7 -> uint16_t shooter_17mm_id2_speed_limit;
  //   // 8 -> uint16_t shooter_42mm_id1_cooling_rate;
  //   // 9 -> uint16_t shooter_42mm_id1_cooling_limit;
  //   // 10 -> uint16_t shooter_42mm_id1_speed;
  //   // 11 -> uint16_t chassis_power_limit;
  //   // 12 -> uint16_t gimbal_power_output;
  //   // 13 -> uint16_t chassis_power_output;
  //   // 14 -> uint16_t shooter_power_output;
  //
  //   // client command
  //   float command_target_position[3];
  //   // float target_position_x;
  //   // float target_position_y;
  //   // float target_position_z;
  //   uint16_t keyboard_key_pressed;
  //   uint8_t command_target_robot_id;
  //
  //   // client receive
  //   float receive_target_position[2];
  //   // float target_position_x;
  //   // float target_position_y;
  //   uint8_t receive_target_robot_id;
  //
  //   uint8_t checksum;
  // } ProjectileRx;
  //
  std::atomic<bool> read_stop_flag_{};
  std::atomic<bool> write_stop_flag_{};
  std::atomic<bool> write_clear_flag_{};
  std::atomic<bool> exception_handled_flag_{};
  serial::Serial port_;

  std::shared_ptr<spdlog::logger> logger_;
  std::vector<serial::PortInfo> serial_port_info_;
  std::string device_desc_;

public:
  uint8_t tx_buffer_[64]{};  // TODO: bigger buffer?
  uint8_t rx_buffer_[256]{}; // TODO: BIGGer buffer?

  typedef struct __attribute__((packed)) {
    uint8_t header;
    float q[4];

    float yaw;
    float pitch;

    uint8_t lever_mode;
    // the lever_mode show the mode of right lever on the remote controller
    // SW_UP -> 1 SW_MID = 2 SW_DOWN = 3

    float bullet_speed;
    // NOTE: Now the bullet_speed is added through the shoot_data and
    // _bullet_speed_pub_

    uint8_t checksum;
  } ProjectileRx_fast;

  typedef struct ProjectileRx_slow {
    uint8_t header;

    // TODO: WHAT IS THIS?
    uint32_t timestamp;

    // NOTE: the mode can be set as any value you want
    // 0 -> CLASSIC
    uint8_t vision_mode;

    // the gurgement part (coming from c borad)
    uint8_t shoot_remote;
    uint8_t armor_color;
    // TODO: WTF is the two things above
    // the current side color (which team we are)
    //
    // 0 -> red
    // 1 -> blue
    uint8_t current_side_color;
    // the enemt_hp
    uint16_t enemy_hp[6];

    // the bullet_remaining
    uint16_t bullet_remaining_info;
    // for sentry, only 17mm needed
    // 0 -> uint16_t remaining_17mm_num;
    // 1 -> uint16_t remaining_42mm_num;
    // 2 -> uint16_t remaining_coin_num;

    // field events
    uint8_t field_events[11];
    // 0 -> uint8_t supplier_1_occupation;
    // 1 -> uint8_t supplier_2_occupation;
    // 2 -> uint8_t supplier_3_occupation;
    // 3 -> uint8_t power_rune_activation_point_occupation;
    // 4 -> uint8_t small_power_rune_activation_status;
    // 5 -> uint8_t big_power_rune_activation_status;
    // 6 -> uint8_t r2b2_ground_occupation;
    // 7 -> uint8_t r3b3_ground_occupation;
    // 8 -> uint8_t r4b4_ground_occupation;
    // 9 -> uint8_t base_has_shield;
    // 10 -> uint8_t outpost_alive;

    // // game status
    uint8_t game_type;
    uint8_t game_progress;
    uint16_t state_remain_time;
    uint64_t sync_time_stamp;

    // game result
    uint8_t winner;

    // robot buffs
    uint8_t robot_buffs[4];
    // 0 -> uint8_t robot_replenishing_blood;
    // 1 -> uint8_t shooter_cooling_acceleration;
    // 2 -> uint8_t robot_defense_bonus;
    // 3 -> uint8_t robot_attack_bonus;

    // robot position
    float robot_position[4];
    // 0 -> x = 0.0f;
    // 1 -> y = 0.0f;
    // 2 -> z = 0.0f;
    // 3 -> yaw = 0.0f;

    // robot status
    uint8_t robot_status_0[2];
    // 0 -> uint8_t robot_id;
    // 1 -> uint8_t robot_level;
    uint16_t robot_status_1[15];
    // 0 -> uint16_t remain_hp;
    // 1 -> uint16_t max_hp;
    // 2 -> uint16_t shooter_17mm_id1_cooling_rate;
    // 3 -> uint16_t shooter_17mm_id1_cooling_limit;
    // 4 -> uint16_t shooter_17mm_id1_speed_limit;
    // 5 -> uint16_t shooter_17mm_id2_cooling_rate;
    // 6 -> uint16_t shooter_17mm_id2_cooling_limit;
    // 7 -> uint16_t shooter_17mm_id2_speed_limit;
    // 8 -> uint16_t shooter_42mm_id1_cooling_rate;
    // 9 -> uint16_t shooter_42mm_id1_cooling_limit;
    // 10 -> uint16_t shooter_42mm_id1_speed;
    // 11 -> uint16_t chassis_power_limit;
    // 12 -> uint16_t gimbal_power_output;
    // 13 -> uint16_t chassis_power_output;
    // 14 -> uint16_t shooter_power_output;

    // client command
    float command_target_position[3];
    // float target_position_x;
    // float target_position_y;
    // float target_position_z;
    uint16_t keyboard_key_pressed;
    uint8_t command_target_robot_id;

    // client receive
    float receive_target_position[2];
    // float target_position_x;
    // float target_position_y;
    uint8_t receive_target_robot_id;

    uint8_t checksum;
  } ProjectileRx_slow;

  typedef struct ProjectileTx {
    uint8_t header;
    // original rx
    float yaw;
    float pitch;

    // autoaim
    float actual1;
    uint8_t raw1[4];
    float actial2;
    uint8_t raw2[4];
    // navigation
    float linear_x;
    float linear_y;
    float angular_z;

    uint8_t autofire; // the autofire is to tell the robot to fire
    uint8_t placeholder2;
    uint8_t placeholder3;

    uint8_t checksum;
  } ProjectileTx;

  ProjectileTx tx_struct_{};

  ProjectileRx_fast rx_struct_fast_{};
  ProjectileRx_slow rx_struct_slow_{};

  enum SERIAL_MODE { TX_SYNC, TX_RX_ASYNC };

  CommPort();

  ~CommPort();

  void RunAsync(SERIAL_MODE mode);

  void Start();

  void Stop();

  void Write(const uint8_t *tx_packet, size_t size, bool safe_write);

  void Read();

  void RxHandler_fast();

  void RxHandler_slow();

  void tx_struct_dump(uint8_t *tx_buffer);

  void SerialFailsafeCallback(bool reopen);
};

#endif // ROBO_CV_COMMPORT_H
