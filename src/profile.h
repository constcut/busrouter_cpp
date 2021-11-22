#ifndef TABLE_PROFILE
#define TABLE_PROFILE

#include <chrono>
#include <iostream>
#include <string>

#include<unordered_map>
#include<map>

class LogDuration {
public:
  explicit LogDuration(const std::string& msg = "")
    : message(msg + ": ")
    , start(std::chrono::steady_clock::now())
  {
  }

  ~LogDuration() {
    auto finish = std::chrono::steady_clock::now();
    auto dur = finish - start;
    std::cerr << message
      << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count()
      << " ms" << std::endl;
  }

  auto getNow() {
    auto finish = std::chrono::steady_clock::now();
    auto dur = finish - start;
    return std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
  }

private:
  std::string message;
  std::chrono::steady_clock::time_point start;
};

using TimeQuant = std::chrono::nanoseconds;
using Moment = std::chrono::steady_clock::time_point;

inline std::map<std::string, std::pair<TimeQuant, size_t>> globalTimeMap;
inline std::map<std::string, Moment> lastMoment;


inline void cleanProfile() {
  globalTimeMap.clear();
}


inline void startProfile(std::string name) {
  auto t = std::chrono::steady_clock::now();
  lastMoment[name] = t;
}

//Check profile


inline void stopProfile(std::string name) {
  auto dur = std::chrono::steady_clock::now() - lastMoment[name];
  globalTimeMap[name].first += std::chrono::duration_cast<std::chrono::nanoseconds>(dur);
  ++globalTimeMap[name].second;
}

inline void printProfile() {
  for (const auto& [key, value]: globalTimeMap) {
    std::cerr << "__" << key << " used " << value.second 
      << " times, total time: " << (value.first.count()/1000000.0) << std::endl;
  }
}


class LogProfile {
public:
  explicit LogProfile(const std::string& msg = "")
    : message(msg)
  {
    startProfile(msg);
  }

  ~LogProfile() {
    stopProfile(message);
  }

private:
  std::string message;
};


#define UNIQ_ID_IMPL(lineno) ___local___##lineno
#define UNIQ_ID(lineno) UNIQ_ID_IMPL(lineno)

#define LOG_DURATION(message) \
  LogDuration UNIQ_ID(__LINE__){message};

#define LOG_PROFILE(message) \
  LogProfile UNIQ_ID(__LINE__){message};


#endif