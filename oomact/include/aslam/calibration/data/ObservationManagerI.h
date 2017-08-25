#ifndef H579F5188_F15B_47A4_B00E_B16F5109EE17
#define H579F5188_F15B_47A4_B00E_B16F5109EE17

#include <string>

#include "StorageI.h"
#include "../Timestamp.hpp"

namespace aslam {
namespace calibration {

struct Interval;
class Module;
class Sensor;

class ObservationManagerI {
 public:
  typedef StorageI<const Module*> Storage;

  ObservationManagerI();
  virtual ~ObservationManagerI();

  virtual Storage & getCurrentStorage() = 0;

  virtual void setLowestTimestamp(Timestamp lowestTimeStamp) = 0;
  virtual void addMeasurementTimestamp(Timestamp t, const Sensor & sensor) = 0;

  virtual double secsSinceStart(Timestamp timestamp) const = 0;
  virtual std::string secsSinceStart(const Interval & interval) const = 0;

  virtual const Interval& getCurrentEffectiveBatchInterval() const = 0;
};

} /* namespace calibration */
} /* namespace aslam */

#endif /* H579F5188_F15B_47A4_B00E_B16F5109EE17 */
