#include <ros/ros.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <topic_tools/shape_shifter.h>
#include <unordered_map>
#include <string>
#include <sys/ipc.h>

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

  // get shm
  boost::interprocess::message_queue sndq(boost::interprocess::open_or_create,
                                          (ros::this_node::getName()+(slaveside?"SLAVE":"MASTER")).c_str(),
                                          10,
                                          sizeof(Data));
  boost::interprocess::message_queue rcvq(boost::interprocess::open_or_create,
                                          (ros::this_node::getName()+(slaveside?"MASTER":"SLAVE")).c_str(),
                                          10,
                                          sizeof(Data));
  // clear shm. (大昔のデータが残っている場合がある))
  {
    Data data;
    uint64_t rcv_size;
    unsigned priority;
    while(rcvq.try_receive(&data, sizeof(Data), rcv_size, priority)) continue;
  }

  Data data;
  std::vector<ros::Subscriber> subs;
  std::unordered_map<std::string, ros::Publisher> pubMap;

  for(int i=0;i<topics.size();i++){
    subs.push_back(nh.subscribe<topic_tools::ShapeShifter>(topics[i], 1, [&,i](const topic_tools::ShapeShifter::ConstPtr& topic_msg){
          // トピック情報の取得
          if(topics[i].length()+1 > sizeof(data.topicName)) {
            std::cerr << "sizeof [" << topics[i] << "] >" << "sizeof(data.topicName)" << sizeof(data.topicName) << std::endl;
            exit(1);
          }
          strcpy(data.topicName, topics[i].c_str());
          if(topic_msg->getMD5Sum().size() > sizeof(data.md5sum)) {
            std::cerr << "topic_msg->getMD5Sum().size()" << topic_msg->getMD5Sum().size() << ">" << "sizeof(data.md5sum)" << sizeof(data.md5sum) << std::endl;
            exit(1);
          }
          strcpy(data.md5sum, topic_msg->getMD5Sum().c_str());
          if(topic_msg->getDataType().size() > sizeof(data.datatype)) {
            std::cerr << "topic_msg->getDataType().size()" << topic_msg->getDataType().size() << ">" << "sizeof(data.datatype)" << sizeof(data.datatype) << std::endl;
            exit(1);
          }
          strcpy(data.datatype, topic_msg->getDataType().c_str());
          if(topic_msg->getMessageDefinition().size() > sizeof(data.definition)) {
            std::cerr << "topic_msg->getMessageDefinition().size()" << topic_msg->getMessageDefinition().size() << ">" << "sizeof(data.definition)" << sizeof(data.definition) << std::endl;
            exit(1);
          }
          strcpy(data.definition, topic_msg->getMessageDefinition().c_str());
          data.size = topic_msg->size();
          if(topic_msg->size() > sizeof(data.data)){
            std::cerr << "topic_msg->size()" << topic_msg->size() << ">" << "sizeof(data.data)" << sizeof(data.data) << std::endl;
            exit(1);
          }
          ros::serialization::OStream stream(data.data, data.size);
          topic_msg->write(stream);
          sndq.try_send(&data, sizeof(Data), 0 );
        }));
  }

  ros::Rate r(1000);

  while(ros::ok()){
    while(true){
      uint64_t rcv_size;
      unsigned priority;
      if(!rcvq.try_receive(&data, sizeof(Data), rcv_size, priority)) break;
      topic_tools::ShapeShifter shape_shifter;
      shape_shifter.morph(data.md5sum, data.datatype, data.definition, "");
      ros::serialization::OStream stream(data.data, data.size);
      shape_shifter.read(stream);
      if(pubMap.find(data.topicName)==pubMap.end()){
        std::cerr <<data.topicName <<std::endl;
        pubMap[data.topicName] = shape_shifter.advertise(nh, data.topicName, 1);
      }
      pubMap[data.topicName].publish(shape_shifter);
    }
    ros::spinOnce();
    r.sleep();
  }



  return 0;
}
