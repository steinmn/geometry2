/*
 * Copyright (c) 2008, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/** \author Tully Foote */

#include "tf2_ros/transform_listener.h"


using namespace tf2;


TransformListener::TransformListener(tf2::Buffer& buffer, bool spin_thread):
  dedicated_listener_thread_(NULL), buffer_(buffer), using_dedicated_thread_(false)
{
  if (spin_thread)
    initWithThread();
  else
    init();
}


TransformListener::~TransformListener()
{
  using_dedicated_thread_ = false;
  if (dedicated_listener_thread_)
  {
    dedicated_listener_thread_->join();
    delete dedicated_listener_thread_;
  }
}

void TransformListener::init()
{
  message_subscriber_tf_ = node_.subscribe<tf2_msgs::TFMessage>("/tf", 100, boost::bind(&TransformListener::subscription_callback, this, _1)); ///\todo magic number
  message_subscriber_tf_static_ = node_.subscribe<tf2_msgs::TFMessage>("/tf_static", 100, boost::bind(&TransformListener::static_subscription_callback, this, _1)); ///\todo magic number
}

void TransformListener::initWithThread()
{
  using_dedicated_thread_ = true;
  ros::SubscribeOptions ops_tf = ros::SubscribeOptions::create<tf2_msgs::TFMessage>("/tf", 100, boost::bind(&TransformListener::subscription_callback, this, _1), ros::VoidPtr(), &tf_message_callback_queue_); ///\todo magic number
  message_subscriber_tf_ = node_.subscribe(ops_tf);
  
  ros::SubscribeOptions ops_tf_static = ros::SubscribeOptions::create<tf2_msgs::TFMessage>("/tf_static", 100, boost::bind(&TransformListener::static_subscription_callback, this, _1), ros::VoidPtr(), &tf_message_callback_queue_); ///\todo magic number
  message_subscriber_tf_static_ = node_.subscribe(ops_tf_static);

  dedicated_listener_thread_ = new boost::thread(boost::bind(&TransformListener::dedicatedListenerThread, this));
}



void TransformListener::subscription_callback(const tf2_msgs::TFMessageConstPtr& msg)
{
  subscription_callback_impl(msg, false);
}
void TransformListener::static_subscription_callback(const tf2_msgs::TFMessageConstPtr& msg)
{
  subscription_callback_impl(msg, true);
}

void TransformListener::subscription_callback_impl(const tf2_msgs::TFMessageConstPtr& msg, bool is_static)
{

  const tf2_msgs::TFMessage& msg_in = *msg;
  for (unsigned int i = 0; i < msg_in.transforms.size(); i++)
  {
    std::map<std::string, std::string>* msg_header_map = msg_in.__connection_header.get();
    std::string authority;
    std::map<std::string, std::string>::iterator it = msg_header_map->find("callerid");
    if (it == msg_header_map->end())
    {
      ROS_WARN("Message recieved without callerid");
      authority = "no callerid";
    }
    else 
    {
      authority = it->second;
    }

    try
    {
      buffer_.setTransform(msg_in.transforms[i], authority, is_static);
    }
    
    catch (TransformException& ex)
    {
      ///\todo Use error reporting
      std::string temp = ex.what();
      ROS_ERROR("Failure to set recieved transform from %s to %s with error: %s\n", msg_in.transforms[i].child_frame_id.c_str(), msg_in.transforms[i].header.frame_id.c_str(), temp.c_str());
    }
  }
};




