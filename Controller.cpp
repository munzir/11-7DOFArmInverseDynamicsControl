/*
 * Copyright (c) 2014-2016, Humanoid Lab, Georgia Tech Research Corporation
 * Copyright (c) 2014-2017, Graphics Lab, Georgia Tech Research Corporation
 * Copyright (c) 2016-2017, Personal Robotics Lab, Carnegie Mellon University
 * All rights reserved.
 *
 * This file is provided under the following "BSD-style" License:
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 */

#include "Controller.hpp"
#include <nlopt.hpp>

//==============================================================================
Controller::Controller(dart::dynamics::SkeletonPtr _robot,
                       dart::dynamics::BodyNode* _endEffector)
  : mRobot(_robot),
    mEndEffector(_endEffector)
{
  assert(_robot != nullptr);
  assert(_endEffector != nullptr);

  int dof = mRobot->getNumDofs();

  mForces.setZero(dof);

  mKp.setZero();
  mKv.setZero();

  for (int i = 0; i < 3; ++i)
  {
    mKp(i, i) = 750.0;
    mKv(i, i) = 250.0;
  }

  // Remove position limits
  for (int i = 0; i < dof; ++i)
    _robot->getJoint(i)->setPositionLimitEnforced(false);

  // Set joint damping
  for (int i = 0; i < dof; ++i)
    _robot->getJoint(i)->setDampingCoefficient(0, 0.5);
}

//==============================================================================
Controller::~Controller()
{
}

//==============================================================================
void Controller::update(const Eigen::Vector3d& _targetPosition)
{
  using namespace dart;

  // Get equation of motions
  Eigen::Vector3d x    = mEndEffector->getTransform().translation();
  Eigen::Vector3d dx   = mEndEffector->getLinearVelocity();
  Eigen::MatrixXd invM = mRobot->getInvMassMatrix();                   // n x n
  Eigen::MatrixXd M = mRobot->getMassMatrix();                         // n x n
  Eigen::VectorXd Cg   = mRobot->getCoriolisAndGravityForces();        // n x 1
  math::LinearJacobian Jv   = mEndEffector->getLinearJacobian();       // 3 x n
  math::LinearJacobian dJv  = mEndEffector->getLinearJacobianDeriv();  // 3 x n
  Eigen::VectorXd dq        = mRobot->getVelocities();                 // n x 1
  // Compute the pseudo-inverse of the Jacobian 
  Eigen::MatrixXd pinv_Jv = Jv.transpose() * (Jv * Jv.transpose()
                            + 0.0025 * Eigen::Matrix6d::Identity()).inverse();

  // Compute operational space values
  Eigen::MatrixXd A = Jv*invM;                 // 3 x n
  Eigen::Vector3d b = /*-(A*Cg) + */dJv*dq;    // 3 x 1
  Eigen::MatrixXd M2 = Jv*invM*Jv.transpose(); // 3 x 3

  // Compute virtual operational space spring force at the end effector
  Eigen::Vector3d f = -mKp*(x - _targetPosition) - mKv*dx;

  // Compute desired operational space acceleration given f
  Eigen::Vector3d desired_ddx = b + M2*f;
  Eigen::Vector3d M2f = M2*f;

  // Compute acceleration in joint angles ddq
  Eigen::VectorXd desired_ddq = pinv_Jv*(desired_ddx - dJv*dq);
  int tol = 1e-8;
  // Gravity compensation
  mForces = Cg;

  // Computing optimization.........
  nlopt::opt opt(nlopt::LD_MMA, 3);
  opt.set_min_objective(nonlinear_objective,desired_ddx, M2f);
  opt.add_inequality_constraint(lconstraint, M, desired_ddq,  tol);

  try{
    nlopt::result result = opt.optimize(desired_ddx, M2f) ;
  }
  catch(std::exception &e){
    std::cout<<"NLOPT failed"<<std::endl;
  }
  // Compute joint space forces to acheive the desired acceleration by solving:
  // tau = A*ddq + Cg
  mForces += A.colPivHouseholderQr().solve(desired_ddx - b);

  // Apply the joint space forces to the robot
  mRobot->setForces(mForces);
}

//==============================================================================
double Controller::nonlinear_objective(Eigen::Vector3d desired_ddx, Eigen::Vector3d ddx)
{
  Eigen::Vector3d objective = ddx - desired_ddx;
  return objective.dot(objective);

}

//==============================================================================
Eigen::VectorXd Controller::lconstraint(Eigen::MatrixXd M, Eigen::VectorXd desired_ddq)
{
  Eigen::VectorXd Cg = mRobot->getCoriolisAndGravityForces();        // n x 1
  Eigen::VectorXd mForces_lower_bound;
  return M*desired_ddq + Cg - mForces_lower_bound;
}

//==============================================================================
dart::dynamics::SkeletonPtr Controller::getRobot() const
{
  return mRobot;
}

//==============================================================================
dart::dynamics::BodyNode* Controller::getEndEffector() const
{
  return mEndEffector;
}

//==============================================================================
void Controller::keyboard(unsigned char /*_key*/, int /*_x*/, int /*_y*/)
{
}
