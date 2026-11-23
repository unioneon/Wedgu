#pragma once
#include "environment.h"
#include <atomic>
#include <memory>
namespace location {
struct Job {
 std::wstring query,error;
 bool system=false;
 std::vector<environment::Place> places;
 std::atomic<bool> done{false};
};
bool SystemPlace(environment::Place&,std::wstring& error,bool requestPermission);
std::shared_ptr<Job> Start(const std::wstring& query,bool system);
}
