#include "semantic_map_manager/visualizer.h"

namespace semantic_map_manager {

Visualizer::Visualizer(rclcpp::NodeOptions options, int node_id)
    : Node("visualizer", options), node_id_(node_id) {
  ego_tf_name_ = "ego_vehicle_vis_" + std::to_string(node_id_);

  RCLCPP_INFO(this->get_logger(), "node_id_ = %d", node_id_);
  RCLCPP_INFO(this->get_logger(), "ego_tf_name_ = %s", ego_tf_name_.c_str());

  std::string ego_vehicle_vis_topic = "/vis/agent_" + std::to_string(node_id_) + "/ego_vehicle_vis";
  std::string obstacle_map_vis_topic = "/vis/agent_" + std::to_string(node_id_) + "/obstacle_map";
  std::string surrounding_lane_net_vis_topic = "/vis/agent_" + std::to_string(node_id_) + "/surrounding_lane_net_vis";
  std::string local_lanes_vis_topic = "/vis/agent_" + std::to_string(node_id_) + "/local_lanes_vis";
  std::string ego_vehicle_behavior_topic = "/vis/agent_" + std::to_string(node_id_) + "/ego_behavior_vis";
  std::string pred_intention_topic = "/vis/agent_" + std::to_string(node_id_) + "/pred_initial_intention_vis";
  std::string pred_traj_openloop_topic = "/vis/agent_" + std::to_string(node_id_) + "/pred_traj_openloop_vis";
  std::string surrounding_vehicle_topic = "/vis/agent_" + std::to_string(node_id_) + "/surrounding_vehicle_vis";
  std::string speed_limit_topic = "/vis/agent_" + std::to_string(node_id_) + "/speed_limit";

  ego_vehicle_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>(ego_vehicle_vis_topic, 1);
  obstacle_map_pub_ = this->create_publisher<nav_msgs::msg::OccupancyGrid>(obstacle_map_vis_topic, 1);
  surrounding_lane_net_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>(surrounding_lane_net_vis_topic, 1);
  local_lanes_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>(local_lanes_vis_topic, 1);
  behavior_vis_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>(ego_vehicle_behavior_topic, 1);
  pred_traj_openloop_vis_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>(pred_traj_openloop_topic, 1);
  pred_intention_vis_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>(pred_intention_topic, 1);
  surrounding_vehicle_vis_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>(surrounding_vehicle_topic, 1);
  speed_limit_vis_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>(speed_limit_topic, 1);

  ego_to_map_tf_ = std::make_shared<tf2_ros::TransformBroadcaster>(this);
}

void Visualizer::VisualizeData(const SemanticMapManager &smm) {
  if (smm.time_stamp() < kEPS) return;  // if time stamp unset, return
  auto time_stamp = rclcpp::Time(smm.time_stamp());
  VisualizeDataWithStamp(time_stamp, smm);
  SendTfWithStamp(time_stamp, smm);
}

void Visualizer::VisualizeDataWithStamp(const rclcpp::Time &stamp,
                                        const SemanticMapManager &smm) {
  VisualizeEgoVehicle(stamp, smm.ego_vehicle());
  VisualizeObstacleMap(stamp, smm.obstacle_map());
  VisualizeSurroundingLaneNet(stamp, smm.surrounding_lane_net(),
                              std::vector<int>());
  VisualizeLocalLanes(stamp, smm.local_lanes(), smm, std::vector<int>());
  VisualizeBehavior(stamp, smm.ego_behavior());
  VisualizeIntentionPrediction(stamp, smm.semantic_surrounding_vehicles());
  VisualizeOpenloopTrajPrediction(stamp, smm.openloop_pred_trajs());
  VisualizeSurroundingVehicles(stamp, smm.surrounding_vehicles(),
                               smm.key_vehicle_ids());
  VisualizeSpeedLimit(stamp, smm.RetTrafficInfoSpeedLimit());
}

void Visualizer::VisualizeDataWithStampForPlayback(
    const rclcpp::Time &stamp, const SemanticMapManager &smm,
    const std::vector<int> &deleted_lane_ids) {
  VisualizeEgoVehicle(stamp, smm.ego_vehicle());
  VisualizeObstacleMap(stamp, smm.obstacle_map());
  VisualizeSurroundingLaneNet(stamp, smm.surrounding_lane_net(),
                              deleted_lane_ids);
  VisualizeLocalLanes(stamp, smm.local_lanes(), smm, deleted_lane_ids);
  VisualizeBehavior(stamp, smm.ego_behavior());
  VisualizeIntentionPrediction(stamp, smm.semantic_surrounding_vehicles());
  VisualizeOpenloopTrajPrediction(stamp, smm.openloop_pred_trajs());
  VisualizeSurroundingVehicles(stamp, smm.surrounding_vehicles(),
                               smm.key_vehicle_ids());
  VisualizeSpeedLimit(stamp, smm.RetTrafficInfoSpeedLimit());
}

void Visualizer::VisualizeSurroundingVehicles(
    const rclcpp::Time &stamp, const common::VehicleSet &vehicle_set,
    const std::vector<int> &nearby_ids) {
  visualization_msgs::msg::MarkerArray vehicle_marker_list;
  for (const auto &v : vehicle_set.vehicles) {
    visualization_msgs::msg::MarkerArray vehicle_marker;
    common::ColorARGB color_obb(0.5, 0.2, 0.7, 1.0);
    common::ColorARGB color_vel_vec(0.4, 0.0, 1.0, 1.0);
    common::ColorARGB color_steer(0.4, 1.0, 1.0, 1.0);
    if (v.second.type().compare("brokencar") == 0) {
      color_obb = common::ColorARGB(0.4, 1.0, 0.2, 0.2);
    }
    if (nearby_ids.end() !=
        std::find(nearby_ids.begin(), nearby_ids.end(), v.second.id())) {
      color_obb.a = 0.9;
      color_vel_vec.a = 0.9;
      color_steer.a = 0.9;
    }
    common::VisualizationUtil::GetRosMarkerArrayUsingVehicle(
        v.second, color_obb, color_vel_vec, color_steer, 1, &vehicle_marker);
    for (auto &marker : vehicle_marker.markers)
      vehicle_marker_list.markers.push_back(marker);
  }
  int num_markers = static_cast<int>(vehicle_marker_list.markers.size());
  common::VisualizationUtil::FillHeaderIdInMarkerArray(
      stamp, std::string("map"), last_surrounding_vehicle_marker_cnt_,
      &vehicle_marker_list);
  last_surrounding_vehicle_marker_cnt_ = num_markers;
  surrounding_vehicle_vis_pub_->publish(vehicle_marker_list);
}

void Visualizer::VisualizeEgoVehicle(const rclcpp::Time &stamp,
                                     const common::Vehicle &vehicle) {
  visualization_msgs::msg::MarkerArray vehicle_marker;
  common::ColorARGB color_obb(1.0, 0.66, 0.66, 0.66);
  common::ColorARGB color_vel_vec(1.0, 0.0, 1.0, 1.0);
  common::ColorARGB color_steer(1.0, 1.0, 1.0, 1.0);
  common::VisualizationUtil::GetRosMarkerArrayUsingVehicle(
      vehicle, color_obb, color_vel_vec, color_steer, 1, &vehicle_marker);
  common::VisualizationUtil::FillStampInMarkerArray(stamp, &vehicle_marker);
  ego_vehicle_pub_->publish(vehicle_marker);
}

void Visualizer::VisualizeObstacleMap(
    const rclcpp::Time &stamp,
    const common::GridMapND<ObstacleMapType, 2> &obstacle_map) {
  if (obstacle_map.data_size() < 1) return;
  nav_msgs::msg::OccupancyGrid occ_map;
  common::VisualizationUtil::GetRosOccupancyGridUsingGripMap2D(
      obstacle_map, this->now(), &occ_map);
  occ_map.header.stamp = stamp;
  obstacle_map_pub_->publish(occ_map);
}

void Visualizer::SendTfWithStamp(const rclcpp::Time &stamp,
                                 const SemanticMapManager &smm) {
  if (smm.time_stamp() < kEPS) {
    return;  // if time stamp unset, return
  }
  // * Publish TF: map -> ego_vehicle
  Vec3f state = smm.ego_vehicle().Ret3DofState();
  geometry_msgs::msg::Pose pose;
  common::VisualizationUtil::GetRosPoseFrom3DofState(state, &pose);
  geometry_msgs::msg::TransformStamped transformStamped;
  transformStamped.header.stamp = stamp;
  transformStamped.header.frame_id = "map";
  transformStamped.child_frame_id = ego_tf_name_;
  transformStamped.transform.translation.x = pose.position.x;
  transformStamped.transform.translation.y = pose.position.y;
  transformStamped.transform.translation.z = pose.position.z;
  transformStamped.transform.rotation = pose.orientation;
  ego_to_map_tf_->sendTransform(transformStamped);
}

void Visualizer::VisualizeSurroundingLaneNet(
    const rclcpp::Time &stamp, const common::LaneNet &lane_net,
    const std::vector<int> &deleted_lane_ids) {
  visualization_msgs::msg::MarkerArray lane_net_marker;
  int id_cnt = 0;
  for (auto iter = lane_net.lane_set.begin(); iter != lane_net.lane_set.end();
       ++iter) {
    if (deleted_lane_ids.end() != std::find(deleted_lane_ids.begin(),
                                            deleted_lane_ids.end(),
                                            iter->second.id)) {
      continue;
    }
    visualization_msgs::msg::Marker lane_marker;
    common::VisualizationUtil::GetRosMarkerLineStripUsing2DofVec(
        iter->second.lane_points, common::cmap.at("sky blue"),
        Vec3f(0.1, 0.1, 0.1), iter->second.id, &lane_marker);
    lane_marker.header.stamp = stamp;
    lane_marker.header.frame_id = "map";
    lane_marker.id = id_cnt++;
    lane_net_marker.markers.push_back(lane_marker);
    // Visualize the start and end point
    visualization_msgs::msg::Marker start_point_marker, end_point_marker,
        lane_id_text_marker;
    {
      start_point_marker.header.stamp = stamp;
      start_point_marker.header.frame_id = "map";
      Vec2f pt = *(iter->second.lane_points.begin());
      common::VisualizationUtil::GetRosMarkerSphereUsingPoint(
          Vec3f(pt(0), pt(1), 0.0), common::ColorARGB(1.0, 0.2, 0.6, 1.0),
          Vec3f(0.5, 0.5, 0.5), id_cnt++, &start_point_marker);
      lane_id_text_marker.header.stamp = stamp;
      lane_id_text_marker.header.frame_id = "map";
      common::VisualizationUtil::GetRosMarkerTextUsingPositionAndString(
          Vec3f(pt(0), pt(1), 0.5), std::to_string(iter->second.id),
          common::ColorARGB(1.0, 0.0, 0.0, 1.0), Vec3f(0.6, 0.6, 0.6), id_cnt++,
          &lane_id_text_marker);
    }
    {
      end_point_marker.header.stamp = stamp;
      end_point_marker.header.frame_id = "map";
      Vec2f pt = *(iter->second.lane_points.rbegin());
      common::VisualizationUtil::GetRosMarkerSphereUsingPoint(
          Vec3f(pt(0), pt(1), 0.0), common::ColorARGB(1.0, 0.2, 0.6, 1.0),
          Vec3f(0.5, 0.5, 0.5), id_cnt++, &end_point_marker);
    }

    lane_net_marker.markers.push_back(start_point_marker);
    lane_net_marker.markers.push_back(end_point_marker);
    lane_net_marker.markers.push_back(lane_id_text_marker);
  }
  int num_markers = static_cast<int>(lane_net_marker.markers.size());
  common::VisualizationUtil::FillHeaderIdInMarkerArray(
      stamp, std::string("map"), last_surrounding_lanes_cnt_,
      &lane_net_marker);
  last_surrounding_lanes_cnt_ = num_markers;
  surrounding_lane_net_pub_->publish(lane_net_marker);
}

void Visualizer::VisualizeLocalLanes(
    const rclcpp::Time &stamp,
    const std::unordered_map<int, common::Lane> &local_lanes,
    const SemanticMapManager &smm, const std::vector<int> &deleted_lane_ids) {
  static int last_mks_num = 0;
  visualization_msgs::msg::MarkerArray mks;
  for (const auto &p_lane : local_lanes) {
    bool is_to_del = false;
    for (const auto &del_id : deleted_lane_ids) {
      if (smm.IsLocalLaneContainsLane(p_lane.first, del_id)) {
        is_to_del = true;
        break;
      }
    }
    if (is_to_del) continue;

    visualization_msgs::msg::Marker mk;
    common::VisualizationUtil::GetMarkerByLane(
        p_lane.second, 1.0, Vec3f(1.0, 0.0, 0.0),
        common::cmap.at("magenta").set_a(0.2), -0.3, &mk);
    mks.markers.push_back(mk);
  }
  common::VisualizationUtil::FillHeaderIdInMarkerArray(
      stamp, std::string("map"), last_mks_num, &mks);
  last_mks_num = mks.markers.size();
  local_lanes_pub_->publish(mks);
}

void Visualizer::VisualizeBehavior(const rclcpp::Time &stamp,
                                   const common::SemanticBehavior &behavior) {
  visualization_msgs::msg::MarkerArray behavior_marker_arr;
  common::VisualizationUtil::GetRosMarkerArrUsingSemanticBehavior(
      behavior, &behavior_marker_arr);
  common::VisualizationUtil::FillStampInMarkerArray(stamp,
                                                    &behavior_marker_arr);

  int num_markers = static_cast<int>(behavior_marker_arr.markers.size());
  common::VisualizationUtil::FillHeaderIdInMarkerArray(
      stamp, std::string("map"), last_behavior_marker_cnt_,
      &behavior_marker_arr);
  last_behavior_marker_cnt_ = num_markers;

  behavior_vis_pub_->publish(behavior_marker_arr);
}

void Visualizer::VisualizeSpeedLimit(
    const rclcpp::Time &stamp, const vec_E<common::SpeedLimit> &speed_limits) {
  visualization_msgs::msg::MarkerArray traffic_signal_arr;
  int id_cnt = 0;
  double offset_len = 0;
  for (int i = 0; i < static_cast<int>(speed_limits.size()); ++i) {
    std::string str_start = "Speed limit: ";
    std::string str_end = "Release\n";

    common::ColorARGB start_marker_color = common::ColorARGB(1.0, 1.0, 1.0, 0.0);
    common::ColorARGB end_marker_color = common::ColorARGB(1.0, 0.0, 1.0, 0.0);
    if (speed_limits[i].vel_range()(1) < kEPS) {
      str_start = "Red light: ";
      str_end = "Forbidden\n";
      start_marker_color = common::ColorARGB(1.0, 1.0, 0.0, 0.0);
      end_marker_color = common::ColorARGB(1.0, 1.0, 0.0, 0.0);
    }

    visualization_msgs::msg::Marker start_marker;
    start_marker.header.stamp = stamp;
    start_marker.header.frame_id = "map";
    double start_point_x = speed_limits[i].start_point()(0);
    double start_point_y = speed_limits[i].start_point()(1);
    double start_point_angle = speed_limits[i].start_angle();

    double start_x = start_point_x - offset_len * cos(start_point_angle);
    double start_y = start_point_y - offset_len * sin(start_point_angle);

    common::VisualizationUtil::GetRosMarkerMeshHexagonSignUsingPosition(
        Vec3f(start_x, start_y, start_point_angle), 1.0, start_marker_color,
        ++id_cnt, &start_marker);
    traffic_signal_arr.markers.push_back(start_marker);

    visualization_msgs::msg::Marker start_text_marker;
    start_text_marker.header.stamp = stamp;
    start_text_marker.header.frame_id = "map";
    str_start += std::string(common::GetStringByValueWithPrecision<double>(
                                 speed_limits[i].vel_range()(1), 1) +
                             "m/s");
    common::VisualizationUtil::GetRosMarkerTextUsingPositionAndString(
        Vec3f(start_x, start_y, 3.5), str_start, common::cmap.at("black"),
        Vec3f(0.75, 0.75, 0.75), ++id_cnt, &start_text_marker);
    traffic_signal_arr.markers.push_back(start_text_marker);

    visualization_msgs::msg::Marker end_marker;
    end_marker.header.stamp = stamp;
    end_marker.header.frame_id = "map";
    double end_point_x = speed_limits[i].end_point()(0);
    double end_point_y = speed_limits[i].end_point()(1);
    double end_point_angle = speed_limits[i].end_angle();

    double end_x = end_point_x - offset_len * cos(end_point_angle);
    double end_y = end_point_y - offset_len * sin(end_point_angle);

    common::VisualizationUtil::GetRosMarkerMeshHexagonSignUsingPosition(
        Vec3f(end_x, end_y, speed_limits[i].end_angle()), 1.0, end_marker_color,
        ++id_cnt, &end_marker);
    traffic_signal_arr.markers.push_back(end_marker);

    visualization_msgs::msg::Marker end_text_marker;
    end_text_marker.header.stamp = stamp;
    end_text_marker.header.frame_id = "map";
    common::VisualizationUtil::GetRosMarkerTextUsingPositionAndString(
        Vec3f(end_x, end_y, 3.5), str_end, common::cmap.at("black"),
        Vec3f(0.75, 0.75, 0.75), ++id_cnt, &end_text_marker);
    traffic_signal_arr.markers.push_back(end_text_marker);
  }
  int num_markers = static_cast<int>(traffic_signal_arr.markers.size());
  common::VisualizationUtil::FillHeaderIdInMarkerArray(
      stamp, std::string("map"), last_speed_limit_marker_cnt_,
      &traffic_signal_arr);
  last_speed_limit_marker_cnt_ = num_markers;
  speed_limit_vis_pub_->publish(traffic_signal_arr);
}

void Visualizer::VisualizeIntentionPrediction(
    const rclcpp::Time &stamp,
    const common::SemanticVehicleSet &semantic_vehicles) {
  visualization_msgs::msg::MarkerArray mks;
  int cnt = 0;
  for (const auto &p_sv : semantic_vehicles.semantic_vehicles) {
    auto semantic_vehicle = p_sv.second;
    // * lateral behavior prediction
    common::State state = semantic_vehicle.vehicle.state();
    geometry_msgs::msg::Point pt0, pt1;
    double z = 2.0;
    for (const auto &entry : semantic_vehicle.probs_lat_behaviors.probs) {
      common::LateralBehavior beh = entry.first;
      double prob = entry.second;

      if (prob < kEPS) continue;

      double angle_offset = 0.0;
      double length = prob * 2.0;

      if (beh == common::LateralBehavior::kLaneChangeRight) {
        angle_offset = -kPi / 2.0;
      } else if (beh == common::LateralBehavior::kLaneChangeLeft) {
        angle_offset = +kPi / 2.0;
      } else if (beh == common::LateralBehavior::kLaneKeeping) {
        angle_offset = 0.0;
      }
      pt0.x = state.vec_position(0);
      pt0.y = state.vec_position(1);
      pt0.z = z;

      pt1.x = state.vec_position(0) + cos(state.angle + angle_offset) * length;
      pt1.y = state.vec_position(1) + sin(state.angle + angle_offset) * length;
      pt1.z = z;

      visualization_msgs::msg::Marker behavior_mk;
      behavior_mk.id = cnt++;
      behavior_mk.type = visualization_msgs::msg::Marker::ARROW;
      behavior_mk.action = visualization_msgs::msg::Marker::MODIFY;
      behavior_mk.points.push_back(pt0);
      behavior_mk.points.push_back(pt1);
      behavior_mk.scale.x = 0.2;
      behavior_mk.scale.y = 0.5;
      common::VisualizationUtil::FillColorInMarker(common::cmap.at("yellow"),
                                                   &behavior_mk);
      mks.markers.push_back(behavior_mk);
    }
  }
  int num_mks = static_cast<int>(mks.markers.size());
  common::VisualizationUtil::FillHeaderIdInMarkerArray(
      stamp, std::string("map"), last_intention_marker_cnt_, &mks);
  last_intention_marker_cnt_ = num_mks;
  pred_intention_vis_pub_->publish(mks);
}

void Visualizer::VisualizeOpenloopTrajPrediction(
    const rclcpp::Time &stamp,
    const std::unordered_map<int, vec_E<common::State>> &openloop_pred_trajs) {
  visualization_msgs::msg::MarkerArray mks;
  // * openloop traj prediction
  for (const auto &p_traj : openloop_pred_trajs) {
    std::vector<common::Point> points;
    for (const auto &ps : p_traj.second) {
      common::Point pt(ps.vec_position(0), ps.vec_position(1));
      pt.z = 0.25;
      points.push_back(pt);
      visualization_msgs::msg::Marker point_marker;
      common::VisualizationUtil::GetRosMarkerCylinderUsingPoint(
          common::Point(pt), Vec3f(0.5, 0.5, 0.1), common::cmap.at("sky blue"),
          0, &point_marker);
      mks.markers.push_back(point_marker);
    }
    visualization_msgs::msg::Marker line_marker;
    common::VisualizationUtil::GetRosMarkerLineStripUsingPoints(
        points, Vec3f(0.1, 0.1, 0.1), common::cmap.at("sky blue"), 0,
        &line_marker);
    mks.markers.push_back(line_marker);
  }
  int num_mks = static_cast<int>(mks.markers.size());
  common::VisualizationUtil::FillHeaderIdInMarkerArray(
      stamp, std::string("map"), last_traj_list_marker_cnt_, &mks);
  last_traj_list_marker_cnt_ = num_mks;
  pred_traj_openloop_vis_pub_->publish(mks);
}

}  // namespace semantic_map_manager