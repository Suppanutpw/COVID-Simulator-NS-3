/*
checklist
 - แบ่งพ่อค้ากับลูกค้า (ได้แล้ว)
 - ส่ง broadcast ไปหาทุกคน (ได้แล้ว)
 - จับ event ว่ามีการรับ udp (ได้แล้ว)
 - สุ่มโอกาสการติดโควิด (ได้แล้ว) แก้ได้ทีี่ function receiveCOVID
 - ตกแต่ง UI (ได้แล้ว)


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
#define N 100 // จำนวนคนทั้งหมด
#define M 80 // จำนวนลูกค้า
#define X_BOX 100 // กว้างแนวนอน
#define Y_BOX 50 // กว้างแนวตั้ง
#define INFECTRAD 2 // ระยะห่างที่ปลอดภัย (จำลอง 2 เมตร)
#define INFECTCHANCE 1.5 // โอกาสติด 1.5%
#define NodeSide 1.0 // ขนาดของจุดใน netanim
// list คนที่อยากให้ติดเชื้อ เลือกค่าใน array ได้ 0 <= infected_list[i] < N
// โดยท่ีค่า 0 <= infected_list[i] < M จะเป็นลูกค้า
// โดยท่ีค่า M <= infected_list[i] < N จะเป็นพ่อค้า
// ex. N = 100, M = 80 ต้องการคนติดเชื้อเป็นลูกค้า 3 คน พ่อค้าอีก 2 คน
// ก็ใส่ {0, 1, 2, 80, 81} ลงไป
int infected_list[] = {0, 1, 2, 3, 4};

using namespace ns3;
using namespace std;

struct rgb {
  uint8_t r; 
  uint8_t g; 
  uint8_t b; 
};

struct rgb colors [] = {
  { 252, 44, 44 }, // ติดเชื้อ
  { 127, 255, 46 }, // ลูกค้า
  { 46, 131, 255 }  // พ่อค้า
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
    void setUDPClient(int people_id, Time startTime);
    void setMobility();
    bool receiveCOVID(
      Ptr<NetDevice> dst_device, 
      Ptr<const Packet> packet, 
      uint16_t protocol,
      const Address &src, 
      const Address &dst, 
      BridgeNetDevice::PacketType packetType);
    int getNodeIdFromAddress(const Address &src);
};

// สร้างคน 100 คน โดยแบ่งเป็นลูกค้า 80 พ่อค้า 20
People people(N, M);
AnimationInterface * pAnim = 0;
bool is_infected[N] = {false};

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

void People::setUDPClient(int people_id, Time startTime) { // เสมือนติดตั้งตัวแพร่เชื้อ (ให้เฉพาะคนติดโควิด)
  uint32_t packetSize = 1024;
  uint32_t maxPacketCount = 1000;
  Time interPacketInterval = MilliSeconds (50);
  UdpEchoClientHelper client (serverAddress, port);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  
  // ถ้าติดก็เปลี่ยนสีพร้อมลงตัวแพร่เชื้อ
  if (is_infected[people_id] == false) {
    is_infected[people_id] = true;
    apps = client.Install (node.Get (people_id));
    apps.Start (startTime);
    apps.Stop (Seconds (DURATION));
  }
}

void People::setMobility() {
  // การเคลื่อนไหวของลูกค้าที่เดินไป-มา
  mobility_move.SetMobilityModel ("ns3::GaussMarkovMobilityModel",
    "Bounds", BoxValue (Box (0, X_BOX, 0, Y_BOX, 0, 10)),
    "TimeStep", TimeValue (MilliSeconds (10)),
    "Alpha", DoubleValue (0.85),
    "MeanVelocity", StringValue ("ns3::UniformRandomVariable[Min=50|Max=100]"),
    "MeanDirection", StringValue ("ns3::UniformRandomVariable[Min=0|Max=6.283185307]"),
    "MeanPitch", StringValue ("ns3::UniformRandomVariable[Min=0.05|Max=0.05]"),
    "NormalVelocity", StringValue ("ns3::NormalRandomVariable[Mean=0.0|Variance=0.0|Bound=0.0]"),
    "NormalDirection", StringValue ("ns3::NormalRandomVariable[Mean=0.0|Variance=0.2|Bound=0.4]"),
    "NormalPitch", StringValue ("ns3::NormalRandomVariable[Mean=0.0|Variance=0.02|Bound=0.04]"));
  mobility_move.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
    "X", StringValue ("ns3::UniformRandomVariable[Min=0|Max=" + to_string(X_BOX) + "]"),
    "Y", StringValue ("ns3::UniformRandomVariable[Min=0|Max=" + to_string(Y_BOX) + "]"),
    "Z", StringValue ("ns3::UniformRandomVariable[Min=0|Max=10]"));

  // การเคลื่อนไหวของพ่อค้าที่ยืนเฝ้าร้านอย่างเดียว
  mobility_nomove.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
    "X", StringValue ("ns3::UniformRandomVariable[Min=0|Max=" + to_string(X_BOX) + "]"),
    "Y", StringValue ("ns3::UniformRandomVariable[Min=0|Max=" + to_string(Y_BOX) + "]"),
    "Z", StringValue ("ns3::UniformRandomVariable[Min=0|Max=10]"));

  for (int i = 0; i < customer_count; i++) {
    mobility_move.Install (node.Get (i)); 
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
  if (packet->GetSize() > 1000) {
    // คำนวณโอกาสติด
    // เอา pos จาก device ปลายทาง
    // Packet::EnablePrinting();
    // packet->Print(cout);

    // ดึง pos (x,y) ของต้นทางและปลายทางมาคำนวณระยะห่าง ยิ่งใกล้ยิ่งติดง่าย
    Ptr<MobilityModel> dst_mob = dst_device->GetNode()->GetObject<MobilityModel>();
    double dst_x = dst_mob->GetPosition().x;
    double dst_y = dst_mob->GetPosition().y;

    Ptr<Node> src_node = node.Get(getNodeIdFromAddress(src));
    Ptr<MobilityModel> src_mob = src_node->GetObject<MobilityModel>();
    double src_x = src_mob->GetPosition().x;
    double src_y = src_mob->GetPosition().y;

    double distance = sqrt(pow(dst_x-src_x, 2) + pow(dst_y-src_y, 2));
    if (distance < INFECTRAD) {
      double chance = (1 - (distance/INFECTRAD))*INFECTCHANCE; // โอกาสติดโควิด
      double random = ((double)rand() / RAND_MAX)*100;

      // เช็คว่าตัวเลขที่สุ่มจะเป็นเลขติดโควิดหรือไม่
      if (random <= chance) {
        int dst_node_id = getNodeIdFromAddress(dst);
        people.setUDPClient(dst_node_id, Simulator::Now());
        pAnim->UpdateNodeColor (dst_device->GetNode(), colors[0].r, colors[0].g, colors[0].b);
        // printf("source (%lf,%lf) -> dest (%lf,%lf) = %lf \n", src_x, src_y, dst_x, dst_y, distance);
      }

    }
  }
  
  return true;
}

int People::getNodeIdFromAddress(const Address &address) {
  // เอา mac address มาหาว่าเป็น node ไหนเพื่อไปดึง pos(x, y)
  int found = 0;
  for (int i = 0; i < node.GetN(); i++) {
    if (operator == (address, node.Get(i)->GetDevice(1)->GetAddress())) {
      found = i;
      break;
    }
  }
  return found;
}

int main (int argc, char *argv[]) {
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // setCSMA -> DataRate, Delay, Mtu
  people.setCSMA(5000000, 0, 1400);

  // setIPV4 -> ip, netmask
  people.setIPV4("10.10.100.0", "255.255.255.0");

  // แยกพ่อค้ากับลูกค้า (เคลื่อนไหวได้กับไม่ได้)
  people.setMobility();

  // ตั้ง udp client ตัวแพร่เชื้อ -> คนที่ติดเชื้อ 
  int arr_size = sizeof(infected_list)/sizeof(infected_list[0]);
  for (int i = 0; i < arr_size; i++) {
    people.setUDPClient(infected_list[i], Seconds(0.0));
  }

  Simulator::Schedule (Seconds (0.00),
    [] ()
    {
      for (int i = 0; i < M; i++) {
        pAnim->UpdateNodeSize (i, NodeSide, NodeSide);
        pAnim->UpdateNodeColor (people.node.Get(i), colors[1].r, colors[1].g, colors[1].b);
      } for (int i = M; i < N; i++) {
        pAnim->UpdateNodeSize (i, NodeSide, NodeSide);
        pAnim->UpdateNodeColor (people.node.Get(i), colors[2].r, colors[2].g, colors[2].b);
      }

      int arr_size = sizeof(infected_list)/sizeof(infected_list[0]);
      for (int i = 0; i < arr_size; i++) {
        pAnim->UpdateNodeColor (people.node.Get(infected_list[i]), colors[0].r, colors[0].g, colors[0].b);
      }
    });
  
  // จบ Simulation
  Simulator::Stop (Seconds (DURATION));

  // ไฟล์ NetAnimation
  pAnim = new AnimationInterface ("covid-model.xml");
  // pAnim->EnablePacketMetadata (true); // แสดงประเภท packet บนลูกศร
  pAnim->SetMaxPktsPerTraceFile (1000000);

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
