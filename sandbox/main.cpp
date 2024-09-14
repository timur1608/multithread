#include <openssl/sha.h>
#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <signal.h>
#include <stdio.h>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;
using namespace rapidjson;

std::vector<std::vector<std::string>> array;
std::mutex mut;
auto num_threads = std::thread::hardware_concurrency();
std::vector<std::jthread> threads(num_threads);

void init() {
  logging::formatter formatter =
      expr::stream << expr::format_date_time<boost::posix_time::ptime>(
                          "TimeStamp", "%Y-%m-%d %H:%M:%S")
                   << ": <" << logging::trivial::severity << "> "
                   << expr::smessage;
  typedef sinks::synchronous_sink<sinks::text_file_backend> f_sink;
  boost::shared_ptr<f_sink> file = logging::add_file_log(
      keywords::file_name = "sample_%N.log", /*< file name pattern >*/
      keywords::rotation_size =
          10 * 1024 * 1024, /*< rotate files every 10 MiB... >*/
      keywords::format = formatter, keywords::auto_flush = true);

  typedef sinks::synchronous_sink<sinks::text_ostream_backend> c_sink;
  boost::shared_ptr<c_sink> console =
      logging::add_console_log(std::cout, keywords::format = formatter);
  console->set_formatter(formatter);
  logging::core::get()->set_filter(logging::trivial::severity >=
                                   logging::trivial::trace);
}

std::atomic<int> flag = 1;

void my_handler(int s) {
  Document d;
  std::ofstream ofs("output.json");
  OStreamWrapper osw(ofs);
  auto& a = d.GetAllocator();
  d.SetArray();
  Value v;

  // v["timestamp"].SetString(array[0][2].c_str(), array[0][2].length());
  // v["hash"].SetString("hello", 5);
  // v["data"].SetString("Hello", 5);

  // v["timestamp"].SetString(array[0][2].c_str(), array[0][2].length());
  // v["hash"].SetString("hello", 5);
  // v["data"].SetString("Hello", 5);
  // v["timestamp"].SetString(array[1][2].c_str(), array[1][2].length());
  // v["hash"].SetString(array[1][1].c_str(), array[1][1].length());
  // v["data"].SetString(array[1][0].c_str(), array[1][0].length());
  // d.PushBack(v, d.GetAllocator());

  std::unique_lock<std::mutex>(mut);
  for (auto i : array) {
    v.SetObject();
    v.AddMember("timestamp", 0, a);
    v.AddMember("hash", 0, a);
    v.AddMember("data", 0, a);
    v["timestamp"].SetString(i[2].c_str(), i[2].length(), a);
    v["hash"].SetString(i[1].c_str(), i[1].length(), a);
    v["data"].SetString(i[0].c_str(), i[0].length(), a);
    d.PushBack(v, a);
  }
  // d.PushBack(v, a);
  std::cout << "SIGINT" << std::endl;
  flag = 0;
  Writer<OStreamWrapper> writer(osw);
  d.Accept(writer);
  for (auto& entry : threads) {
    if (entry.joinable()) {
      entry.join();
    }
  }
  exit(1);
}

void getListOfRandomBytes() {
  std::stringstream ss;
  auto start_time = std::chrono::steady_clock::now();
  std::vector<uint8_t> hash(32);
  std::vector<unsigned char> data_vector;
  while (flag == 1) {
    auto n = rand();
    auto copy = n;
    data_vector.clear();
    while (n > 0) {
      data_vector.push_back("0123456789"[n % 10]);
      n /= 10;
    }
    std::reverse(data_vector.begin(), data_vector.end());
    SHA256(data_vector.data(), data_vector.size() * sizeof(unsigned char),
           hash.data());
    std::string result;
    //for (auto i : hash) {
      //ss << std::hex << (int)i;
    //}
    //ss >> result;
    //ss.clear();
    //BOOST_LOG_TRIVIAL(trace) << "thread_id: " << std::this_thread::get_id() << " input: " << copy << " result: " << result;
    //result.clear();
    if (hash[30] == 0 && hash[31] == 0 && hash[29] == 0 && hash[28] == 0) {
      std::unique_lock<std::mutex>(mut);
      for (auto i : hash) {
        ss << std::hex << (int)i;
      }
      ss >> result;
      ss.clear();
      BOOST_LOG_TRIVIAL(info) << result;
      array.push_back(
          {std::to_string(copy), result,
           std::to_string(
               std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count())});
    }
    // std::cout << std::this_thread::get_id() << "\n";
  }
}

int main() {
  std::srand(std::time(nullptr));
  auto start_time = std::chrono::steady_clock::now();

  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = my_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);
  init();
  logging::add_common_attributes();

  for (size_t i = 0; i < num_threads; ++i) {
    threads[i] = std::jthread(getListOfRandomBytes);
  }
  for (auto& entry : threads) {
    entry.join();
  }
  // pause();
  return 0;
}
