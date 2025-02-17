#include <CommPort.h>
#include <iomanip>
#include <sstream>
#include <stdio.h>

#include <geometry_msgs/msg/quaternion.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/rclcpp.hpp>

#include "std_msgs/msg/int32.hpp"

// new include
#include <geometry_msgs/msg/point.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <nav_msgs/msg/path.hpp>
#include <sensor_msgs/msg/joy.hpp>
#include <sentry_msgs/msg/auto_aim.hpp>
#include <sentry_msgs/msg/bullet_remaining.hpp>
#include <sentry_msgs/msg/client_command.hpp>
#include <sentry_msgs/msg/client_receive.hpp>
#include <sentry_msgs/msg/dr16_receiver.hpp>
#include <sentry_msgs/msg/field_events.hpp>
#include <sentry_msgs/msg/game_result.hpp>
#include <sentry_msgs/msg/game_status.hpp>
#include <sentry_msgs/msg/robot_buff.hpp>
#include <sentry_msgs/msg/robot_hp.hpp>
#include <sentry_msgs/msg/robot_position.hpp>
#include <sentry_msgs/msg/robot_status.hpp>
#include <sentry_msgs/msg/shoot_data.hpp>
#include <tf2_msgs/msg/tf_message.hpp>

using namespace std::chrono_literals;

class ContactNode : public rclcpp::Node {
public:
  ContactNode() : Node("contact") {
    comm.Start();

    // publishers
    _mode_pub_ = this->create_publisher<std_msgs::msg::Int32>("/auto/mode", 10);
    _timestamp_pub_ =
        this->create_publisher<std_msgs::msg::Int32>("/timestamp", 10);
    quaternion_pub_ = this->create_publisher<geometry_msgs::msg::Quaternion>(
        "/quaternion", 10);

    _autoaim_status_pub_ = this->create_publisher<sentry_msgs::msg::AutoAIM>(
        "/autoaim/status", 10);

    // new subsriber
    _chasssis_cmd_vel_sub_ =
        this->create_subscription<geometry_msgs::msg::Twist>(
            "/chassis/cmd_vel", 10,

            std::bind(&ContactNode::chassis_cmd_vel_callback, this,
                      std::placeholders::_1));

    _autoaim_status_sub_ = this->create_subscription<sentry_msgs::msg::AutoAIM>(
        "/autoaim/target", 10,
        std::bind(&ContactNode::autoaim_sub_callback, this,
                  std::placeholders::_1));
    // timers
    __rx_timer__ = this->create_wall_timer(
        10ms, std::bind(&ContactNode::rx_timer_callback, this));
    __tx_timer__ = this->create_wall_timer(
        10ms, std::bind(&ContactNode::tx_timer_callback, this));

    // new publisher
    _robot_hp_pub_ = this->create_publisher<sentry_msgs::msg::RobotHP>(
        "/referee/robot_hp", 10);
    _bullet_speed_pub_ = this->create_publisher<sentry_msgs::msg::ShootData>(
        "/referee/bullet_speed", 10);
    _bullet_remain_pub_ =
        this->create_publisher<sentry_msgs::msg::BulletRemaining>(
            "/referee/bullet_remaining", 10);
    _field_events_pub_ = this->create_publisher<sentry_msgs::msg::FieldEvents>(
        "/referee/field_events", 10);
    _game_status_pub_ = this->create_publisher<sentry_msgs::msg::GameStatus>(
        "/referee/game_status", 10);
    _game_result_pub_ = this->create_publisher<sentry_msgs::msg::GameResult>(
        "/referee/game_result", 10);
    _robot_buff_pub_ = this->create_publisher<sentry_msgs::msg::RobotBuff>(
        "/referee/robot_buff", 10);
    _robot_position_pub_ =
        this->create_publisher<sentry_msgs::msg::RobotPosition>(
            "/referee/robot_position", 10);
    _robot_status_pub_ = this->create_publisher<sentry_msgs::msg::RobotStatus>(
        "/referee/robot_status", 10);
    _client_command_pub_ =
        this->create_publisher<sentry_msgs::msg::ClientCommand>(
            "/referee/client_command", 10);
    _client_receive_pub_ =
        this->create_publisher<sentry_msgs::msg::ClientReceive>(
            "/referee/client_receive", 10);
    _dr16_receive_pub_ = this->create_publisher<sentry_msgs::msg::DR16Receiver>(
        "/remote_control", 10);
    _ins_pub_ = this->create_publisher<tf2_msgs::msg::TFMessage>("/tf", 10);
  }

