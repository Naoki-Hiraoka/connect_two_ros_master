#include <ros/ros.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <topic_tools/shape_shifter.h>
#include <unordered_map>
#include <string>
#include <sys/ipc.h>

#include <boost/program_options.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

struct Data {
  char topicName[100];
  char md5sum[100];
  char datatype[100];
  char definition[100];
  uint8_t data[1000000];
  int size;
};

void process(int argc, char** argv, std::string this_uri, std::string snd_name, std::string rcv_name, std::vector<std::string>& topics, double rate) {
  std::string ros_master_uri_str = "ROS_MASTER_URI=" + this_uri;
  putenv( const_cast<char*>(ros_master_uri_str.c_str()));
  ros::init(argc, argv, "connect_two_master");

  boost::interprocess::message_queue::remove((ros::this_node::getName()+snd_name).c_str());
  sleep(1);
  ///// get shm
  boost::interprocess::message_queue sndq(boost::interprocess::open_or_create,
                                          (ros::this_node::getName()+snd_name).c_str(),
                                          10,
                                          sizeof(Data));
  boost::interprocess::message_queue rcvq(boost::interprocess::open_or_create,
                                          (ros::this_node::getName()+rcv_name).c_str(),
                                          10,
                                          sizeof(Data));

  ros::NodeHandle nh;
  Data data;
  std::vector<ros::Subscriber> subs;
  std::unordered_map<std::string, ros::Publisher> pubMap;

  for(int i=0;i<topics.size();i++){
    subs.push_back(nh.subscribe<topic_tools::ShapeShifter>(topics[i], 1, [&](const topic_tools::ShapeShifter::ConstPtr& topic_msg){
          // トピック情報の取得
          std::cerr << 1<<std::endl;
          if(topics[i].size() > sizeof(data.topicName)) {
            std::cerr << "topics[i].size()" << topics[i].size() << ">" << "sizeof(data.topicName)" << sizeof(data.topicName) << std::endl;
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
          std::cerr << 2<<std::endl;
          std::cerr <<data.topicName <<std::endl;
          sndq.try_send(&data, sizeof(Data), 0 );
                    std::cerr << 3<<std::endl;
        }));
  }

  ros::Rate r(rate);
  ROS_INFO_STREAM("[connect_two_master] start with " << ros_master_uri_str);

  while(ros::ok()){
    while(true){
                std::cerr << 4<<std::endl;
      uint64_t rcv_size;
      unsigned priority;
      if(!rcvq.try_receive(&data, sizeof(Data), rcv_size, priority)) break;
          std::cerr << 5<<std::endl;
      topic_tools::ShapeShifter shape_shifter;
      shape_shifter.morph(data.md5sum, data.datatype, data.definition, "");
      ros::serialization::OStream stream(data.data, data.size);
      shape_shifter.read(stream);
      if(pubMap.find(data.topicName)==pubMap.end()){
        std::cerr <<data.topicName <<std::endl;
        pubMap[data.topicName] = shape_shifter.advertise(nh, data.topicName, 1);
      }
      pubMap[data.topicName].publish(shape_shifter);
                std::cerr << 6<<std::endl;
    }
    ros::spinOnce();
    r.sleep();
  }

  exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {
  ///// parse options
  boost::program_options::options_description op("target_uri_info");
  op.add_options()
    ("help,h",                                                      "show help.")
    ("master_uri,m",boost::program_options::value<std::string>(),   "master hostname or ip.")
    ("slave_uri,s", boost::program_options::value<std::string>(),   "slave hostname or ip.")
    ("master_topics,mt", boost::program_options::value<std::vector<std::string>>(), "topics subscribed in master side")
    ("slave_topics,mt", boost::program_options::value<std::vector<std::string>>(), "topics subscribed in slave side")
    ;
  boost::program_options::variables_map argmap;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, op), argmap);
  boost::program_options::notify(argmap);
  std::string master_uri = "http://localhost:11311"; // default
  std::string slave_uri = "http://localhost:12345"; // default
  std::vector<std::string> master_topics;
  std::vector<std::string> slave_topics;
  if( argmap.count("help") ){         std::cerr << op << std::endl; return 1; }
  if( argmap.count("master_uri") ){   master_uri  = argmap["master_uri"].as<std::string>();   }
  if( argmap.count("slave_uri") ){    slave_uri   = argmap["slave_uri"].as<std::string>();    }
  if( argmap.count("master_topics") ){   master_topics  = argmap["master_topics"].as<std::vector<std::string> >();   }
  if( argmap.count("slave_topics") ){   slave_topics  = argmap["slave_topics"].as<std::vector<std::string> >();   }
  std::cerr << "master_uri is set as = "  << master_uri   << std::endl;
  std::cerr << "slave_uri is set as = "   << slave_uri    << std::endl;

  ///// start fork
  if(fork() == 0) process(argc, argv, slave_uri, "slave", "master",  slave_topics, 1000);
  process(argc, argv, master_uri, "master", "slave", master_topics, 1000);

  // exit
  for (int child_cnt = 0; child_cnt < 1; ++child_cnt) { wait(NULL); }
  return EXIT_SUCCESS;
}
