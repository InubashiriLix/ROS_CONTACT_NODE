#include "CommPort.h"
#include "Checksum.h"

CommPort::CommPort() {
  char *device = "/dev/ttyACM0";
  logger_ = spdlog::get("cv_program_logger");
  port_.setPort(device);
  port_.setBaudrate(115200);
  try {
    port_.open();
  } catch (const serial::IOException &ex) {
    logger_->critical("Failed to open serial port by index {0}", device);
    exit(-2);
  } catch (const serial::PortNotOpenedException &ex) {
    logger_->critical("Failed to open serial port by index {0}", device);
    exit(-2);
  } catch (const serial::SerialException &ex) {
    logger_->critical("Failed to open serial port by index {0}", device);
    exit(-2);
  }

  read_stop_flag_ = false;
  write_stop_flag_ = false;
  write_clear_flag_ = true;
  exception_handled_flag_ = true;

  memset(tx_buffer_, 0, sizeof(tx_buffer_));
  memset(rx_buffer_, 0, sizeof(rx_buffer_));
}

CommPort::~CommPort() { Stop(); }

void CommPort::Read() {
  while (!read_stop_flag_) {
    try {
      if (port_.read(rx_buffer_, sizeof(rx_buffer_)) != 0) {
        switch (rx_buffer_[0]) {
        case 0x3A: {
          RxHandler();
//                        write_clear_flag_ = false;
#ifdef USE_DEBUG_SETTINGS
          auto start = std::chrono::high_resolution_clock::now();
#endif
//                        TxHandler();
//                        Write(tx_buffer_, sizeof(tx_buffer_), true);
#ifdef USE_DEBUG_SETTINGS
          auto elapsed = std::chrono::high_resolution_clock::now() - start;
          printf("Motion result: %d, Latency: %ld\n", tx_struct_.flag,
                 std::chrono::duration_cast<std::chrono::microseconds>(elapsed)
                     .count());
#endif
          //                        write_clear_flag_ = true;
          break;
        }

        case 0xF8: {
          break;
        }

        default:
          break;
        }
      }
    } catch (const serial::SerialException &ex) {
      while (true) {
        if (exception_handled_flag_) {
          break;
        }
      }
    } catch (const serial::IOException &ex) {
      while (true) {
        if (exception_handled_flag_) {
          break;
        }
      }
    } catch (const serial::PortNotOpenedException &ex) {
      while (true) {
        if (exception_handled_flag_) {
          break;
        }
      }
    }
  }
}

void CommPort::Write(const uint8_t *tx_packet, size_t size, bool safe_write) {
  while (!write_clear_flag_)
    ;
  if (safe_write) {
    try {
      port_.write(tx_packet, size);
    } catch (const serial::SerialException &ex) {
      SerialFailsafeCallback(true);
    } catch (const serial::IOException &ex) {
      SerialFailsafeCallback(true);
    }
  } else {
    port_.write(tx_packet, size);
  }
}

// sentry only
void CommPort::RunAsync(SERIAL_MODE mode) {
  if (mode == TX_SYNC) {
    logger_->info("Serial mode: TX_SYNC");
  } else if (mode == TX_RX_ASYNC) {
    logger_->info("Serial mode: TX_SYNC & RX_ASYNC");
    std::thread serial_read(&CommPort::Read, this);
    serial_read.detach();
    logger_->info("Async serial read thread started");
  }
}

void CommPort::Start() {
  std::thread t(&CommPort::Read, this);
  t.detach();
}

void CommPort::Stop() {
  write_stop_flag_ = true;
  read_stop_flag_ = true;
#define MS 1000000
  usleep(MS / 2);
#undef MS
  port_.close();
}

void CommPort::SerialFailsafeCallback(bool reopen) {
  exception_handled_flag_ = false;
  std::string target_device = port_.getPort();
  logger_->error("IO failed at serial device: {0}, retrying", target_device);
  bool new_device_found = false;

  // Serial exception handling
  while (true) {
#define MS 1000000
    usleep(MS);
#undef MS
    serial_port_info_ = serial::list_ports();
    for (auto &i : serial_port_info_) {
      if (i.description.find("STMicroelectronics") != std::string::npos) {
        target_device = i.port;
        new_device_found = true;
        break;
      }
    }
    if (new_device_found) {
      break;
    }
  }
  if (reopen) {
    port_.close();
  }
  port_.setPort(target_device);
  port_.open();
  logger_->info("Serial open failsafe succeeded at port: {0}", target_device);
  exception_handled_flag_ = true;
}

// rx: receive
// tx: transport
void CommPort::RxHandler() {
  // if (Crc8Verify(rx_buffer_, sizeof(ProjectileRx))) {
  //   memcpy(&rx_struct_, rx_buffer_, sizeof(ProjectileRx));
  // }
  // the infantryDL is not using CRC8 for now
  memcpy(&rx_struct_, rx_buffer_, sizeof(ProjectileRx));
}

float CommPort::get_rx_Pitch() { return this->rx_struct_.pitch; }

float CommPort::get_rx_Yaw() { return this->rx_struct_.yaw; }

void CommPort::get_rx_quaternion(float *q_) {
  for (int i = 0; i < 4; i++) {
    q_[i] = this->rx_struct_.q[i];
  }
}

uint8_t CommPort::get_rx_color() { return this->rx_struct_.color; }

uint8_t CommPort::get_rx_autoaim_mode() {
  return this->rx_struct_.auto_aim_mode;
}

uint8_t CommPort::get_rx_shoot_decision() {
  return this->rx_struct_.shoot_decision;
}

void CommPort::set_tx_header(uint8_t header) {
  this->tx_struct_.header = header;
}

void CommPort::set_tx_pitch(float pitch) { this->tx_struct_.pitch = pitch; }

void CommPort::set_tx_yaw(float yaw) { this->tx_struct_.yaw = yaw; }

void CommPort::set_tx_found(uint8_t found) { this->tx_struct_.found = found; }

void CommPort::set_tx_shoot_or_not(uint8_t shoot_or_not) {
  this->tx_struct_.shoot_or_not = shoot_or_not;
}

void CommPort::set_tx_done_fitting(uint8_t done_fitting) {
  this->tx_struct_.done_fitting = done_fitting;
}

void CommPort::set_tx_patrolling(uint8_t patrolling) {
  this->tx_struct_.patrolling = patrolling;
}

void CommPort::set_tx_is_updated(uint8_t is_updated) {
  this->tx_struct_.is_updated = is_updated;
}

void CommPort::set_tx_checksum(uint8_t checksum) {
  this->tx_struct_.checksum = checksum;
}

CommPort::ProjectileTx CommPort::get_tx_struct() { return this->tx_struct_; }

uint8_t *CommPort::get_tx_buffer() {
  static uint8_t tx_buffer[32];
  memcpy(tx_buffer, &tx_struct_, sizeof(tx_struct_));
  Crc8Append(tx_buffer, sizeof(tx_buffer));
  return tx_buffer;
}
