#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace esphome
{
    namespace samsung_ac
    {
        // All times in milliseconds. Comparisons use unsigned subtraction, so they
        // stay correct when millis() wraps around (~49 days).

        // The bus must have been quiet for this long before we start transmitting.
        static const uint32_t SEND_SILENCE_INTERVAL = 100;

        // How long to wait for an Ack before transmitting the same packet again.
        static const uint32_t SEND_RETRY_INTERVAL = 500;

        // Maximum number of transmissions per packet (first attempt + retries).
        static const uint8_t SEND_MAX_ATTEMPTS = 4;

        // A packet that was transmitted at least once is given up when it is older
        // than this (covers a busy bus delaying the retries).
        static const uint32_t SEND_TIMEOUT = 4000;

        // Upper bound for waiting packets, protects RAM if the bus never gets quiet.
        static const size_t SEND_QUEUE_MAX_SIZE = 16;

        struct OutgoingPacket
        {
            uint8_t id;
            std::vector<uint8_t> data;
            uint32_t queued_at;
            uint32_t last_sent_at;
            uint8_t attempts;
        };

        // Outgoing NASA packets that must be acknowledged by the receiver.
        //
        // Only the packet at the head of the queue is in flight: the next packet is
        // not transmitted before the current one was acknowledged (ack()) or given up.
        // Independent of ESPHome so it can be unit tested with simulated time.
        class SendQueue
        {
        public:
            // Queues a packet. Returns false (and discards the packet) if the queue is full.
            bool enqueue(uint8_t id, std::vector<uint8_t> &&data, uint32_t now);

            // Removes the in-flight packet if its id matches. Returns true if removed.
            bool ack(uint8_t id);

            // Returns the data to transmit right now, or nullptr if nothing is due.
            // last_bus_activity is the time of the last byte received or sent.
            // Call only when no received bytes are waiting to be processed.
            // The returned pointer is valid until the next call on this queue.
            const std::vector<uint8_t> *poll(uint32_t now, uint32_t last_bus_activity);

            bool empty() const { return queue_.empty(); }
            size_t size() const { return queue_.size(); }

        protected:
            std::deque<OutgoingPacket> queue_;
        };

    } // namespace samsung_ac
} // namespace esphome