  void chassis_cmd_vel_callback(const geometry_msgs::msg::Twist msg) {
    comm.tx_struct_.linear_x = -msg.linear.x;
    comm.tx_struct_.linear_y = msg.linear.y;
    comm.tx_struct_.angular_z = msg.angular.z;
  }

  void autoaim_sub_callback(const sentry_msgs::msg::AutoAIM msg) {
    // update the data in the tx_struct_
    comm.tx_struct_.yaw = msg.yaw;
    comm.tx_struct_.pitch = msg.pitch;
    if (msg.target_distance > 0)
      comm.tx_struct_.autofire = 1;
    else
      comm.tx_struct_.autofire = 0;
  }

  std::string bytesToHexStr(const uint8_t *data, size_t length) {
    std::ostringstream oss;
    for (size_t i = 0; i < length; i++) {
      // 设置为十六进制、至少 2 位宽度、不足补 0
      oss << std::hex << std::setw(2) << std::setfill('0')
          << static_cast<unsigned int>(data[i]);

      // 如果想在每字节之间用空格或其它分隔符，可以加上:
      if (i + 1 < length) {
        oss << " ";
      }
    }
    return oss.str();
  }

  void rx_timer_callback() {
    // publish the quaternion
    geometry_msgs::msg::Quaternion quaternion_msg;
    quaternion_msg.x = comm.rx_struct_fast_.q[0];
    quaternion_msg.y = comm.rx_struct_fast_.q[1];
    quaternion_msg.z = comm.rx_struct_fast_.q[2];
    quaternion_msg.w = comm.rx_struct_fast_.q[3]; // 写到打符pre
    quaternion_pub_->publish(quaternion_msg);
    RCLCPP_INFO(this->get_logger(), "Published quaternion: %f, %f, %f, %f",
                quaternion_msg.x, quaternion_msg.y, quaternion_msg.z,
                quaternion_msg.w);

    // NOTE: new added the tf2 msg for sending the quaternion
    geometry_msgs::msg::TransformStamped transform_stamped;
    auto ins_msg = tf2_msgs::msg::TFMessage();

    transform_stamped.header.stamp = this->get_clock()->now();
    // the frame_id is the parent frame
    transform_stamped.header.frame_id = "map";
    // the child_frame_id is the child frame
    transform_stamped.child_frame_id = "robot";

    // WARNING: I don't know whether we can use this   // set the translation
    // transform_stamped.transform.translation.x =

    // set the rotation
    transform_stamped.transform.rotation.x = comm.rx_struct_fast_.q[0];
    transform_stamped.transform.rotation.y = comm.rx_struct_fast_.q[1];
    transform_stamped.transform.rotation.z = comm.rx_struct_fast_.q[2];
    transform_stamped.transform.rotation.w = comm.rx_struct_fast_.q[3];

    ins_msg.transforms.push_back(transform_stamped);
    _ins_pub_->publish(ins_msg);

    // publish the current yaw and pitch
    // TODO: where is the according message
    sentry_msgs::msg::AutoAIM autoaim_status_publishing_msg;
    autoaim_status_publishing_msg.pitch = comm.rx_struct_fast_.pitch;
    autoaim_status_publishing_msg.yaw = comm.rx_struct_fast_.yaw;
    RCLCPP_INFO(this->get_logger(), "Published pitch: %f, yaw: %f",
                autoaim_status_publishing_msg.pitch,
                autoaim_status_publishing_msg.yaw);
    _autoaim_status_pub_->publish(autoaim_status_publishing_msg);

    sentry_msgs::msg::DR16Receiver dr16_receiver_msg;
    dr16_receiver_msg.sw_right = comm.rx_struct_fast_.lever_mode;
    _dr16_receive_pub_->publish(dr16_receiver_msg);
    RCLCPP_INFO(this->get_logger(), "Published dr16 receiver: %d",
                dr16_receiver_msg.sw_right);

    // publish vision mode
    auto mode_msg = std_msgs::msg::Int32();
    mode_msg.data =
        comm.rx_struct_slow_.vision_mode; // NOTE: 可以替换为任何你想要的整数值
    // 发布消息
    _mode_pub_->publish(mode_msg);
    RCLCPP_INFO(this->get_logger(), "Published mode: %d", mode_msg.data);

    // TODO: please add the topic stamp somewhere
    uint32_t timestamp = comm.rx_struct_slow_.timestamp;
    auto timestamp_msg = std_msgs::msg::Int32();
    timestamp_msg.data = timestamp;
    _timestamp_pub_->publish(timestamp_msg);
    RCLCPP_INFO(this->get_logger(), "Published timestamp: %d", timestamp);

    // the bullet_speed publisher
    sentry_msgs::msg::ShootData shoot_data;
    shoot_data.bullet_speed = comm.rx_struct_fast_.bullet_speed;
    _bullet_speed_pub_->publish(shoot_data);
    RCLCPP_INFO(this->get_logger(), "Publishing bullet_speed: %f",
                shoot_data.bullet_speed);

    uint8_t side = comm.rx_struct_slow_.current_side_color;
    sentry_msgs::msg::RobotHP enemy_hp_msg;
    auto enemy_hp = comm.rx_struct_slow_.enemy_hp;
    // TODO: please update the color info in the C board
    if (side == 2) { // we are blue
      enemy_hp_msg.red_1_hero_hp = enemy_hp[0];
      enemy_hp_msg.red_2_engineer_hp = enemy_hp[1];
      enemy_hp_msg.red_3_infantry_hp = enemy_hp[2];
      enemy_hp_msg.red_4_infantry_hp = enemy_hp[3];
      enemy_hp_msg.red_5_infantry_hp = enemy_hp[4];
      enemy_hp_msg.red_7_sentry_hp = enemy_hp[5];
    } else if (side == 1) { // we are red
      enemy_hp_msg.blue_1_hero_hp = enemy_hp[0];
      enemy_hp_msg.blue_2_engineer_hp = enemy_hp[1];
      enemy_hp_msg.blue_3_infantry_hp = enemy_hp[2];
      enemy_hp_msg.blue_4_infantry_hp = enemy_hp[3];
      enemy_hp_msg.blue_5_infantry_hp = enemy_hp[4];
      enemy_hp_msg.blue_7_sentry_hp = enemy_hp[5];
    } else {
      RCLCPP_WARN(this->get_logger(), "Invalid side color: %d", side);
    }
    _robot_hp_pub_->publish(enemy_hp_msg);
    RCLCPP_INFO(this->get_logger(),
                "Published enemy hp: %d, %d, %d, %d, %d, %d", enemy_hp[0],
                enemy_hp[1], enemy_hp[2], enemy_hp[3], enemy_hp[4],
                enemy_hp[5]);
    RCLCPP_INFO(this->get_logger(), "current_side_color: %d", side);

    // publish the bullet remaining
    sentry_msgs::msg::BulletRemaining bullet_remaining_msgs;
    bullet_remaining_msgs.remaining_17mm_num =
        comm.rx_struct_slow_.bullet_remaining_info;
    // bullet_remaining_msgs.remaining_42mm_num =
    // comm.rx_struct_slow_.bullet_remaining_info[1];
    // bullet_remaining_msgs.remaining_coin_num =
    // comm.rx_struct_slow_.bullet_remaining_info[2];
    _bullet_remain_pub_->publish(bullet_remaining_msgs);
    RCLCPP_INFO(this->get_logger(), "Published bullet remaining: %d",
                comm.rx_struct_slow_.bullet_remaining_info);

    // publish the field events
    sentry_msgs::msg::FieldEvents field_events_msgs;
    field_events_msgs.supplier_1_occupation =
        comm.rx_struct_slow_.field_events[0];
    field_events_msgs.supplier_2_occupation =
        comm.rx_struct_slow_.field_events[1];
    field_events_msgs.supplier_3_occupation =
        comm.rx_struct_slow_.field_events[2];
    field_events_msgs.power_rune_activation_point_occupation =
        comm.rx_struct_slow_.field_events[3];
    field_events_msgs.small_power_rune_activation_status =
        comm.rx_struct_slow_.field_events[4];
    field_events_msgs.big_power_rune_activation_status =
        comm.rx_struct_slow_.field_events[5];
    field_events_msgs.r2b2_ground_occupation =
        comm.rx_struct_slow_.field_events[6];
    field_events_msgs.r3b3_ground_occupation =
        comm.rx_struct_slow_.field_events[7];
    field_events_msgs.r4b4_ground_occupation =
        comm.rx_struct_slow_.field_events[8];
    field_events_msgs.base_has_shield = comm.rx_struct_slow_.field_events[9];
    field_events_msgs.outpost_alive = comm.rx_struct_slow_.field_events[10];

    _field_events_pub_->publish(field_events_msgs);

    // game satatus
    sentry_msgs::msg::GameStatus game_status_msgs;
    game_status_msgs.game_type = comm.rx_struct_slow_.game_type;
    game_status_msgs.game_progress = comm.rx_struct_slow_.game_progress;
    game_status_msgs.stage_remain_time = comm.rx_struct_slow_.state_remain_time;
    game_status_msgs.sync_time_stamp = comm.rx_struct_slow_.sync_time_stamp;
    _game_status_pub_->publish(game_status_msgs);
    RCLCPP_INFO(this->get_logger(), "Published game status: %d, %d, %d",
                comm.rx_struct_slow_.game_type,
                comm.rx_struct_slow_.game_progress,
                comm.rx_struct_slow_.state_remain_time);

    // game result
    sentry_msgs::msg::GameResult game_result_msgs;
    game_result_msgs.winner = comm.rx_struct_slow_.winner;
    _game_result_pub_->publish(game_result_msgs);
    RCLCPP_INFO(this->get_logger(), "Published game result: %d",
                comm.rx_struct_slow_.winner);

    // robot buff
    sentry_msgs::msg::RobotBuff robot_buffs_msgs;
    robot_buffs_msgs.robot_replenishing_blood =
        comm.rx_struct_slow_.robot_buffs[0];
    robot_buffs_msgs.shooter_cooling_acceleration =
        comm.rx_struct_slow_.robot_buffs[1];
    robot_buffs_msgs.robot_defense_bonus = comm.rx_struct_slow_.robot_buffs[2];
    robot_buffs_msgs.robot_attack_bonus = comm.rx_struct_slow_.robot_buffs[3];
    RCLCPP_INFO(this->get_logger(), "Published robot buff: %d, %d, %d, %d",
                comm.rx_struct_slow_.robot_buffs[0],
                comm.rx_struct_slow_.robot_buffs[1],
                comm.rx_struct_slow_.robot_buffs[2],
                comm.rx_struct_slow_.robot_buffs[3]);

    // robot position
    sentry_msgs::msg::RobotPosition robot_position_msgs;
    robot_position_msgs.x = comm.rx_struct_slow_.robot_position[0];
    robot_position_msgs.y = comm.rx_struct_slow_.robot_position[1];
    robot_position_msgs.z = comm.rx_struct_slow_.robot_position[2];
    robot_position_msgs.yaw = comm.rx_struct_slow_.robot_position[3];

    _robot_position_pub_->publish(robot_position_msgs);
    RCLCPP_INFO(this->get_logger(), "Published robot position: %f, %f, %f, %f",
                comm.rx_struct_slow_.robot_position[0],
                comm.rx_struct_slow_.robot_position[1],
                comm.rx_struct_slow_.robot_position[2],
                comm.rx_struct_slow_.robot_position[3]);

    // robot status
    sentry_msgs::msg::RobotStatus robot_status_msgs;
    robot_status_msgs.robot_id = comm.rx_struct_slow_.robot_status_0[0];
    robot_status_msgs.robot_level = comm.rx_struct_slow_.robot_status_0[1];
    robot_status_msgs.remain_hp = comm.rx_struct_slow_.robot_status_1[0];
    robot_status_msgs.max_hp = comm.rx_struct_slow_.robot_status_1[1];
    robot_status_msgs.shooter_17mm_id1_cooling_rate =
        comm.rx_struct_slow_.robot_status_1[2];
    robot_status_msgs.shooter_17mm_id1_cooling_limit =
        comm.rx_struct_slow_.robot_status_1[3];
    robot_status_msgs.shooter_17mm_id1_speed_limit =
        comm.rx_struct_slow_.robot_status_1[4];
    robot_status_msgs.shooter_17mm_id2_cooling_rate =
        comm.rx_struct_slow_.robot_status_1[5];
    robot_status_msgs.shooter_17mm_id2_cooling_limit =
        comm.rx_struct_slow_.robot_status_1[6];
    robot_status_msgs.shooter_17mm_id2_speed_limit =
        comm.rx_struct_slow_.robot_status_1[7];
    robot_status_msgs.chassis_power_limit =
        comm.rx_struct_slow_.robot_status_1[11];
    robot_status_msgs.gimbal_power_output =
        comm.rx_struct_slow_.robot_status_1[12];
    robot_status_msgs.chassis_power_output =
        comm.rx_struct_slow_.robot_status_1[13];
    robot_status_msgs.shooter_power_output =
        comm.rx_struct_slow_.robot_status_1[14];

    // client command
    sentry_msgs::msg::ClientCommand client_command_msgs;
    client_command_msgs.target_position_x =
        comm.rx_struct_slow_.command_target_position[0];
    client_command_msgs.target_position_y =
        comm.rx_struct_slow_.command_target_position[1];
    client_command_msgs.target_position_z =
        comm.rx_struct_slow_.command_target_position[2];
    client_command_msgs.keyboard_key_pressed =
        comm.rx_struct_slow_.keyboard_key_pressed;
    client_command_msgs.target_robot_id =
        comm.rx_struct_slow_.command_target_robot_id;
    _client_command_pub_->publish(client_command_msgs);
    RCLCPP_INFO(this->get_logger(),
                "Published client command: %f, %f, %f, %d, %d",
                comm.rx_struct_slow_.command_target_position[0],
                comm.rx_struct_slow_.command_target_position[1],
                comm.rx_struct_slow_.command_target_position[2],
                comm.rx_struct_slow_.keyboard_key_pressed,
                comm.rx_struct_slow_.command_target_robot_id);

    // client receive
    sentry_msgs::msg::ClientCommand client_receive_msgs;
    client_receive_msgs.target_position_x =
        comm.rx_struct_slow_.receive_target_position[0];
    client_receive_msgs.target_position_y =
        comm.rx_struct_slow_.receive_target_position[1];
    client_receive_msgs.target_robot_id =
        comm.rx_struct_slow_.receive_target_robot_id;

    RCLCPP_INFO(this->get_logger(), "Published client receive: %f, %f, %d",
                comm.rx_struct_slow_.receive_target_position[0],
                comm.rx_struct_slow_.receive_target_position[1],
                comm.rx_struct_slow_.receive_target_robot_id);
  }

