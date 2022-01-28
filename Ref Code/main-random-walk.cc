#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#define DURATION 30

using namespace ns3;

static void 
CourseChange (std::string context, Ptr<const MobilityModel> mobility)
{
  Vector pos = mobility->GetPosition ();
  Vector vel = mobility->GetVelocity ();
  std::cout << Simulator::Now () << ", model=" << mobility << ", POS: x=" << pos.x << ", y=" << pos.y
            << ", z=" << pos.z << "; VEL:" << vel.x << ", y=" << vel.y
            << ", z=" << vel.z << std::endl;
}

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // พ่อค้า/แม่ค้า
  NodeContainer merchants;
  merchants.Create (20);

  // ลูกค้า
  NodeContainer customers;
  customers.Create (80);

  // Class ให้เคลื่อนไหวได้ โดยใส่ให้ลูกค้าที่เดินไป-มา
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("100.0"),
                                 "Y", StringValue ("100.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=90]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=3.0]"),
                             "Bounds", StringValue ("0|200|0|200"));
  mobility.Install (customers); // ใส่ให้ node ลูกค้า
  Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange",
                   MakeCallback (&CourseChange));

  Simulator::Stop (Seconds (DURATION));

  // ไฟล์ NetAnimation
  AnimationInterface anim ("first.xml");

  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}
