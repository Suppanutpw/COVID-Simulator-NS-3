/*
checklist
 - แบ่งพ่อค้ากับลูกค้า (ได้แล้ว)
 - ส่ง broadcast ไปหาทุกคน (ได้แล้ว)
 - จับ event ว่ามีการรับ udp (ได้แล้ว)
 - สุ่มโอกาสการติดโควิด (ยังไม่ทำ) แก้ได้ทีี่ function receiveCOVID
 - ยังไม่ติดโควิดสีแดง ถ้าติดจะเป็นสีฟ้า (ขัดใจแป๊ป) แก้หลังบรรทัด 246 ได้


ใช้ mobility ในการเคลื่อนไหว
ใช้ udp ส่งข้อมูล
*/

#include <ostream>
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#define DURATION 10.0 // เวลาจำลอง 10 วินาที

using namespace ns3;
using namespace std;

struct rgb {
  uint8_t r; 
  uint8_t g; 
  uint8_t b; 
};

struct rgb colors [] = {
  { 255, 0, 0 }, // Red
  { 0, 255, 0 }, // Green
  { 0, 0, 255 }  // Blue
};

class People {
  public:
    int people_count, customer_count;
    NodeContainer node;
    NetDeviceContainer device;
    Address serverAddress;
    Ipv4AddressHelper ipv4;
    InternetStackHelper stack;
    CsmaHelper csma;
    Ipv4InterfaceContainer interface;
    ApplicationContainer apps;
    MobilityHelper mobility_move, mobility_nomove;
    uint16_t port = 9;

    People(int people, int customer);
    void setCSMA(int dataRate, int delay, int mtu);
    void setIPV4(string address, string netmask);
    void setUDPServer();
    void setUDPClient();
    void setMobility();
    bool receiveCOVID(
      Ptr<NetDevice> dst_device, 
      Ptr<const Packet> packet, 
      uint16_t protocol,
      const Address &src, 
      const Address &dst, 
      BridgeNetDevice::PacketType packetType);
    Ptr<Node> getNodeFromAddress(const Address &src);
};

// สร้างคน 100 คน โดยแบ่งเป็นลูกค้า 80 พ่อค้า 20
People people(100, 80);
AnimationInterface * pAnim = 0;

static void 
CourseChange (string context, Ptr<const MobilityModel> mobility)
{
  Simulator::Now();
  /* 
  Vector pos = mobility->GetPosition ();
  Vector vel = mobility->GetVelocity ();
  cout << Simulator::Now () << ", model=" << mobility << ", POS: x=" << pos.x << ", y=" << pos.y
            << ", z=" << pos.z << "; VEL:" << vel.x << ", y=" << vel.y
            << ", z=" << vel.z << endl;
  */
}

People::People(int people, int customer) {
  people_count = people;
  customer_count = customer;

  // สร้าง node จำนวน people โหนด
  node.Create (people_count);

  // ติดตั้ง internet stack
  stack.Install (node);
}

void People::setCSMA(int dataRate, int delay, int mtu) {
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (delay)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (mtu));
  device = csma.Install (node);

  // ตั้งค่า handler กรณีมี packet เข้ามา
  NetDeviceContainer::Iterator i;
  for (i = device.Begin (); i != device.End (); ++i) {
    (*i)->SetPromiscReceiveCallback (MakeCallback (&People::receiveCOVID, this));  // some NetDevice method
  }
}

void People::setIPV4(string address, string netmask) {
  ipv4.SetBase (Ipv4Address(address.c_str()), Ipv4Mask(netmask.c_str()));
  interface = ipv4.Assign (device);
  serverAddress = Address(interface.GetAddress (1));
}

// เราไม่สนใจ udp เราสนตอนที่ node ส่ง arp เป็น broadcast
void People::setUDPServer() {
  UdpEchoServerHelper server (port); // อย่าลืม
  apps = server.Install (node.Get (1));
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (DURATION));
}

void People::setUDPClient() {
  uint32_t packetSize = 1024;
  uint32_t maxPacketCount = 1000;
  Time interPacketInterval = Seconds (1.0);
  UdpEchoClientHelper client (serverAddress, port);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  apps = client.Install (node);
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (DURATION));
}

