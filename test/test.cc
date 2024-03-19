#include "tag_tool.h"
#include <sys/time.h>
#include <thread>
#include <unistd.h>
inline static uint64_t get_current_nano_sec() {
  timespec ts;
  ::clock_gettime(CLOCK_REALTIME, &ts);
  return ts.tv_sec * 1000000000 + ts.tv_nsec;
}
int main() {
  tag_tool::get_tool().ensure_staging_buffer_allocated();
  tag_tool::get_tool().set_tag_file("./test.log");
  std::thread t([]() {
    std::cout << "start poll" << std::endl;
    while (1) {
      tag_tool::get_tool().poll();
      usleep(1);
    }
  });

  auto t1 = get_current_nano_sec();
  for (int i = 0; i < 100000; i++) {
    tag_tool::get_tool().do_tag(10, i);
  }
  auto t2 = get_current_nano_sec();
  std::cout << (t2 - t1) * 1.0 / 100000 << std::endl;
  sleep(1);
  t1 = get_current_nano_sec();
  for (int i = 0; i < 100000; i++) {
    tag_tool::get_tool().do_tag(10, i);
  }
  t2 = get_current_nano_sec();
  std::cout << (t2 - t1) * 1.0 / 100000 << std::endl;
  sleep(1);
  t1 = get_current_nano_sec();
  for (int i = 0; i < 100000; i++) {
    tag_tool::get_tool().do_tag(10, i);
  }
  t2 = get_current_nano_sec();
  std::cout << (t2 - t1) * 1.0 / 100000 << std::endl;
  t.join();
}