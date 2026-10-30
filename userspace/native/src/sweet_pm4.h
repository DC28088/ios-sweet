/* SPDX-License-Identifier: MIT */
/*
 * Checked PM4 emission for the A6xx command processor.
 *
 * A packet is a 32-bit header followed by its payload. Type 4 addresses
 * registers (18-bit register space, payload 1..127 dwords); type 7 is an
 * opcode (1..16383 dwords). This builder owns no memory except its vector and
 * refuses malformed counts instead of masking them, because a masked count is
 * a command that does something other than what its caller wrote.
 *
 * The kernel appends the completion fence to every submission, so there is no
 * fence packet here; CP_MEM_WRITE and friends are for the caller's own data.
 */
#ifndef SWEET_PM4_H
#define SWEET_PM4_H

#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <vector>

namespace sweet {

enum {
    /* adreno_pm4.xml */
    CP_NOP = 0x10,
    CP_MEM_WRITE = 0x3d,
    CP_EVENT_WRITE = 0x46,
    CP_WHERE_AM_I = 0x62,
    CP_WAIT_FOR_IDLE = 0x26,
    CP_WAIT_MEM_WRITES = 0x33,
    CP_SET_MARKER = 0x0d,
};

/* Event codes for CP_EVENT_WRITE. */
enum {
    CACHE_FLUSH_TS = 4,
    END_OF_PIPE_SYNC = 0x2a,
};

class Pm4 {
public:
    Pm4() = default;
    explicit Pm4(size_t reserve_words) { words_.reserve(reserve_words); }

    void clear() { words_.clear(); }
    size_t size() const { return words_.size(); }
    const std::vector<uint32_t> &words() const { return words_; }
    std::vector<uint32_t> &words() { return words_; }

    /* Type 4: register write(s). `reg` must be 4-byte aligned and inside the
     * 18-bit register space (the A6xx packet header carries reg >> 2 in 16
     * bits after the type). Returns false without emitting anything if the
     * count or the register cannot be encoded. */
    bool type4(uint32_t reg, std::initializer_list<uint32_t> values)
    {
        const size_t count = values.size();
        if (count < 1 || count > 127 || (reg & 3) || reg > 0x3ffff)
            return false;
        words_.push_back((4u << 28) | (uint32_t)((count - 1) << 16) | (reg >> 2));
        words_.insert(words_.end(), values.begin(), values.end());
        return true;
    }

    /* Type 7: opcode with 0..16383 payload dwords. */
    bool type7(uint32_t opcode, std::initializer_list<uint32_t> values)
    {
        const size_t count = values.size();
        if (count > 0x3fff || opcode > 0x7f)
            return false;
        words_.push_back((7u << 28) | (uint32_t)((count ? count - 1 : 0) << 16) |
                         (opcode & 0x7f));
        words_.insert(words_.end(), values.begin(), values.end());
        return true;
    }

    bool nop(unsigned payload_words)
    {
        if (payload_words > 0x3fff)
            return false;
        /* CP_NOP's payload is ignored; a one-word packet is the common form. */
        words_.push_back((7u << 28) |
                         (uint32_t)((payload_words ? payload_words - 1 : 0) << 16) |
                         CP_NOP);
        words_.insert(words_.end(), payload_words, 0u);
        return true;
    }

    /* CP_MEM_WRITE: 64-bit GPU address followed by one or more dwords. */
    bool mem_write(uint64_t address, std::initializer_list<uint32_t> values)
    {
        const size_t count = values.size();
        if (count < 1 || count > 0x3ffd)
            return false;
        words_.push_back((7u << 28) | (uint32_t)((count + 2 - 1) << 16) | CP_MEM_WRITE);
        words_.push_back((uint32_t)address);
        words_.push_back((uint32_t)(address >> 32));
        words_.insert(words_.end(), values.begin(), values.end());
        return true;
    }

    /* CP_EVENT_WRITE with a destination: {event, address lo, address hi, value}.
     * `event` is ORed with the caller's flags (e.g. bit 31 for an interrupt). */
    bool event_write(uint32_t event, uint64_t address, uint32_t value)
    {
        words_.push_back((7u << 28) | (3u << 16) | CP_EVENT_WRITE);
        words_.push_back(event);
        words_.push_back((uint32_t)address);
        words_.push_back((uint32_t)(address >> 32));
        words_.push_back(value);
        return true;
    }

    bool wait_for_idle()
    {
        words_.push_back((7u << 28) | CP_WAIT_FOR_IDLE);
        return true;
    }

private:
    std::vector<uint32_t> words_;
};

}  // namespace sweet

#endif /* SWEET_PM4_H */
