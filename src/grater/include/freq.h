#pragma once

#include <TimeTagger.h>
#include <cstddef>
#include <unordered_map>
#include <vector>

using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i32 = int32_t;
using i64 = int64_t;

using f64 = double;

enum class Size { Small, Medium, Large };

constexpr std::size_t capacity(Size s) {
    switch (s) {
    case Size::Small:
        return 1 << 8;
    case Size::Medium:
        return 1 << 12;
    case Size::Large:
        return 1 << 16;
    }

    return 0;
}

inline constexpr channel_t CHANNEL_OFFSET = 18;

class PatternCounter : public IteratorBase {
  public:
    PatternCounter(
        TimeTaggerBase *tagger,
        std::vector<channel_t> &channels,
        u64 coincidence_window,
        Size buffer_size = Size::Small,
        int max_length = 2
    );
    ~PatternCounter();

    std::vector<std::pair<std::vector<channel_t>, i64>> get_data();

  protected:
    bool next_impl(std::vector<Tag> &incoming_tags, timestamp_t begin_time, timestamp_t end_time)
        override;

    void on_start() override;

  private:
    std::vector<Tag> _buffer;
    std::size_t _mask;
    std::size_t _head = 0;
    std::size_t _tail = 0;
    u64 _coincidence_window;
    std::unordered_set<channel_t> _channels;
    std::unordered_map<i64, std::vector<channel_t>> _reversed_channel_map;
    std::unordered_map<i64, i64> _data;
};
