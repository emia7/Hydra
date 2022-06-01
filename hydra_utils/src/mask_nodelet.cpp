#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <nodelet/nodelet.h>
#include <pluginlib/class_list_macros.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

namespace hydra {

struct MaskNodelet : public nodelet::Nodelet  {
  void onInit() {
    auto& pnh = getPrivateNodeHandle();

    std::string mask_path = "";
    if (!pnh.getParam("mask_path", mask_path)) {
      ROS_FATAL("mask path is required!");
      throw std::runtime_error("mask path not specified");
    }

    ROS_INFO_STREAM("Reading mask from " << mask_path);
    mask_ = cv::imread(mask_path, cv::IMREAD_GRAYSCALE);
    if (mask_.empty()) {
      ROS_FATAL("invalid mask; mat is empty");
      throw std::runtime_error("invalid mask!");
    }

    auto& nh = getNodeHandle();
    transport_.reset(new image_transport::ImageTransport(nh));
    image_sub_ =
        transport_->subscribe("depth/image_raw", 1, &MaskNodelet::callback, this);
    image_pub_ = transport_->advertise("depth_masked/image_raw", 1);
  }

  void callback(const sensor_msgs::ImageConstPtr& msg) {
    cv_bridge::CvImageConstPtr img_ptr;
    try {
      img_ptr = cv_bridge::toCvShare(msg);
    } catch (const cv_bridge::Exception& e) {
      ROS_ERROR_STREAM("cv_bridge exception: " << e.what());
      return;
    }

    if (!result_image_) {
      result_image_.reset(new cv_bridge::CvImage());
      result_image_->encoding = img_ptr->encoding;
      result_image_->image = cv::Mat(img_ptr->image.rows, img_ptr->image.cols, img_ptr->image.type());
    }

    result_image_->image.setTo(0);
    result_image_->header = img_ptr->header;
    cv::bitwise_or(img_ptr->image, img_ptr->image, result_image_->image, mask_);

    image_pub_.publish(result_image_->toImageMsg());
  }

  std::unique_ptr<image_transport::ImageTransport> transport_;
  image_transport::Subscriber image_sub_;
  image_transport::Publisher image_pub_;

  cv_bridge::CvImagePtr result_image_;
  cv::Mat mask_;
};

}  // namespace hydra

PLUGINLIB_EXPORT_CLASS(hydra::MaskNodelet, nodelet::Nodelet)