#include <ros/ros.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <topic_tools/shape_shifter.h>
#include <unordered_map>
#include <string>
#include <memory>
#include <sys/ipc.h>

#include <ros/serialization.h>
#include <boost/interprocess/ipc/message_queue.hpp>

struct Data {
  char topicName[100];
  char md5sum[100];
  char datatype[100];
  char definition[100];
  uint8_t data[1000000];
  int size;
};

int main(int argc, char** argv) {
  // init node
  ros::init(argc, argv, "connect_two_master");
  ros::NodeHandle nh;
  ros::NodeHandle pnh("~");

  std::vector<std::string> topics;
  if(pnh.hasParam("topics")) pnh.getParam("topics", topics);
  bool slaveside = false;
  if(pnh.hasParam("slaveside")) pnh.getParam("slaveside", slaveside);
  int datasize = 1000000;
  if(pnh.hasParam("datasize")) pnh.getParam("datasize", datasize);
  int queuesize = 10;
  if(pnh.hasParam("queuesize")) pnh.getParam("queuesize", queuesize);
  bool reallocate = false;
  if(pnh.hasParam("reallocate")) pnh.getParam("reallocate", reallocate);

  char buffer[datasize];

  if(reallocate){
    boost::interprocess::message_queue::remove((ros::this_node::getName()+(slaveside?"SLAVE":"MASTER")).c_str());
    boost::interprocess::message_queue::remove((ros::this_node::getName()+(slaveside?"MASTER":"SLAVE")).c_str());
  }

  // get shm
  boost::interprocess::message_queue sndq(boost::interprocess::open_or_create,
                                          (ros::this_node::getName()+(slaveside?"SLAVE":"MASTER")).c_str(),
                                          queuesize,
                                          datasize);
  boost::interprocess::message_queue rcvq(boost::interprocess::open_or_create,
                                          (ros::this_node::getName()+(slaveside?"MASTER":"SLAVE")).c_str(),
                                          queuesize,
                                          datasize);
  // clear shm. (大昔のデータが残っている場合がある))
  {
    uint64_t rcv_size;
    unsigned priority;
    while(rcvq.try_receive(buffer, sizeof(buffer), rcv_size, priority)) continue;
  }

  std::vector<ros::Subscriber> subs;
  std::unordered_map<std::string, ros::Publisher> pubMap;

  for(int i=0;i<topics.size();i++){
    subs.push_back(nh.subscribe<topic_tools::ShapeShifter>(topics[i], 1, [&,i](const topic_tools::ShapeShifter::ConstPtr& topic_msg){
          if(topics[i].length()+1 +
             topic_msg->getMD5Sum().length()+1 +
             topic_msg->getDataType().length()+1 +
             topic_msg->getMessageDefinition().length()+1 +
             topic_msg->size()
             > datasize){
            std::cerr << "sizeof [" << topics[i] << "] " <<
              topics[i].length()+1 +
              topic_msg->getMD5Sum().length()+1 +
              topic_msg->getDataType().length()+1 +
              topic_msg->getMessageDefinition().length()+1 +
              topic_msg->size() << " > " << datasize << std::endl;
            exit(1);
          }
          int idx=0;
          strcpy(buffer+idx,topics[i].c_str()); idx+=topics[i].length()+1;
          strcpy(buffer+idx,topic_msg->getMD5Sum().c_str()); idx+=topic_msg->getMD5Sum().length()+1;
          strcpy(buffer+idx,topic_msg->getDataType().c_str()); idx+=topic_msg->getDataType().length()+1;
          strcpy(buffer+idx,topic_msg->getMessageDefinition().c_str()); idx+=topic_msg->getMessageDefinition().length()+1;
          ros::serialization::OStream stream((uint8_t*)buffer+idx, topic_msg->size());
          topic_msg->write(stream); idx+=topic_msg->size();
          sndq.try_send(buffer, idx, 0 );
        }));
  }

  ros::Rate r(1000);

  while(ros::ok()){
    while(true){
      uint64_t rcv_size;
      unsigned priority;
      if(!rcvq.try_receive(buffer, datasize, rcv_size, priority)) break;
      int idx=0;
      std::string topicName=buffer+idx; idx+=topicName.length()+1;
      std::string MD5Sum=buffer+idx; idx+=MD5Sum.length()+1;
      std::string DataType=buffer+idx; idx+=DataType.length()+1;
      std::string MessageDefinition=buffer+idx; idx+=MessageDefinition.length()+1;
      topic_tools::ShapeShifter shape_shifter;
      shape_shifter.morph(MD5Sum, DataType, MessageDefinition, "");
      ros::serialization::OStream ostream((uint8_t*)buffer+idx, rcv_size-idx);
      shape_shifter.read(ostream);
      if(pubMap.find(topicName)==pubMap.end()){
        std::cerr <<topicName <<std::endl;
        pubMap[topicName] = shape_shifter.advertise(nh, topicName, 1);
      }
      pubMap[topicName].publish(shape_shifter);
    }
    ros::spinOnce();
    r.sleep();
  }



  return 0;
}
