/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2017-2020, Waterplus http://www.6-robot.com
*  All rights reserved.
* 
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
* 
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the WaterPlus nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  FOOTPRINTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/
/*!******************************************************************
 @author     ZhangWanjie
 ********************************************************************/

#include <ros/ros.h>
#include <sensor_msgs/Imu.h>
#include <tf/transform_broadcaster.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/Pose2D.h>
#include <geometry_msgs/Twist.h>
#include "WPR1_driver.h"
#include <math.h>

static CWPR1_driver m_wpr1;
static int nLastMotorPos[3];
void cmdVelCallback(const geometry_msgs::Twist::ConstPtr& msg)
{
    //ROS_INFO("[m_wpr1] liner(%.2f %.2f) angular(%.2f)", msg->linear.x,msg->linear.y,msg->angular.z);
    m_wpr1.Velocity(msg->linear.x,msg->linear.y,msg->angular.z);
}

static bool bFirstYaw = true;
static double fYawZero = 0;
static double fCurYaw = 0;
void IMUCallback(const sensor_msgs::Imu::ConstPtr & imu)
{
     tf::Quaternion quat(imu->orientation.x, imu->orientation.y, imu->orientation.z, imu->orientation.w);
     double yaw = 0;//tf::getYaw(quat);
     double roll = 0;
     double pitch = 0;
     tf::Matrix3x3(quat).getRPY(roll, pitch, yaw);
     //ROS_WARN("[IMU] yaw= %.2f, roll= %.2f, pitch= %.2f",yaw, roll, pitch);
     if(bFirstYaw == false)
     {
        fCurYaw = yaw;
     }
     else
     {
         //第一祯
         fYawZero = yaw;
         fCurYaw = fYawZero;
         bFirstYaw = false;
     }
}
 