void People::setMobility() {
  // การเคลื่อนไหวของลูกค้าที่เดินไป-มา
  mobility_move.SetMobilityModel ("ns3::GaussMarkovMobilityModel",
    "Bounds", BoxValue (Box (0, 200, 0, 200, 0, 10)),
    "TimeStep", TimeValue (MilliSeconds (10)),
    "Alpha", DoubleValue (0.85),
    "MeanVelocity", StringValue ("ns3::UniformRandomVariable[Min=50|Max=100]"),
    "MeanDirection", StringValue ("ns3::UniformRandomVariable[Min=0|Max=6.283185307]"),
    "MeanPitch", StringValue ("ns3::UniformRandomVariable[Min=0.05|Max=0.05]"),
    "NormalVelocity", StringValue ("ns3::NormalRandomVariable[Mean=0.0|Variance=0.0|Bound=0.0]"),
    "NormalDirection", StringValue ("ns3::NormalRandomVariable[Mean=0.0|Variance=0.2|Bound=0.4]"),
    "NormalPitch", StringValue ("ns3::NormalRandomVariable[Mean=0.0|Variance=0.02|Bound=0.04]"));
  mobility_move.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
    "X", StringValue ("ns3::UniformRandomVariable[Min=0|Max=200]"),
    "Y", StringValue ("ns3::UniformRandomVariable[Min=0|Max=200]"),
    "Z", StringValue ("ns3::UniformRandomVariable[Min=0|Max=10]"));

  // การเคลื่อนไหวของพ่อค้าที่ยืนเฝ้าร้านอย่างเดียว
  mobility_nomove.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
    "X", StringValue ("ns3::UniformRandomVariable[Min=0|Max=200]"),
    "Y", StringValue ("ns3::UniformRandomVariable[Min=0|Max=200]"),
    "Z", StringValue ("ns3::UniformRandomVariable[Min=0|Max=10]"));

  for (int i = 0; i < customer_count; i++) {
    mobility_move.Install (node.Get (i)); 
    Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange",
                     MakeCallback (&CourseChange));
  } for (int i = customer_count; i < people_count; i++) {
    mobility_nomove.Install (node.Get (i));
  }
}

// ถ้าได้รับ packet จะมาทำ function นี้ ใช้คำนวณโอกาสติดได้
bool People::receiveCOVID (
    Ptr<NetDevice> dst_device, 
    Ptr<const Packet> packet, 
    uint16_t protocol,
    const Address &src, 
    const Address &dst, 
    BridgeNetDevice::PacketType packetType) {

  // เช็คดูว่าเป็น UDP ไหม (ดูขนาด packet) เพื่อกรอง arp ออก
  Packet::EnablePrinting();
  if (packet->GetSize() > 1000) {
    // คำนวณโอกาสติด
    // เอา pos จาก device ปลายทาง
    // packet->Print(cout);

    /*
    // ดึง pos (x,y) ของต้นทางและปลายทางมาคำนวณระยะห่าง ยิ่งใกล้ยิ่งติดง่าย
    Ptr<MobilityModel> dst_mob = dst_device->GetNode()->GetObject<MobilityModel>();
    double dst_x = dst_mob->GetPosition().x;
    double dst_y = dst_mob->GetPosition().y;

    Ptr<Node> src_node = getNodeFromAddress(src);
    Ptr<MobilityModel> src_mob = src_node->GetObject<MobilityModel>();
    double src_x = src_mob->GetPosition().x;
    double src_y = src_mob->GetPosition().y;
    */

    // printf("\nsource (%lf,%lf) -> dest (%lf,%lf)\n\n",src_x,src_y,dst_x,dst_y);

    // ถ้าติดก็เปลี่ยนสี
    pAnim->UpdateNodeColor (dst_device->GetNode(), colors[2].r, colors[2].g, colors[2].b);

  }
  
  return true;
}

Ptr<Node> People::getNodeFromAddress(const Address &address) {
  // เอา mac address มาหาว่าเป็น node ไหนเพื่อไปดึง pos(x, y)
  int found = 0;
  for (uint32_t i = 0; i < node.GetN(); i++) {
    if (operator == (address, node.Get(i)->GetDevice(1)->GetAddress())) {
      found = i;
      break;
    }
  }
  return node.Get(found);
}

int main (int argc, char *argv[]) {
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // setCSMA -> DataRate, Delay, Mtu
  people.setCSMA(5000000, 2, 1400);

  // setIPV4 -> ip, netmask
  people.setIPV4("10.10.100.0", "255.255.255.0");

  // แยกพ่อค้ากับลูกค้า (เคลื่อนไหวได้กับไม่ได้)
  people.setMobility();

  // ตั้ง udp Server
  people.setUDPServer();

  // ตั้ง udp client
  people.setUDPClient();

  // จบ Simulation
  Simulator::Stop (Seconds (DURATION));

  // ไฟล์ NetAnimation
  pAnim = new AnimationInterface ("covid-model.xml");
  pAnim->EnablePacketMetadata (true); // แสดงประเภท packet บนลูกศร
  pAnim->SetMaxPktsPerTraceFile (1000000);

  // เปลี่ยนสี node
  // anim.UpdateNodeColor (people.node.Get(0), 0, 0, 255);

  // เปลี่ยนขนาด node (node-id, กว้าง, ยาว)
  // anim.UpdateNodeSize (5, 3.0, 3.0);

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
