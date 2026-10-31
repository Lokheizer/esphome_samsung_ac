#include "esphome/core/log.h"
#include "send_queue.h"
#include "util.h"

namespace esphome
{
    namespace samsung_ac
    {
        bool SendQueue::enqueue(uint8_t id, std::vector<uint8_t> &&data, uint32_t now)
        {
            if (queue_.size() >= SEND_QUEUE_MAX_SIZE)
            {
                ESP_LOGE(TAG, "Send queue full (%u packets), discarding packet %u", (unsigned)queue_.size(), id);
                return false;
            }

            OutgoingPacket packet;
            packet.id = id;
            packet.data = std::move(data);
            packet.queued_at = now;
            packet.last_sent_at = 0;
            packet.attempts = 0;
            queue_.push_back(std::move(packet));
            return true;
        }

        bool SendQueue::ack(uint8_t id)
        {
            if (queue_.empty() || queue_.front().id != id)
                return false;

            queue_.pop_front();
            return true;
        }

        const std::vector<uint8_t> *SendQueue::poll(uint32_t now, uint32_t last_bus_activity)
        {
            while (!queue_.empty())
            {
                OutgoingPacket &packet = queue_.front();

                if (packet.attempts > 0)
                {
                    // Still waiting for the Ack of the last transmission
                    if (now - packet.last_sent_at < SEND_RETRY_INTERVAL)
                        return nullptr;

                    if (packet.attempts >= SEND_MAX_ATTEMPTS || now - packet.queued_at >= SEND_TIMEOUT)
                    {
                        ESP_LOGE(TAG, "No Ack for packet %u after %u attempts, giving up", packet.id, packet.attempts);
                        queue_.pop_front();
                        continue;
                    }
                }

                if (now - last_bus_activity < SEND_SILENCE_INTERVAL)
                    return nullptr;

                if (packet.attempts > 0)
                {
                    ESP_LOGW(TAG, "No Ack for packet %u, sending again (attempt %u)", packet.id, packet.attempts + 1);
                }

                packet.attempts++;
                packet.last_sent_at = now;
                return &packet.data;
            }
            return nullptr;
        }

    } // namespace samsung_ac
} // namespace esphome
