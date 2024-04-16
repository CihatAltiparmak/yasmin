// Copyright (C) 2023  Miguel Ángel González Santamarta

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef YASMIN_ROS_MONITOR_STATE_HPP
#define YASMIN_ROS_MONITOR_STATE_HPP

#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"

#include "yasmin/blackboard/blackboard.hpp"
#include "yasmin/state.hpp"
#include "yasmin_ros/basic_outcomes.hpp"
#include "yasmin_ros/yasmin_node.hpp"

using std::placeholders::_1;

namespace yasmin_ros {

template <typename MsgT> class MonitorState : public yasmin::State {

  using MonitorHandler = std::function<std::string(
      std::shared_ptr<yasmin::blackboard::Blackboard>, std::shared_ptr<MsgT>)>;

public:
  MonitorState(std::string topic_name, std::vector<std::string> outcomes,
               MonitorHandler monitor_handler, rclcpp::QoS qos, int msg_queue)
      : MonitorState(topic_name, outcomes, monitor_handler, qos, msg_queue,
                     -1) {}

  MonitorState(std::string topic_name, std::vector<std::string> outcomes,
               MonitorHandler monitor_handler)
      : MonitorState(topic_name, outcomes, monitor_handler, 10, 10, -1) {}

  MonitorState(std::string topic_name, std::vector<std::string> outcomes,
               MonitorHandler monitor_handler, rclcpp::QoS qos, int msg_queue,
               int timeout)
      : MonitorState(nullptr, topic_name, outcomes, monitor_handler, qos,
                     msg_queue, timeout) {}

  MonitorState(rclcpp::Node *node, std::string topic_name,
               std::vector<std::string> outcomes,
               MonitorHandler monitor_handler, rclcpp::QoS qos, int msg_queue,
               int timeout)
      : State({}) {

    // set outcomes
    if (timeout > 0) {
      this->outcomes = {basic_outcomes::CANCEL};
    } else {
      this->outcomes = {};
    }

    if (outcomes.size() > 0) {
      for (std::string outcome : outcomes) {
        this->outcomes.push_back(outcome);
      }
    }

    if (node == nullptr) {
      this->node = YasminNode::get_instance();
    } else {
      this->node = node;
    }

    // other attributes
    this->monitor_handler = monitor_handler;
    this->msg_queue = msg_queue;
    this->timeout = timeout;
    this->time_to_wait = 1000;
    this->monitoring = false;

    // create subscriber
    this->sub = this->node->create_subscription<MsgT>(
        topic_name, qos, std::bind(&MonitorState::callback, this, _1));
  }

  std::string
  execute(std::shared_ptr<yasmin::blackboard::Blackboard> blackboard) override {

    float elapsed_time = 0;
    this->msg_list.clear();
    this->monitoring = true;

    while (this->msg_list.size() == 0) {
      std::this_thread::sleep_for(
          std::chrono::microseconds(this->time_to_wait));

      if (this->timeout > 0) {

        if (elapsed_time / 1e6 >= this->timeout) {
          this->monitoring = false;
          return basic_outcomes::CANCEL;
        }
      }

      elapsed_time += this->time_to_wait;
    }

    std::string outcome =
        this->monitor_handler(blackboard, this->msg_list.at(0));
    this->msg_list.erase(this->msg_list.begin());

    this->monitoring = false;
    return outcome;
  }

private:
  rclcpp::Node *node;
  std::shared_ptr<rclcpp::Subscription<MsgT>> sub;

  MonitorHandler monitor_handler;
  std::string topic_name;
  std::vector<std::shared_ptr<MsgT>> msg_list;
  int msg_queue;
  int timeout;
  int time_to_wait;
  bool monitoring;

  void callback(const typename MsgT::SharedPtr msg) {

    if (this->monitoring) {
      this->msg_list.push_back(msg);

      if ((int)this->msg_list.size() >= this->msg_queue) {
        this->msg_list.erase(this->msg_list.begin());
      }
    }
  }
};

} // namespace yasmin_ros

#endif
