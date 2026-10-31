#include "test_stuff.h"
#include "../components/samsung_ac/protocol_nasa.h"
#include "../components/samsung_ac/send_queue.h"
#include "../components/samsung_ac/samsung_ac_device_custClim.h"

using namespace std;
using namespace esphome::samsung_ac;

void test_nasa_1()
{
    cout << "test_nasa_1" << endl;
    Address to = Address::parse("20.00.00");
    Packet packet;

    packet = Packet::create(to, DataType::Request, MessageNumber::ENUM_in_operation_power, 1);
    packet.command.packetNumber = 1;
    std::cout << packet.to_string() << std::endl;
    assert_str(bytes_to_hex(packet.encode()), "32001180ff00200000c0130101400001c24734");
}

void test_nasa_2()
{
    cout << "test_nasa_2" << endl;
    auto data = hex_to_bytes("32001280ff00200002c013f201420101186e5434");
    Packet packetT;
    assert(packetT.decode(data) == DecodeResult::Ok);
    std::cout << packetT.to_string() << std::endl;

    Packet packet;
    packet = Packet::create(Address::parse("20.00.02"), DataType::Request, MessageNumber::VAR_in_temp_target_f, 28 * 10.0);
    packet.command.packetNumber = 242;
    std::cout << packet.to_string() << std::endl;
    assert_str(bytes_to_hex(packet.encode()), "32001280ff00200002c013f201420101186e5434");
}

// ---- SendQueue ----

void test_send_queue_send_retry_ack()
{
    cout << "test_send_queue_send_retry_ack" << endl;
    SendQueue q;
    const uint32_t t = 1000;
    assert(q.poll(t, 0) == nullptr); // empty queue

    assert(q.enqueue(5, {1, 2, 3}, t));

    // bus not quiet long enough
    assert(q.poll(t, t - SEND_SILENCE_INTERVAL + 1) == nullptr);

    // bus quiet -> first transmission
    const std::vector<uint8_t> *data = q.poll(t, t - SEND_SILENCE_INTERVAL);
    assert(data != nullptr);
    assert(data->size() == 3 && (*data)[0] == 1);

    // waiting for the Ack: no retransmission before the retry interval
    assert(q.poll(t + SEND_RETRY_INTERVAL - 1, 0) == nullptr);

    // Ack with a different id is ignored
    assert(!q.ack(6));
    assert(q.size() == 1);

    // no Ack -> retransmission
    assert(q.poll(t + SEND_RETRY_INTERVAL, 0) != nullptr);

    // Ack removes the packet
    assert(q.ack(5));
    assert(q.empty());
    assert(!q.ack(5));
}

void test_send_queue_one_packet_in_flight()
{
    cout << "test_send_queue_one_packet_in_flight" << endl;
    SendQueue q;
    const uint32_t t = 1000;
    q.enqueue(1, {0x01}, t);
    q.enqueue(2, {0x02}, t);

    const std::vector<uint8_t> *data = q.poll(t, 0);
    assert(data != nullptr && (*data)[0] == 0x01);

    // packet 2 must wait until packet 1 is acknowledged
    assert(q.poll(t + 100, 0) == nullptr);
    assert(!q.ack(2)); // only the in-flight packet can be acknowledged

    assert(q.ack(1));
    data = q.poll(t + 100, 0);
    assert(data != nullptr && (*data)[0] == 0x02);
}

void test_send_queue_gives_up_after_max_attempts()
{
    cout << "test_send_queue_gives_up_after_max_attempts" << endl;
    SendQueue q;
    const uint32_t t = 1000;
    q.enqueue(1, {0x01}, t);
    q.enqueue(2, {0x02}, t);

    uint32_t now = t;
    for (int attempt = 1; attempt <= SEND_MAX_ATTEMPTS; attempt++)
    {
        const std::vector<uint8_t> *data = q.poll(now, 0);
        assert(data != nullptr && (*data)[0] == 0x01);
        now += SEND_RETRY_INTERVAL;
    }

    // packet 1 is given up, packet 2 is sent in the same call
    const std::vector<uint8_t> *data = q.poll(now, 0);
    assert(data != nullptr && (*data)[0] == 0x02);
    assert(q.size() == 1);
}

