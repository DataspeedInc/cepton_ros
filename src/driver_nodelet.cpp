#include "driver_nodelet.hpp"

#include <cmath>
#include <cstdint>

#include <pcl_conversions/pcl_conversions.h>
#include <pluginlib/class_list_macros.h>

PLUGINLIB_EXPORT_CLASS(cepton_ros::DriverNodelet, nodelet::Nodelet);

namespace cepton_ros {

DriverNodelet::~DriverNodelet() { cepton_sdk_deinitialize(); }

static void global_on_error(cepton_sdk::SensorHandle sensor_handle,
                            cepton_sdk::SensorErrorCode error_code,
                            const char *const error_msg,
                            const void *const error_data,
                            size_t error_data_size, void *instance) {
  ((DriverNodelet *)instance)
      ->on_error(sensor_handle, error_code, error_msg, error_data,
                 error_data_size);
}

static void global_on_image_points(
    cepton_sdk::SensorHandle sensor_handle, std::size_t n_points,
    const cepton_sdk::SensorImagePoint *const image_points,
    void *const instance) {
  ((DriverNodelet *)instance)
      ->on_image_points(sensor_handle, n_points, image_points);
}

void DriverNodelet::onInit() {
  this->node_handle = getNodeHandle();
  this->private_node_handle = getPrivateNodeHandle();

  // Get parameters
  std::string capture_path = "";
  private_node_handle.param("capture_path", capture_path, capture_path);
  private_node_handle.param("combine_sensors", combine_sensors,
                            combine_sensors);
  int control_flags = 0;
  private_node_handle.param("control_flags", control_flags, control_flags);
  private_node_handle.param("output_namespace", output_namespace,
                            output_namespace);

  const std::string sensor_information_topic_id =
      output_namespace + "_sensor_information";
  sensor_information_publisher =
      node_handle.advertise<SensorInformation>(sensor_information_topic_id, 2);

  if (combine_sensors) {
    combined_image_points_publisher =
        node_handle.advertise<sensor_msgs::PointCloud2>(get_points_topic_id(0),
                                                        2);
    combined_points_publisher = node_handle.advertise<sensor_msgs::PointCloud2>(
        get_points_topic_id(0), 2);
  }

  // Initialize sdk
  cepton_sdk::SensorErrorCode error_code;

  const int ver = 10;
  auto options = cepton_sdk::create_options();
  options.control_flags = control_flags;
  options.frame.mode = CEPTON_SDK_FRAME_COVER;
  error_code = cepton_sdk::initialize(ver, options, global_on_error, this);
  if (error_code) {
    NODELET_FATAL("cepton_sdk::initialize failed: %s",
                  cepton_sdk::get_error_code_name(error_code));
    return;
  }

  // Listen
  error_code = cepton_sdk::listen_image_frames(global_on_image_points, this);
  if (error_code) {
    NODELET_FATAL("cepton_sdk_listen_image_frames failed: %s",
                  cepton_sdk::get_error_code_name(error_code));
    return;
  }

  // Start capture
  if (!capture_path.empty()) {
    error_code = cepton_sdk::capture_replay::open(capture_path.c_str());
    if (error_code) {
      NODELET_FATAL("cepton_sdk_capture_replay_open failed: %s",
                    cepton_sdk::get_error_code_name(error_code));
      return;
    }

    error_code = cepton_sdk::capture_replay::set_enable_loop(true);
    if (error_code) {
      NODELET_FATAL("cepton_sdk::capture_replay::set_enable_loop failed: %s",
                    cepton_sdk::get_error_code_name(error_code));
      return;
    }

    error_code = cepton_sdk::capture_replay::resume();
    if (error_code) {
      NODELET_FATAL("cepton_sdk::capture_replay::resume failed: %s",
                    cepton_sdk::get_error_code_name(error_code));
      return;
    }
  }
}

std::string DriverNodelet::get_image_points_topic_id(
    uint64_t sensor_serial_number) const {
  if (combine_sensors) {
    return (output_namespace + "_image_points");
  } else {
    return (output_namespace + "_image_points_" +
            std::to_string(sensor_serial_number));
  }
}

std::string DriverNodelet::get_points_topic_id(
    uint64_t sensor_serial_number) const {
  if (combine_sensors) {
    return (output_namespace + "_points");
  } else {
    return (output_namespace + "_points_" +
            std::to_string(sensor_serial_number));
  }
}

std::string DriverNodelet::get_frame_id(uint64_t sensor_serial_number) const {
  if (combine_sensors) {
    return output_namespace;
  } else {
    return (output_namespace + "_" + std::to_string(sensor_serial_number));
  }
}

ros::Publisher &DriverNodelet::get_image_points_publisher(
    uint64_t sensor_serial_number) {
  if (combine_sensors) {
    return combined_image_points_publisher;
  } else {
    if (!image_points_publishers.count(sensor_serial_number)) {
      std::string topic_id = get_image_points_topic_id(sensor_serial_number);
      image_points_publishers[sensor_serial_number] =
          node_handle.advertise<sensor_msgs::PointCloud2>(topic_id, 10);
    }
    return image_points_publishers.at(sensor_serial_number);
  }
}

ros::Publisher &DriverNodelet::get_points_publisher(
    uint64_t sensor_serial_number) {
  if (combine_sensors) {
    return combined_points_publisher;
  } else {
    if (!points_publishers.count(sensor_serial_number)) {
      std::string topic_id = get_points_topic_id(sensor_serial_number);
      points_publishers[sensor_serial_number] =
          node_handle.advertise<sensor_msgs::PointCloud2>(topic_id, 10);
    }
    return points_publishers.at(sensor_serial_number);
  }
}

void DriverNodelet::on_error(cepton_sdk::SensorHandle sensor_handle,
                             int error_code, const char *const error_msg,
                             const void *const error_data,
                             size_t error_data_size) {
  NODELET_WARN("%s: %s", cepton_sdk::get_error_code_name(error_code),
               error_msg);
  return;
}

void DriverNodelet::on_image_points(
    cepton_sdk::SensorHandle sensor_handle, std::size_t n_points,
    const cepton_sdk::SensorImagePoint *const p_image_points) {
  int error_code;

  // Get sensor info
  cepton_sdk::SensorInformation sensor_info;
  error_code = cepton_sdk::get_sensor_information(sensor_handle, sensor_info);
  if (error_code) {
    NODELET_WARN("cepton_sdk::get_sensor_information failed: %s",
                 cepton_sdk::get_error_code_name(error_code));
    return;
  }

  // Cache image points
  image_points.reserve(n_points);
  for (std::size_t i = 0; i < n_points; ++i) {
    image_points.push_back(p_image_points[i]);
  }

  // Publish sensor info
  publish_sensor_information(sensor_info);

  // Publish points
  uint64_t message_timestamp = pcl_conversions::toPCL(ros::Time::now());
  publish_image_points(sensor_info.serial_number, message_timestamp);
  publish_points(sensor_info.serial_number, message_timestamp);
  image_points.clear();
  points.clear();
}

void DriverNodelet::publish_sensor_information(
    const CeptonSensorInformation &sensor_information) {
  cepton_ros::SensorInformation msg;
  msg.handle = sensor_information.handle;
  msg.serial_number = sensor_information.serial_number;
  msg.model_name = sensor_information.model_name;
  msg.model = sensor_information.model;
  msg.firmware_version = sensor_information.firmware_version;
  sensor_information_publisher.publish(msg);
}

void DriverNodelet::publish_image_points(uint64_t sensor_serial_number,
                                         uint64_t message_timestamp) {
  CeptonImagePointCloud::Ptr image_point_cloud_ptr(new CeptonImagePointCloud());
  image_point_cloud_ptr->header.stamp = message_timestamp;
  image_point_cloud_ptr->header.frame_id = get_frame_id(sensor_serial_number);
  image_point_cloud_ptr->height = 1;
  image_point_cloud_ptr->width = image_points.size();

  image_point_cloud_ptr->resize(image_points.size());
  for (std::size_t i_image_point = 0; i_image_point < image_points.size();
       ++i_image_point) {
    const auto &cepton_image_point = image_points[i_image_point];
    auto &pcl_image_point = image_point_cloud_ptr->points[i_image_point];
    pcl_image_point.timestamp = cepton_image_point.timestamp;
    pcl_image_point.image_x = cepton_image_point.image_x;
    pcl_image_point.distance = cepton_image_point.distance;
    pcl_image_point.image_z = cepton_image_point.image_z;
    pcl_image_point.intensity = cepton_image_point.intensity;
    pcl_image_point.return_number = cepton_image_point.return_number;
    pcl_image_point.valid = cepton_image_point.valid;
  }

  get_image_points_publisher(sensor_serial_number)
      .publish(image_point_cloud_ptr);
}

void DriverNodelet::publish_points(uint64_t sensor_serial_number,
                                   uint64_t message_timestamp) {
  // Convert image points to points
  points.clear();
  points.resize(image_points.size());
  std::size_t i_point = 0;
  for (const auto &image_point : image_points) {
    if (image_point.distance == 0.0f) continue;
    cepton_sdk::convert_sensor_image_point_to_point(image_point,
                                                    points[i_point]);
    ++i_point;
  }
  points.resize(i_point);

  CeptonPointCloud::Ptr point_cloud_ptr(new CeptonPointCloud());
  point_cloud_ptr->header.stamp = message_timestamp;
  point_cloud_ptr->header.frame_id = get_frame_id(sensor_serial_number);
  point_cloud_ptr->height = 1;
  point_cloud_ptr->width = points.size();

  point_cloud_ptr->resize(points.size());
  for (std::size_t i_point = 0; i_point < points.size(); ++i_point) {
    const auto &cepton_point = points[i_point];
    auto &pcl_point = point_cloud_ptr->points[i_point];
    pcl_point.timestamp = cepton_point.timestamp;
    pcl_point.x = cepton_point.x;
    pcl_point.y = cepton_point.y;
    pcl_point.z = cepton_point.z;
    pcl_point.intensity = cepton_point.intensity;
    pcl_point.return_number = cepton_point.return_number;
    pcl_point.valid = cepton_point.valid;
  }

  get_points_publisher(sensor_serial_number).publish(point_cloud_ptr);
}

}  // namespace cepton_ros
