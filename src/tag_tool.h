#pragma once
#include "file_helper.h"
#include "staging_buffer.h"
#include "tag_common.h"
#include "tscns.h"
#include <atomic>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <string.h>
#include <vector>
class tag_tool {
public:
  static tag_tool &get_tool() {
    static tag_tool logger;
    return logger;
  }
  ~tag_tool() { file_.flush(); }
  void set_tag_file(const char *filename) { file_.open(filename); }
  void do_tag(uint16_t tag_id, uint64_t guid, tag_context *context = nullptr) {
    auto header = alloc();
    if (!header) {
      fprintf(stderr, "queue_full...\n");
      return;
    }
    header->tag_id = tag_id;
    header->guid = guid;
    header->nano_sec = tscns_.rdns();
    header->context = context;
    finish();
  }
  void ensure_staging_buffer_allocated() {
    if (staging_buffer_ == nullptr) {
      std::unique_lock<std::mutex> guard(buffer_mutex_);
      guard.unlock();
      staging_buffer_ = new staging_buffer();
      sbc_.staging_buffer_created();
      guard.lock();
      thread_buffers_.push_back(staging_buffer_);
    }
  }
  void poll() { poll_inner(); }

private:
  tag_tool() { tscns_.init(); }

private:
  /////////////////////////////////////////////////////////////////

  tag *alloc() {
    if (staging_buffer_ == nullptr) {
      ensure_staging_buffer_allocated();
    }
    return staging_buffer_->alloc();
  }

  void finish() { staging_buffer_->finish(); }
  void adjust_heap(size_t i) {
    while (true) {
      size_t min_i = i;
      size_t ch = std::min(i * 2 + 1, bg_thread_buffers_.size());
      size_t end = std::min(ch + 2, bg_thread_buffers_.size());
      for (; ch < end; ch++) {
        auto h_ch = bg_thread_buffers_[ch].header;
        auto h_min = bg_thread_buffers_[min_i].header;
        if (h_ch &&
            (!h_min || *(int64_t *)(h_ch + 1) < *(int64_t *)(h_min + 1)))
          min_i = ch;
      }
      if (min_i == i)
        break;
      std::swap(bg_thread_buffers_[i], bg_thread_buffers_[min_i]);
      i = min_i;
    }
  }
  void poll_inner() {
    int64_t tsc = tscns_.rdtsc();
    if (thread_buffers_.size()) {
      std::unique_lock<std::mutex> lock(buffer_mutex_);
      for (auto tb : thread_buffers_) {
        bg_thread_buffers_.emplace_back(tb);
      }
      thread_buffers_.clear();
    }
    for (size_t i = 0; i < bg_thread_buffers_.size(); i++) {
      auto &node = bg_thread_buffers_[i];
      if (node.header)
        continue;
      node.header = node.tb->front();
      if (!node.header && node.tb->check_can_delete()) {
        delete node.tb;
        node = bg_thread_buffers_.back();
        bg_thread_buffers_.pop_back();
        i--;
      }
    }
    if (bg_thread_buffers_.empty())
      return;

    // build heap
    for (int i = bg_thread_buffers_.size() / 2; i >= 0; i--) {
      adjust_heap(i);
    }
    while (true) {
      auto h = bg_thread_buffers_[0].header;
      if (!h || *(int64_t *)(h + 1) >= tsc)
        break;
      auto tb = bg_thread_buffers_[0].tb;
      if (file_.is_open()) {
        static char buf[2048];
        int len = snprintf(buf, sizeof(buf), "%ld %ld %d\n", h->nano_sec,
                           h->guid, h->tag_id);
        file_.write(buf, len);
        file_.flush();
      }
      tb->pop();
      bg_thread_buffers_[0].header = tb->front();
      adjust_heap(0);
    }
  }

private:
  class staging_buffer_destroyer {
  public:
    explicit staging_buffer_destroyer() {}
    void staging_buffer_created() {}
    virtual ~staging_buffer_destroyer() {
      if (tag_tool::staging_buffer_ != nullptr) {
        staging_buffer_->set_delete_flag();
        tag_tool::staging_buffer_ = nullptr;
      }
    }
    friend class tag_tool;
  };

private:
  struct heap_node {
    heap_node(staging_buffer *buffer) : tb(buffer) {}

    staging_buffer *tb;
    tag *header{nullptr};
  };
  std::vector<heap_node> bg_thread_buffers_;

private:
  inline static thread_local staging_buffer *staging_buffer_{nullptr};
  inline static thread_local staging_buffer_destroyer sbc_{};
  std::vector<staging_buffer *> thread_buffers_;
  std::mutex buffer_mutex_;
  TSCNS tscns_;
  file_helper file_;
};