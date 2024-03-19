#pragma once
// #include "SPSCQueueOPT.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
// #include <sys/syscall.h>
// #include <unistd.h>
#include <atomic>
#include "tag_common.h"

template <class T, uint32_t CNT = 1 << 20>
class SPSCQueueOPT
{
  public:
    static_assert(CNT && !(CNT & (CNT - 1)), "CNT must be a power of 2");
    SPSCQueueOPT() = default;
    T *alloc()
    {
        if (free_write_cnt == 0) {
            uint32_t rd_idx = ((std::atomic<uint32_t> *)&read_idx)->load(std::memory_order_consume);
            free_write_cnt  = (rd_idx - write_idx + CNT - 1) % CNT;
            if (__builtin_expect(free_write_cnt == 0, 0))
                return nullptr;
        }
        return &blk[write_idx].data;
    }

    void push()
    {
        ((std::atomic<bool> *)&blk[write_idx].avail)->store(true, std::memory_order_release);
        write_idx = (write_idx + 1) % CNT;
        free_write_cnt--;
    }

    template <typename Writer>
    bool tryPush(Writer writer)
    {
        T *p = alloc();
        if (!p)
            return false;
        writer(p);
        push();
        return true;
    }

    template <typename Writer>
    void blockPush(Writer writer)
    {
        while (!tryPush(writer))
            ;
    }

    T *front()
    {
        auto &cur_blk = blk[read_idx];
        if (!((std::atomic<bool> *)&cur_blk.avail)->load(std::memory_order_acquire))
            return nullptr;
        return &cur_blk.data;
    }

    void pop()
    {
        blk[read_idx].avail = false;
        ((std::atomic<uint32_t> *)&read_idx)->store((read_idx + 1) % CNT, std::memory_order_release);
    }

    template <typename Reader>
    bool tryPop(Reader reader)
    {
        T *v = front();
        if (!v)
            return false;
        reader(v);
        pop();
        return true;
    }

  private:
    struct alignas(64) Block
    {
        bool avail = false; // avail will be updated by both write and read thread
        T    data;
    } blk[CNT] = {};

    alignas(128) uint32_t write_idx = 0; // used only by writing thread
    uint32_t free_write_cnt         = CNT - 1;

    alignas(128) uint32_t read_idx = 0;
};
class staging_buffer
{
  public:
    staging_buffer() : varq_()
    {
        // uint32_t tid = static_cast<pid_t>(syscall(SYS_gettid));
        snprintf(name_, sizeof(name_), "%d", 1);
    }
    ~staging_buffer()
    {
    }
    inline tag *alloc()
    {
        return varq_.alloc();
    }
    inline void finish()
    {
        varq_.push();
    }
    tag *front()
    {
        return varq_.front();
    }
    inline void pop()
    {
        varq_.pop();
    }
    void set_name(const char *name)
    {
        strncpy(name_, name, sizeof(name) - 1);
    }
    const char *get_name()
    {
        return name_;
    }

    bool check_can_delete()
    {
        return should_deallocate_;
    }

    void set_delete_flag()
    {
        should_deallocate_ = true;
    }

  private:
    SPSCQueueOPT<tag> varq_;
    char              name_[16] = {0};
    bool              should_deallocate_{false};
};