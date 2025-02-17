#ifndef ROBO_CV_COMMPORT_H
#define ROBO_CV_COMMPORT_H

#include <Checksum.h>
#include <atomic>
#include <chrono>
#include <serial/serial.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <unistd.h>

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
  // NOTE: the structure is changed
  typedef struct ProjectileRx {
    uint8_t header; // 0xA3 for InfantryDL
    float roll;
    float pitch;
    float yaw;
    float q[4];
    uint8_t color;
    uint8_t auto_aim_mode;
    uint8_t shoot_decision;
    uint8_t EOF_; // 0xAA for InfantryDL // EOF_ not EOF (variable name)
  } __attribute__((packed));

  ProjectileRx rx_struct_{};

  std::atomic<bool> read_stop_flag_{};
  std::atomic<bool> write_stop_flag_{};
  std::atomic<bool> write_clear_flag_{};
  std::atomic<bool> exception_handled_flag_{};

  typedef struct ProjectileTx {
    // WARNING:: the found, patrolling, done_fitting, is_updated are not clear
    uint8_t header; // 0xA3 for InfantryDL
    float pitch;
    float yaw;
    uint8_t found;
    uint8_t shoot_or_not;
    uint8_t done_fitting;
    uint8_t patrolling; // 0xAA for InfantryDL // EOF_ not EOF (variable name)
    uint8_t is_updated;
    uint8_t checksum;
  } __attribute__((packed));

  ProjectileTx tx_struct_{};

  uint8_t tx_buffer_[32]{};
  uint8_t rx_buffer_[64]{};
  serial::Serial port_;

  std::shared_ptr<spdlog::logger> logger_;
  std::vector<serial::PortInfo> serial_port_info_;
  std::string device_desc_;

public:
  enum SERIAL_MODE { TX_SYNC, TX_RX_ASYNC };

  CommPort();

  ~CommPort();

  void RunAsync(SERIAL_MODE mode);

  void Start();

  void Stop();

  void Write(const uint8_t *tx_packet, size_t size, bool safe_write);

  void Read();

  void RxHandler();

  void SerialFailsafeCallback(bool reopen);

  // for RX APIS
  float get_rx_Roll();

  float get_rx_Pitch();

  float get_rx_Yaw();

  void get_rx_quaternion(float *q_);

  uint8_t get_rx_color();

  uint8_t get_rx_autoaim_mode();

  uint8_t get_rx_shoot_decision();

  // for TX APIS
  void set_tx_header(uint8_t header);

  void set_tx_pitch(float pitch);

  void set_tx_yaw(float yaw);

  void set_tx_found(uint8_t found);

  void set_tx_shoot_or_not(uint8_t shoot_or_not);

  void set_tx_done_fitting(uint8_t done_fitting);

  void set_tx_patrolling(uint8_t patrolling);

  void set_tx_is_updated(uint8_t is_updated);

  void set_tx_checksum(uint8_t checksum);

  ProjectileTx get_tx_struct();

  uint8_t tx_struct_len = (uint8_t)sizeof(tx_struct_);

  uint8_t *get_tx_buffer();
};

#endif // ROBO_CV_COMMPORT_H
