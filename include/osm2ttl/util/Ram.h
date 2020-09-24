// Copyright 2020, University of Freiburg
// Authors: Axel Lehmann <lehmann@cs.uni-freiburg.de>.

#ifndef OSM2TTL_UTIL_RAM_H_
#define OSM2TTL_UTIL_RAM_H_

#include <unistd.h>

#include <cstdint>

namespace osm2ttl {
namespace util {
namespace ram {

constexpr int64_t KILO = 1024;
constexpr int64_t MEGA = KILO * KILO;
constexpr int64_t GIGA = KILO * MEGA;

int64_t available();
int64_t physPages();

}  // namespace ram
}  // namespace util
}  // namespace osm2ttl

#endif  // OSM2TTL_UTIL_RAM_H_