  void tx_timer_callback() {
    TxPacket tx_packet;

    comm.tx_struct_.header = 0x5A;

    // NOTE: comm.tx_struct_.yaw and pitch has been  updated in the
    // autoaim_sub_callback
    //
    comm.tx_struct_.actual1 = comm.tx_struct_.pitch;
    comm.tx_struct_.actial2 = comm.tx_struct_.yaw;
    std::cout << comm.tx_struct_.actual1 << " " << comm.tx_struct_.actial2;
    for (int i = 0; i < 4; i++) {
      comm.tx_struct_.raw1[i] = 0;
      comm.tx_struct_.raw2[i] = 0;
    }

    // NOTE: comm.tx_struct_.linear_x, linear_y and angular_z has been updated
    // in the chassis_cmd_vel

    // TODO: delete the RCLCPP_INFO
    RCLCPP_INFO(
        this->get_logger(),
        "sending to C board: yaw: %.2f, pitch: %.2f, actual1: %.2f, actual2 "
        ":%.2f, linear_x: %.2f, linear_y: %.2f, angular_z: %.2f",
        comm.tx_struct_.yaw, comm.tx_struct_.pitch, comm.tx_struct_.actual1,
        comm.tx_struct_.actial2, comm.tx_struct_.linear_x,
        comm.tx_struct_.linear_y, comm.tx_struct_.angular_z);

    RCLCPP_INFO(this->get_logger(), "len: %ld", sizeof(comm.rx_struct_slow_));
    RCLCPP_INFO(this->get_logger(), "len: %ld", sizeof(comm.rx_struct_fast_));

    memcpy(&tx_packet, &comm.tx_struct_, sizeof(CommPort::ProjectileTx));

    comm.tx_struct_.checksum =
        Crc8Append(&tx_packet, sizeof(CommPort::ProjectileTx));
    comm.Write(&tx_packet, sizeof(CommPort::ProjectileTx), true);
  }

private:
  // publisher
  rclcpp::Publisher<geometry_msgs::msg::Quaternion>::SharedPtr quaternion_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr _mode_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr _timestamp_pub_;