void test_send_queue_timeout_on_busy_bus()
{
    cout << "test_send_queue_timeout_on_busy_bus" << endl;
    SendQueue q;
    const uint32_t t = 1000;
    q.enqueue(1, {0x01}, t);
    assert(q.poll(t, 0) != nullptr); // first attempt

    // bus stays busy, so no retry can happen; after SEND_TIMEOUT the packet is given up
    uint32_t now = t + SEND_TIMEOUT - 1;
    assert(q.poll(now, now) == nullptr);
    assert(q.size() == 1);

    now = t + SEND_TIMEOUT;
    assert(q.poll(now, now) == nullptr);
    assert(q.empty());
}

void test_send_queue_millis_wraparound()
{
    cout << "test_send_queue_millis_wraparound" << endl;
    SendQueue q;
    const uint32_t t = 0xFFFFFF00u; // ~256ms before millis() wraps
    q.enqueue(1, {0x01}, t);
    assert(q.poll(t, t - SEND_SILENCE_INTERVAL) != nullptr);

    // t + SEND_RETRY_INTERVAL wraps around to a small value
    assert(q.poll(t + SEND_RETRY_INTERVAL - 1, 0) == nullptr);
    assert(q.poll(t + SEND_RETRY_INTERVAL, t) != nullptr);
    assert(q.size() == 1);
}

void test_send_queue_size_limit()
{
    cout << "test_send_queue_size_limit" << endl;
    SendQueue q;
    for (size_t i = 0; i < SEND_QUEUE_MAX_SIZE; i++)
        assert(q.enqueue(i + 1, {0x01}, 0));
    assert(!q.enqueue(99, {0x01}, 0));
    assert(q.size() == SEND_QUEUE_MAX_SIZE);
}

// ---- NasaProtocol ----

Packet decode_published(DebugTarget &target)
{
    auto bytes = hex_to_bytes(target.last_publish_data);
    Packet packet;
    assert(packet.decode(bytes) == DecodeResult::Ok);
    assert(packet.command.packetNumber == target.last_publish_id);
    return packet;
}

void assert_message(const MessageSet &message, uint16_t number, long value)
{
    if ((uint16_t)message.messageNumber != number || message.value != value)
        cout << "actual: " << long_to_hex((uint16_t)message.messageNumber) << "=" << message.value
             << " expected: " << long_to_hex(number) << "=" << value << endl;
    assert((uint16_t)message.messageNumber == number);
    assert(message.value == value);
}

void test_nasa_requests_are_grouped()
{
    cout << "test_nasa_requests_are_grouped" << endl;
    DebugTarget target;
    Protocol *protocol = get_protocol("20.00.00");

    ProtocolRequest r1;
    r1.target_temp = 22;
    protocol->publish_request(&target, "20.00.00", r1);

    ProtocolRequest r2;
    r2.target_temp = 23; // replaces 22
    r2.fan_mode = FanMode::High;
    protocol->publish_request(&target, "20.00.00", r2);

    ProtocolRequest r3;
    r3.power = false;
    protocol->publish_request(&target, "20.00.01", r3);

    // nothing is sent before protocol_update()
    assert(target.publish_count == 0);

    protocol->protocol_update(&target);
    assert(target.publish_count == 2); // one packet per address
    assert(target.last_publish_id != 0);

    // std::map orders by address, so the last packet is the one for 20.00.01
    Packet packet = decode_published(target);
    assert(packet.da.to_string() == "20.00.01");
    assert(packet.messages.size() == 1);
    assert_message(packet.messages[0], 0x4000, 0);

    // nothing left to send
    protocol->protocol_update(&target);
    assert(target.publish_count == 2);

    // a single address results in exactly one packet with the merged values
    ProtocolRequest r4;
    r4.target_temp = 22;
    protocol->publish_request(&target, "20.00.00", r4);
    ProtocolRequest r5;
    r5.target_temp = 23;
    r5.fan_mode = FanMode::High;
    protocol->publish_request(&target, "20.00.00", r5);
    protocol->protocol_update(&target);
    assert(target.publish_count == 3);
    packet = decode_published(target);
    assert(packet.da.to_string() == "20.00.00");
    assert(packet.messages.size() == 2);
    assert_message(packet.messages[0], 0x4201, 230);
    assert_message(packet.messages[1], 0x4006, 3);
}

