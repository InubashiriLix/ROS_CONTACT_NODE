
#include <CommPort.h>
#include <gary_msgs/msg/auto_aim.hpp>
#include <geometry_msgs/msg/quaternion.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/int32.hpp>

using namespace std::chrono_literals;

class ContactNode : public rclcpp::Node {
public:
  ContactNode() : Node("contact") {
    comm.Start();
    _autoaim_sub_ = this->create_subscription<gary_msgs::msg::AutoAIM>(
        "/autoaim/target", 10,
        std::bind(&ContactNode::autoaim_callback, this, std::placeholders::_1));
    _autoaim_pub_ =
        this->create_publisher<gary_msgs::msg::AutoAIM>("/autoaim/status", 10);
    _quaternion_pub_ = this->create_publisher<geometry_msgs::msg::Quaternion>(
        "/quaternion", 10);
    _color_pub_ = this->create_publisher<std_msgs::msg::Int32>("/color", 10);
    _autoaim_mode_pub_ =
        this->create_publisher<std_msgs::msg::Int32>("/autoaim/mode", 10);
    _autoaim_decision_pub_ =
        this->create_publisher<std_msgs::msg::Int32>("/autoaim/decision", 10);

    __timer_tx__ = this->create_wall_timer(
        10ms, std::bind(&ContactNode::tx_timer_callback, this));
    __timer_rx__ = this->create_wall_timer(
        10ms, std::bind(&ContactNode::rx_timer_callback, this));
  }

  void autoaim_callback(const gary_msgs::msg::AutoAIM msg) {
    flag_new_autoaim_msg = 1;
    RCLCPP_INFO(this->get_logger(),
                "===================== FROM autoaim  =====================");
    comm.set_tx_pitch(msg.pitch + comm.get_rx_Pitch());
    comm.set_tx_yaw(msg.yaw + comm.get_rx_Yaw());

    comm.set_tx_found(msg.target_distance > 0 ? 1 : 0);
    RCLCPP_INFO(this->get_logger(), "Pitch: %f, Yaw: %f", msg.pitch, msg.yaw);
  }

  void tx_timer_callback() {
    if (flag_new_autoaim_msg) // if there comes the new message from the autoaim
    {
      RCLCPP_INFO(this->get_logger(),
                  "===================== SENDING =====================");

      comm.set_tx_header(0xA3);

      // the pitch, yaw, found are setted in the autoaim_callback
      // WARNING: the shoot_or_not is setted to 0 in order to secure demage
      comm.set_tx_shoot_or_not(0);

      RCLCPP_INFO(this->get_logger(), "Pitch: %f, Yaw: %f",
                  comm.get_tx_struct().pitch, comm.get_tx_struct().yaw);

      comm.Write(comm.get_tx_buffer(), comm.tx_struct_len, true);
      flag_new_autoaim_msg = 0;
    }
  }

  void rx_timer_callback() {
    RCLCPP_INFO(this->get_logger(),
                "===================== RECEIVING =====================");

    gary_msgs::msg::AutoAIM autoaim_msg;
    autoaim_msg.pitch = comm.get_rx_Pitch();
    autoaim_msg.yaw = comm.get_rx_Yaw();

    // NOTE: what is the roll?
    // msg.roll = comm.get_Roll();

    _autoaim_pub_->publish(autoaim_msg);
    RCLCPP_INFO(this->get_logger(), "Pitch: %f, Yaw: %f", autoaim_msg.pitch,
                autoaim_msg.yaw);

    // publish quaternion
    geometry_msgs::msg::Quaternion quaternion_msg;
    float temp_q[4] = {1, 1, 1, 1};
    comm.get_rx_quaternion(temp_q);
    quaternion_msg.x = temp_q[0];
    quaternion_msg.y = temp_q[1];
    quaternion_msg.z = temp_q[2];
    quaternion_msg.w = temp_q[3];
    _quaternion_pub_->publish(quaternion_msg);
    RCLCPP_INFO(this->get_logger(), "Quaternion: %f %f %f %f", temp_q[0],
                temp_q[1], temp_q[2], temp_q[3]);

    // the color of the armor
    std_msgs::msg::Int32 color_msg;
    color_msg.data = comm.get_rx_color();
    _color_pub_->publish(color_msg);

    RCLCPP_INFO(this->get_logger(), "Color: %d", comm.get_rx_color());

    // autoaim mode
    std_msgs::msg::Int32 autoaim_mode_msg;
    autoaim_mode_msg.data = comm.get_rx_autoaim_mode();
    _autoaim_mode_pub_->publish(autoaim_mode_msg);

    RCLCPP_INFO(this->get_logger(), "Autoaim Mode: %d",
                comm.get_rx_autoaim_mode());

    // the autoaim decision
    std_msgs::msg::Int32 autoaim_decision_msg;
    autoaim_decision_msg.data = comm.get_rx_shoot_decision();
    _autoaim_decision_pub_->publish(autoaim_decision_msg);

    RCLCPP_INFO(this->get_logger(), "Autoaim Decision: %d",
                comm.get_rx_shoot_decision());
  }

private:
  float pitch_offset = 0.00;
  float yaw_offset = 0.00;

  uint8_t flag_new_autoaim_msg = 0;

  rclcpp::Subscription<gary_msgs::msg::AutoAIM>::SharedPtr _autoaim_sub_;
  rclcpp::Publisher<gary_msgs::msg::AutoAIM>::SharedPtr _autoaim_pub_;
  rclcpp::Publisher<geometry_msgs::msg::Quaternion>::SharedPtr _quaternion_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr _color_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr _autoaim_mode_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr _autoaim_decision_pub_;

  rclcpp::TimerBase::SharedPtr __timer_tx__;
  rclcpp::TimerBase::SharedPtr __timer_rx__;
  CommPort comm;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<ContactNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