  rclcpp::Publisher<sentry_msgs::msg::AutoAIM>::SharedPtr _autoaim_status_pub_;
  rclcpp::Publisher<sentry_msgs::msg::ShootData>::SharedPtr _bullet_speed_pub_;

  rclcpp::Subscription<sensor_msgs::msg::Joy>::SharedPtr _joy_sub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr
      _chasssis_cmd_vel_sub_;
  rclcpp::Subscription<sentry_msgs::msg::AutoAIM>::SharedPtr
      _autoaim_status_sub_;

  // timers
  rclcpp::TimerBase::SharedPtr __rx_timer__;
  rclcpp::TimerBase::SharedPtr __tx_timer__;

  rclcpp::Publisher<sentry_msgs::msg::RobotHP>::SharedPtr _robot_hp_pub_;
  rclcpp::Publisher<sentry_msgs::msg::BulletRemaining>::SharedPtr
      _bullet_remain_pub_;
  rclcpp::Publisher<sentry_msgs::msg::FieldEvents>::SharedPtr
      _field_events_pub_;
  rclcpp::Publisher<sentry_msgs::msg::GameStatus>::SharedPtr _game_status_pub_;
  rclcpp::Publisher<sentry_msgs::msg::GameResult>::SharedPtr _game_result_pub_;
  rclcpp::Publisher<sentry_msgs::msg::RobotBuff>::SharedPtr _robot_buff_pub_;
  rclcpp::Publisher<sentry_msgs::msg::RobotPosition>::SharedPtr
      _robot_position_pub_;
  rclcpp::Publisher<sentry_msgs::msg::RobotStatus>::SharedPtr
      _robot_status_pub_;
  rclcpp::Publisher<sentry_msgs::msg::ClientCommand>::SharedPtr
      _client_command_pub_;
  rclcpp::Publisher<sentry_msgs::msg::ClientReceive>::SharedPtr
      _client_receive_pub_;
  rclcpp::Publisher<sentry_msgs::msg::DR16Receiver>::SharedPtr
      _dr16_receive_pub_;
  rclcpp::Publisher<tf2_msgs::msg::TFMessage>::SharedPtr _ins_pub_;

  CommPort comm;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<ContactNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
