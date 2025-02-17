#include "CommPort.h"

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
        case 0xA5: {
          RxHandler_slow();
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
          RxHandler_fast();
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
// 废物函数，一用就segmentation fault
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

void CommPort::RxHandler_slow() {
  if (Crc8Verify(rx_buffer_, sizeof(ProjectileRx_slow))) {
    memcpy(&rx_struct_slow_, rx_buffer_, sizeof(ProjectileRx_slow));
  }
}

void CommPort::RxHandler_fast() {
  if (Crc8Verify(rx_buffer_, sizeof(ProjectileRx_fast))) {
    memcpy(&rx_struct_fast_, rx_buffer_, sizeof(ProjectileRx_fast));
  }
}
