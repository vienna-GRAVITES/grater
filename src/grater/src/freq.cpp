#include "freq.h"
#include <TimeTagger.h>
#include <functional>
#include <span>
#include <vector>

void combinations(
    std::span<const channel_t> channels,
    int k,
    std::unordered_map<i64, std::vector<channel_t>> &reversed_channel_map,
    std::unordered_map<i64, i64> &data
) {
    std::vector<channel_t> current;
    current.reserve(k);

    std::function<void(size_t)> rec;
    rec = [&](size_t start) {
        // Emit the current subset at every step (sizes 1..k), not only at size k.
        if (!current.empty()) {
            i64 b = 0;
            for (channel_t ch : current) {
                b |= 1LL << (ch + CHANNEL_OFFSET);
            }
            reversed_channel_map.emplace(b, current);
            data.emplace(b, 0);
        }

        // Stop recursing once we've reached the max pattern length.
        if (static_cast<int>(current.size()) == k) {
            return;
        }

        for (size_t i = start; i < channels.size(); ++i) {
            current.push_back(channels[i]);
            rec(i + 1);
            current.pop_back();
        }
    };
    rec(0);
}

PatternCounter::PatternCounter(
    TimeTaggerBase *tagger,
    std::vector<channel_t> &channels,
    u64 coincidence_window,
    Size buffer_size,
    int max_length
)
    : IteratorBase(tagger), _buffer(capacity(buffer_size)), _mask(capacity(buffer_size) - 1),
      _coincidence_window(coincidence_window), _channels(channels.begin(), channels.end()) {
    combinations(channels, max_length, _reversed_channel_map, _data);

    for (channel_t ch : channels) {
        registerChannel(ch);
    }

    finishInitialization();
}

std::vector<std::pair<std::vector<channel_t>, i64>> PatternCounter::get_data() {

    std::vector<std::pair<std::vector<channel_t>, i64>> result;
    result.reserve(_reversed_channel_map.size());

    for (const auto &[mask, channels] : _reversed_channel_map) {
        auto it = _data.find(mask);
        if (it == _data.end()) {
            continue;
        }
        result.emplace_back(channels, it->second);
    }

    return result;
}

void PatternCounter::on_start() {}

PatternCounter::~PatternCounter() { stop(); }

bool PatternCounter::next_impl(
    std::vector<Tag> &incoming_tags,
    timestamp_t begin_time,
    timestamp_t end_time
) {

    for (const Tag &tag : incoming_tags) {

        if (tag.type != Tag::Type::TimeTag) {
            continue;
        }

        // TODO: maybe replace this with a bitset operation
        if (!_channels.contains(tag.channel)) {
            continue;
        }

        _buffer[_head] = tag;

        auto oldest = _buffer[_tail];
        auto newest = _buffer[_head];

        if (newest.time - oldest.time > _coincidence_window) {
            i64 bit_map = 0;

            // section of buffer to scan over
            auto section_length = (_head - _tail) & _mask;
            for (auto i = 0; i < section_length; i++) {
                auto current_tag = _buffer[(_tail + i) % section_length];
                // accumulate channels into bitmap
                bit_map |= (1LL << (current_tag.channel + CHANNEL_OFFSET));
            }

            // increment counter
            _data[bit_map] += 1;

            // wind up the tail so we don't double count
            while (newest.time - _buffer[_tail].time > _coincidence_window) {
                _tail = (_tail + 1) & _mask;
            }
        }

        // increment head always
        _head = (_head + 1) & _mask;
    }

    return false;
}