void test_nasa_mode_turns_power_on()
{
    cout << "test_nasa_mode_turns_power_on" << endl;
    DebugTarget target;
    Protocol *protocol = get_protocol("20.00.00");

    ProtocolRequest request;
    request.mode = Mode::Heat;
    protocol->publish_request(&target, "20.00.00", request);
    protocol->protocol_update(&target);

    Packet packet = decode_published(target);
    assert(packet.messages.size() == 2);
    assert_message(packet.messages[0], 0x4001, 4);
    assert_message(packet.messages[1], 0x4000, 1);
}

void test_nasa_custom_climate_request()
{
    cout << "test_nasa_custom_climate_request" << endl;
    DebugTarget target;
    Protocol *protocol = get_protocol("20.00.00");

    // Water heater as configured in example.yaml
    Samsung_AC_CustClim water_heater;
    water_heater.status = 0x4237;
    water_heater.set = 0x4235;
    water_heater.enable = 0x4065;
    water_heater.presAddr = 0x4066;
    water_heater.presToSend = 2;

    ProtocolRequest request;
    request.caller = &water_heater;
    request.power = true;
    request.target_temp = 50;
    protocol->publish_request(&target, "20.00.00", request);
    assert(water_heater.presToSend == -1); // consumed when the request is queued

    // a regular climate request for the same unit ends up in the same packet
    ProtocolRequest regular;
    regular.target_temp = 21;
    protocol->publish_request(&target, "20.00.00", regular);

    protocol->protocol_update(&target);
    assert(target.publish_count == 1);

    Packet packet = decode_published(target);
    assert(packet.messages.size() == 4);
    assert_message(packet.messages[0], 0x4066, 2);   // preset
    assert_message(packet.messages[1], 0x4065, 1);   // enable
    assert_message(packet.messages[2], 0x4235, 500); // water heater target
    assert_message(packet.messages[3], 0x4201, 210); // regular target temp
}

void test_nasa_packet_number_is_never_zero()
{
    cout << "test_nasa_packet_number_is_never_zero" << endl;
    std::set<int> seen;
    for (int i = 0; i < 600; i++)
    {
        Packet packet = Packet::createa_partial(Address::parse("20.00.00"), DataType::Request);
        assert(packet.command.packetNumber != 0);
        seen.insert(packet.command.packetNumber);
    }
    assert(seen.size() == 255); // 1..255 are all used
}

std::string build_ack(const std::string &from, const std::string &to, uint8_t packet_number)
{
    Packet ack;
    ack.sa = Address::parse(from);
    ack.da = Address::parse(to);
    ack.command.packetType = PacketType::Normal;
    ack.command.dataType = DataType::Ack;
    ack.command.packetNumber = packet_number;
    return bytes_to_hex(ack.encode());
}

void test_nasa_ack_is_forwarded()
{
    cout << "test_nasa_ack_is_forwarded" << endl;
    DebugTarget target;

    // Ack addressed to us (80.ff.00)
    test_process_data(build_ack("20.00.00", "80.ff.00", 42), target);
    assert(target.ack_count == 1);
    assert(target.last_ack_id == 42);

    // Ack for another controller on the bus must not acknowledge our packets
    test_process_data(build_ack("20.00.00", "62.00.00", 43), target);
    assert(target.ack_count == 1);
}

int main(int argc, char *argv[])
{
    test_nasa_1();
    test_nasa_2();

    test_send_queue_send_retry_ack();
    test_send_queue_one_packet_in_flight();
    test_send_queue_gives_up_after_max_attempts();
    test_send_queue_timeout_on_busy_bus();
    test_send_queue_millis_wraparound();
    test_send_queue_size_limit();

    test_nasa_requests_are_grouped();
    test_nasa_mode_turns_power_on();
    test_nasa_custom_climate_request();
    test_nasa_packet_number_is_never_zero();
    test_nasa_ack_is_forwarded();

    cout << "ALL NASA TESTS PASSED" << endl;
};
