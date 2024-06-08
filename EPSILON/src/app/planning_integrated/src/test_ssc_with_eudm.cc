/**
 * @file test_ssc_with_eudm.cc
 * @author HKUST Aerial Robotics Group
 * @brief test ssc planner with eudm behavior planner
 * @version 0.1
 * @date 2019-02
 * @copyright Copyright (c) 2019
 */

#include <chrono>
#include <iostream>
#include <memory>

#include "eudm_planner/eudm_server_ros.h"
#include "semantic_map_manager/data_renderer.h"
#include "semantic_map_manager/ros_adapter.h"
#include "semantic_map_manager/semantic_map_manager.h"
#include "semantic_map_manager/visualizer.h"
#include "ssc_planner/ssc_server_ros.h"
#include "rclcpp/rclcpp.hpp"

DECLARE_BACKWARD;
double ssc_planner_work_rate = 20.0;
double bp_work_rate = 20.0;

std::shared_ptr<planning::SscPlannerServer> p_ssc_server_{nullptr};
std::shared_ptr<planning::EudmPlannerServer> p_bp_server_{nullptr};

int BehaviorUpdateCallback(const semantic_map_manager::SemanticMapManager& smm) {
  if (p_ssc_server_) p_ssc_server_->PushSemanticMap(smm);
  return 0;
}

int SemanticMapUpdateCallback(const semantic_map_manager::SemanticMapManager& smm) {
  if (p_bp_server_) p_bp_server_->PushSemanticMap(smm);
  return 0;
}

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto node = rclcpp::Node::make_shared("planning_integrated");

  int ego_id;
  if (!node->get_parameter("ego_id", ego_id)) {
    RCLCPP_ERROR(node->get_logger(), "Failed to get param ego_id");
    return 1;
  }

  std::string agent_config_path;
  if (!node->get_parameter("agent_config_path", agent_config_path)) {
    RCLCPP_ERROR(node->get_logger(), "Failed to get param agent_config_path");
    return 1;
  }

  std::string bp_config_path;
  if (!node->get_parameter("bp_config_path", bp_config_path)) {
    RCLCPP_ERROR(node->get_logger(), "Failed to get param bp_config_path");
    return 1;
  }

  std::string ssc_config_path;
  if (!node->get_parameter("ssc_config_path", ssc_config_path)) {
    RCLCPP_ERROR(node->get_logger(), "Failed to get param ssc_config_path");
    return 1;
  }

  semantic_map_manager::SemanticMapManager semantic_map_manager(ego_id, agent_config_path);
  semantic_map_manager::RosAdapter smm_ros_adapter(rclcpp::NodeOptions(), &semantic_map_manager);
  smm_ros_adapter.BindMapUpdateCallback(SemanticMapUpdateCallback);

  double desired_vel;
  node->get_parameter("desired_vel", desired_vel);

  // Declare bp
  p_bp_server_ = std::make_shared<planning::EudmPlannerServer>(rclcpp::NodeOptions(), bp_work_rate, ego_id);
  p_bp_server_->set_user_desired_velocity(desired_vel);
  p_bp_server_->BindBehaviorUpdateCallback(BehaviorUpdateCallback);

  p_ssc_server_ = std::make_shared<planning::SscPlannerServer>(rclcpp::NodeOptions(), ssc_planner_work_rate, ego_id);

  p_bp_server_->Init(bp_config_path);
  p_ssc_server_->Init(ssc_config_path);
  smm_ros_adapter.Init();

  p_bp_server_->Start();
  p_ssc_server_->Start();

  rclcpp::Rate rate(100);
  while (rclcpp::ok()) {
    rclcpp::spin_some(node);
    rate.sleep();
  }

  rclcpp::shutdown();
  return 0;
}