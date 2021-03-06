#ifndef ASLAM_CALIBRATION_ACCELEROMETER_MEASUREMENT_H
#define ASLAM_CALIBRATION_ACCELEROMETER_MEASUREMENT_H

#include <Eigen/Core>

namespace aslam {
namespace calibration {
struct AccelerometerMeasurement {
  /// Acceleration [m^2/s]
  Eigen::Vector3d a;
  /// Covariance. Set (0,0) to 0.0 if not available.
  Eigen::Matrix3d cov;
};
}
}

#endif // ASLAM_CALIBRATION_ACCELEROMETER_MEASUREMENT_H