static double fKVx = 1.0f/sqrt(3.0f);
static double fKVy = 2.0f/3.0f;
static double fKVz = 1.0f/3.0f;
static double fSumX =0;
static double fSumY =0;
static double fSumZ =0;
static double fOdomX =0;
static double fOdomY =0;
static double fOdomZ =0;
static geometry_msgs::Pose2D lastPose;
static geometry_msgs::Twist lastVel;
int main(int argc, char** argv)
{
    ros::init(argc,argv,"wpr1_core");
    ROS_INFO("[wpr1_core]");
    ros::NodeHandle n;
    ros::Subscriber cmd_vel_sub = n.subscribe("cmd_vel",10,&cmdVelCallback);

    ros::NodeHandle n_param("~");
    std::string strSerialPort;
    n_param.param<std::string>("serial_port", strSerialPort, "/dev/ttyUSB0");
    m_wpr1.Open(strSerialPort.c_str(),115200);

    sleep(1);

    bool bOdom = true;
    n_param.param<bool>("odom", bOdom, true);
    bool bImuOdom;
    n_param.param<bool>("imu_odom", bImuOdom, false);
    
    ros::Time current_time, last_time;
    current_time = ros::Time::now();
    last_time = ros::Time::now();
    ros::Rate r(100.0);

    ros::Publisher odom_pub;
    geometry_msgs::TransformStamped odom_trans;
    tf::TransformBroadcaster broadcaster;
    nav_msgs::Odometry odom;
    geometry_msgs::Quaternion odom_quat;

    odom_pub = n.advertise<nav_msgs::Odometry>("odom",2);
    odom_trans.header.frame_id = "odom";
    odom_trans.child_frame_id = "base_footprint";
    odom.header.frame_id = "odom";
    odom.child_frame_id = "base_footprint";
    odom_trans.transform.translation.x = 0;
    odom_trans.transform.translation.y = 0;
    odom_trans.transform.translation.z = 0;
    odom_trans.transform.rotation = tf::createQuaternionMsgFromYaw(0);
    odom.pose.pose.position.x = 0;
    odom.pose.pose.position.y = 0;
    odom.pose.pose.position.z = 0.0;
    odom.pose.pose.orientation = odom_quat;
    odom.twist.twist.linear.x = 0;
    odom.twist.twist.linear.y = 0;
    odom.twist.twist.linear.z = 0;
    odom.twist.twist.angular.x = 0;
    odom.twist.twist.angular.y = 0;
    odom.twist.twist.angular.z = 0;

    lastPose.x = lastPose.y = lastPose.theta = 0;
    lastVel.linear.x = lastVel.linear.y = lastVel.linear.z = lastVel.angular.x = lastVel.angular.y = lastVel.angular.z = 0;
    nLastMotorPos[0] = nLastMotorPos[1] = nLastMotorPos[2] = 0;

    bool bFirst = true;
    while(n.ok())
    {
        m_wpr1.ReadNewData();
        if(m_wpr1.nParseCount != 0 && bFirst == true)
        {
            continue;
        }
        else
        {
            if(bFirst == true)
            {
                //获得第一帧数据
                bFirst = false;
                nLastMotorPos[0] = m_wpr1.arMotorPos[0];
                nLastMotorPos[1] = m_wpr1.arMotorPos[1];
                nLastMotorPos[2] = m_wpr1.arMotorPos[2];
                ROS_INFO("reset");
            }
        }
        //ROS_INFO("[m_wpr1.nParseCount]= %d",m_wpr1.nParseCount);
        m_wpr1.nParseCount ++;
        if(m_wpr1.nParseCount > 100)
        {
            m_wpr1.arMotorPos[0] =0; nLastMotorPos[0] = 0;
            m_wpr1.arMotorPos[1] =0; nLastMotorPos[1] = 0;
            m_wpr1.arMotorPos[2] =0; nLastMotorPos[2] = 0;
            m_wpr1.nParseCount = 0;
        }
        
        last_time = current_time;
        current_time = ros::Time::now();

        double fVx,fVy,fVz;
        double fPosDiff[3];
        if(nLastMotorPos[0] != m_wpr1.arMotorPos[0] || nLastMotorPos[1] != m_wpr1.arMotorPos[1] || nLastMotorPos[2] != m_wpr1.arMotorPos[2])
        {
            //printf("last M[1]= %.4d  M[2]= %.4d M[3]= %.4d\n", nLastMotorPos[0],nLastMotorPos[1],nLastMotorPos[2]);
            //printf("robot M[1]= %.4d  M[2]= %.4d M[3]= %.4d\n", m_wpr1.arMotorPos[0],m_wpr1.arMotorPos[1],m_wpr1.arMotorPos[2]);

            fPosDiff[0] = (double)(m_wpr1.arMotorPos[0] - nLastMotorPos[0]); 
            fPosDiff[1] = (double)(m_wpr1.arMotorPos[1] - nLastMotorPos[1]);
            fPosDiff[2] = (double)(m_wpr1.arMotorPos[2] - nLastMotorPos[2]);
            
            fVx = (fPosDiff[1] - fPosDiff[0]) * fKVx;
            fVy = (fPosDiff[0] + fPosDiff[1]) - (fPosDiff[0] + fPosDiff[1] + fPosDiff[2])*fKVy;
            fVz = (fPosDiff[0] + fPosDiff[1] + fPosDiff[2])*fKVz;
            double fTimeDur = current_time.toSec() - last_time.toSec();
            //////////////////////////////////
            //方案一 通过电机码盘结算里程计数据
            fVx = fVx/(fTimeDur*85000);
            fVy = fVy/(fTimeDur*85000);
            fVz = fVz/(fTimeDur*18963);
            //////////////////////////////////
            // 方案二 直接把下发速度当作里程计积分依据
            // fVx = lastVel.linear.x;
            // fVy = lastVel.linear.y;
            // fVz = lastVel.angular.z; 
            ///////////////////////////////////
            
            double dx = (lastVel.linear.x*cos(lastPose.theta) - lastVel.linear.y*sin(lastPose.theta))*fTimeDur;
            double dy = (lastVel.linear.x*sin(lastPose.theta) + lastVel.linear.y*cos(lastPose.theta))*fTimeDur;

            lastPose.x += dx;
            lastPose.y += dy;
            if(bImuOdom == false)
            {
                lastPose.theta += (fVz*fTimeDur);
            }
            else
            {
                //用陀螺
                //ROS_INFO("[odom_imu]");
                lastPose.theta = (fCurYaw - fYawZero);
            }

            //ROS_INFO("[odom] x=%.2f y=%.2f a=%.2f",lastPose.x,lastPose.y,lastPose.theta);

            odom_quat = tf::createQuaternionMsgFromRollPitchYaw(0,0,lastPose.theta);
            //updata transform
            odom_trans.header.stamp = current_time;
            odom_trans.transform.translation.x = lastPose.x;
            odom_trans.transform.translation.y = lastPose.y;
            odom_trans.transform.translation.z = 0;
            odom_trans.transform.rotation = tf::createQuaternionMsgFromYaw(lastPose.theta);

            //filling the odometry
            odom.header.stamp = current_time;
            //position
            odom.pose.pose.position.x = lastPose.x;
            odom.pose.pose.position.y = lastPose.y;
            odom.pose.pose.position.z = 0.0;
            odom.pose.pose.orientation = odom_quat;

            //velocity
            odom.twist.twist.linear.x = fVx;
            odom.twist.twist.linear.y = fVy;
            odom.twist.twist.linear.z = 0;
            odom.twist.twist.angular.x = 0;
            odom.twist.twist.angular.y = 0;
            odom.twist.twist.angular.z = fVz;

            //plublishing the odometry and new tf
            broadcaster.sendTransform(odom_trans);
            odom_pub.publish(odom);

            lastVel.linear.x = fVx;
            lastVel.linear.y = fVy;
            lastVel.angular.z = fVz;

            nLastMotorPos[0] = m_wpr1.arMotorPos[0];
            nLastMotorPos[1] = m_wpr1.arMotorPos[1];
            nLastMotorPos[2] = m_wpr1.arMotorPos[2];
        }
        else
        {
            odom_trans.header.stamp = ros::Time::now();
            //plublishing the odometry and new tf
            broadcaster.sendTransform(odom_trans);
            odom_pub.publish(odom);
            //ROS_INFO("[odom] zero");
        }

        ros::spinOnce();
        r.sleep();
    }
}
