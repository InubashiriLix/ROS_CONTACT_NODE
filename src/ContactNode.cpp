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
        _camera_id_pub_ =
            this->create_publisher<std_msgs::msg::Int32>("/camera_id", 10);  // camera id

        _autoaim_sub_ = this->create_subscription<gary_msgs::msg::AutoAIM>(
            "/autoaim/target", 10,
            std::bind(&ContactNode::autoaim_callback, this, std::placeholders::_1));
        _autoaim_pub_ = this->create_publisher<gary_msgs::msg::AutoAIM>("/autoaim/status", 10);

        _quaternion_pub_ =
            this->create_publisher<geometry_msgs::msg::Quaternion>("/quaternion", 10);

        _color_pub_ = this->create_publisher<std_msgs::msg::Int32>("/color", 10);

        _autoaim_mode_pub_ = this->create_publisher<std_msgs::msg::Int32>("/autoaim/mode", 10);
        _autoaim_decision_pub_ =
            this->create_publisher<std_msgs::msg::Int32>("/autoaim/decision", 10);

        __timer_tx__ =
            this->create_wall_timer(10ms, std::bind(&ContactNode::tx_timer_callback, this));
        __timer_rx__ =
            this->create_wall_timer(10ms, std::bind(&ContactNode::rx_timer_callback, this));
    }

    void autoaim_callback(const gary_msgs::msg::AutoAIM msg) {
        flag_new_autoaim_msg = 1;
        RCLCPP_INFO(this->get_logger(),
                    "===================== FROM autoaim  =====================");
        comm.set_tx_pitch_angle(msg.pitch + comm.get_rx_pitch());
        comm.set_tx_yaw_angle(msg.yaw + comm.get_rx_yaw());

        comm.set_tx_target_found(msg.target_distance > 0 ? 1 : 0);
        RCLCPP_INFO(this->get_logger(), "Pitch: %f, Yaw: %f", msg.pitch, msg.yaw);
    }

    void tx_timer_callback() {
        if (flag_new_autoaim_msg)  // if there comes the new message from the autoaim
        {
            RCLCPP_INFO(this->get_logger(), "===================== SENDING =====================");

            comm.set_tx_SOF(PROJECTILE_TX_SOF);

            RCLCPP_INFO(this->get_logger(), "Pitch: %f, Yaw: %f", comm.get_tx_struct().pitch_angle,
                        comm.get_tx_struct().yaw_angle);

            comm.Write(comm.get_tx_buffer(), comm.tx_struct_len, true);
            flag_new_autoaim_msg = 0;
            // NOTE: reset the angle in case of out of controlling when no message is
            // sended
            comm.set_tx_pitch_angle(comm.get_rx_pitch());
            comm.set_tx_yaw_angle(comm.get_rx_yaw());
            comm.set_tx_target_found(0);
        }
    }

    void rx_timer_callback() {
        RCLCPP_INFO(this->get_logger(), "===================== RECEIVING =====================");

        gary_msgs::msg::AutoAIM autoaim_msg;
        autoaim_msg.pitch = comm.get_rx_pitch();
        autoaim_msg.yaw = comm.get_rx_pitch();
        _autoaim_pub_->publish(autoaim_msg);
        RCLCPP_INFO(this->get_logger(), "Pitch: %f, Yaw: %f", autoaim_msg.pitch, autoaim_msg.yaw);

        // publish quaternion
        geometry_msgs::msg::Quaternion quaternion_msg;
        quaternion_msg.x = comm.get_rx_quat()[0];
        quaternion_msg.y = comm.get_rx_quat()[1];
        quaternion_msg.z = comm.get_rx_quat()[2];
        quaternion_msg.w = comm.get_rx_quat()[3];
        _quaternion_pub_->publish(quaternion_msg);
        RCLCPP_INFO(this->get_logger(), "Quaternion: %f %f %f %f", quaternion_msg.x,
                    quaternion_msg.y, quaternion_msg.z, quaternion_msg.w);

        // the color of the armor
        std_msgs::msg::Int32 color_msg;
        color_msg.data = comm.get_rx_is_self_team_red();
        _color_pub_->publish(color_msg);
        RCLCPP_INFO(this->get_logger(), "Color: %d", comm.get_rx_is_self_team_red());

        // autoaim mode
        std_msgs::msg::Int32 autoaim_mode_msg;
        autoaim_mode_msg.data = comm.get_rx_vision_mode();
        _autoaim_mode_pub_->publish(autoaim_mode_msg);
        RCLCPP_INFO(this->get_logger(), "Autoaim Mode: %d", comm.get_rx_vision_mode());

        // TODO: Cuurent time publish
        std_msgs::msg::Int32 current_time_msg;
        current_time_msg.data = comm.get_rx_system_time();
        RCLCPP_INFO(this->get_logger(), "System Time: %d", current_time_msg.data);

        // TODO: hp infos publsh
        // comm.get_rx_hp_data();
        RCLCPP_INFO(this->get_logger(), "example HP: %d", comm.get_rx_hp_data()[0]);

        // TODO: buller speed
        RCLCPP_INFO(this->get_logger(), "Buller Speed: %f", comm.get_rx_buller_speed());

        // TODO: for lower mechine: add the upper or lower camera state variable
        // we constraint 1 for upper cam, 0 for lower cam
        std_msgs::msg::Int32 camera_id_msg;
        camera_id_msg.data = comm.get_rx_camera_id();
        // camera_id_msg.data = 1;
        _camera_id_pub_->publish(camera_id_msg);
        RCLCPP_INFO(this->get_logger(), "current cam: %s",
                    ((camera_id_msg.data == 1) ? "upper cam" : "lower cam"));
    }

   private:
    float pitch_offset = 0.00;
    float yaw_offset = 0.00;

    uint8_t flag_new_autoaim_msg = 0;

    rclcpp::Subscription<gary_msgs::msg::AutoAIM>::SharedPtr _autoaim_sub_;
    rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr _camera_id_pub_;
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
