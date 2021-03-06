/******************************************************************************
 * Copyright (C) 2013 by Jerome Maye                                          *
 * jerome.maye@gmail.com                                                      *
 *                                                                            *
 * This program is free software; you can redistribute it and/or modify       *
 * it under the terms of the Lesser GNU General Public License as published by*
 * the Free Software Foundation; either version 3 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * Lesser GNU General Public License for more details.                        *
 *                                                                            *
 * You should have received a copy of the Lesser GNU General Public License   *
 * along with this program. If not, see <http://www.gnu.org/licenses/>.       *
 ******************************************************************************/

#include "aslam/calibration/error-terms/ErrorTermPose.h"

#include <Eigen/Dense>

#include <sm/kinematics/rotations.hpp>
#include <sm/kinematics/quaternion_algebra.hpp>
#include <sm/kinematics/RotationVector.hpp>

namespace aslam {
  namespace calibration {

/******************************************************************************/
/* Constructors and Destructor                                                */
/******************************************************************************/

    ErrorTermPose::ErrorTermPose(const aslam::backend::TransformationExpression& T,
        const Eigen::Vector3d & t,
        const Eigen::Vector4d & q,
        const Covariance& sigma2,
        ErrorTermGroupReference etgr) :
        ErrorTermGroupMember(etgr),
        _q(q), _C(sm::kinematics::quat2r(q)), _t(t),_T(T) {
      setInvR(sigma2.inverse());
      aslam::backend::DesignVariable::set_t dv;
      _T.getDesignVariables(dv);
      setDesignVariablesIterator(dv.begin(), dv.end());
    }

    ErrorTermPose::ErrorTermPose(const aslam::backend::TransformationExpression& T, const Eigen::Vector3d & t,
        const Eigen::Vector4d & q, const Eigen::Matrix3d & cov_t, const Eigen::Matrix3d & cov_r, ErrorTermGroupReference etgr):
        ErrorTermPose(T, t, q, Covariance::Identity(), etgr) {
      Covariance Q = Covariance::Zero();
      Q.topLeftCorner<3, 3>() = cov_t;
      Q.bottomRightCorner<3, 3>() = cov_r;
      setInvR(Q.inverse());
    }


    ErrorTermPose::ErrorTermPose(const aslam::backend::TransformationExpression& T, const PoseMeasurement& pm, const Eigen::Matrix3d & cov_t, const Eigen::Matrix3d & cov_r, ErrorTermGroupReference etgr) :
      ErrorTermPose(T, pm.t, pm.q, cov_t, cov_r, etgr) {
    }

    sm::kinematics::RotationVector rotVec;

    double ErrorTermPose::evaluateErrorImplementation() {
      const Eigen::Matrix4d T = _T.toTransformationMatrix();
      error_t error;
      error.head<3>() = T.topRightCorner<3, 1>() - _t;
      error.tail<3>() = rotVec.rotationMatrixToParameters(T.topLeftCorner<3, 3>() * _C.transpose());
      setError(error);
      return evaluateChiSquaredError();
    }

    void ErrorTermPose::evaluateJacobiansImplementation(aslam::backend::JacobianContainer& jacobians) {
      const Eigen::Matrix4d T = _T.toTransformationMatrix();
      Eigen::Matrix<double, 6, 6> J = Eigen::Matrix<double, 6, 6>::Identity();
      J.topRightCorner<3, 3>() = sm::kinematics::crossMx(T.topRightCorner<3, 1>());
      J.bottomRightCorner<3, 3>() = (rotVec.parametersToSMatrix(rotVec.rotationMatrixToParameters(T.topLeftCorner<3, 3>() * _C.transpose()))).inverse();
      _T.evaluateJacobians(jacobians, J);
    }

  }
}

Eigen::VectorXd aslam::calibration::ErrorTermPose::getPrediction() const {
  const Eigen::Matrix4d T = _T.toTransformationMatrix();
  Eigen::VectorXd pred(7);
  pred.head<3>() = T.topRightCorner<3, 1>();
  pred.tail<4>() = sm::kinematics::r2quat(T.topLeftCorner<3, 3>());
  return pred;
}


Eigen::VectorXd aslam::calibration::ErrorTermPose::getMeasurement() const {
  Eigen::VectorXd m(7);
  m.head<3>() = _t;
  m.tail<4>() = _q;
  return m;
}
