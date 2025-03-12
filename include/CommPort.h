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
#include <thread>

constexpr size_t packet_size = 15;

#define PROJECTILE_RX_SOF 0xA5  // lower mechine -> upper mechine
#define PROJECTILE_RX_SIZE 50

#define PROJECTILE_TX_SOF 0x5A  // upper mechine -> lower mechine
#define PROJECTILE_TX_SIZE 11

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
    typedef struct ProjectileRx {
        uint8_t SOF;               // 0
        float INS_quat_vision[4];  // 1 - 16
        uint8_t vision_mode;       // 17
        uint8_t reserved_1;        // 18 0xFF
        uint8_t is_self_team_red;  // bool. 19, 1: red, 0: blue
        float buller_speed;        // 20 - 23  NOTE: buller NOT bullet
        uint8_t hp_data[11];       // 24 - 38
        uint32_t system_time;      // 39 - 42
        float pitch;               // 40 - 43
        float yaw;                 // 44 - 47
        uint8_t camera_id;         // 48
        uint8_t crc8_check_sum;    // 49
    } __attribute__((packed)) ProjectileRx;

    ProjectileRx rx_struct_{};

    std::atomic<bool> read_stop_flag_{};
    std::atomic<bool> write_stop_flag_{};
    std::atomic<bool> write_clear_flag_{};
    std::atomic<bool> exception_handled_flag_{};

    typedef struct ProjectileTx {
        uint8_t SOF;           // 0 // NOTE: 0x5A
        uint8_t target_found;  // 1
        float pitch_angle;     // 2 - 5
        float yaw_angle;       // 6 - 9
        uint8_t checksum;      // 10
    } __attribute__((packed)) ProjectileTx;

    ProjectileTx tx_struct_{};

    uint8_t tx_buffer_[32]{};
    uint8_t rx_buffer_[64]{};
    serial::Serial port_;

    std::shared_ptr<spdlog::logger> logger_;
    std::vector<serial::PortInfo> serial_port_info_;
    std::string device_desc_;

   public:
    size_t tx_struct_len = sizeof(ProjectileTx);

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

    // NOTE: for RX APIS
    uint8_t get_rx_SOF();
    float *get_rx_quat();
    uint8_t get_rx_vision_mode();
    uint8_t get_rx_is_self_team_red();
    float get_rx_buller_speed();
    uint8_t *get_rx_hp_data();
    uint32_t get_rx_system_time();
    float get_rx_pitch();
    float get_rx_yaw();
    uint8_t get_rx_crc8_check_sum();
    uint8_t get_rx_camera_id();

    // NOTE: for TX APIS
    void set_tx_SOF(uint8_t new_SOF);
    void set_tx_target_found(uint8_t new_target_found);
    void set_tx_pitch_angle(float new_pitch_angle);
    void set_tx_yaw_angle(float new_yaw_angle);
    void set_tx_checksum(uint8_t new_checksum);
    CommPort::ProjectileTx get_tx_struct();

    uint8_t *get_tx_buffer();
};

#endif  // ROBO_CV_COMMPORT_H
