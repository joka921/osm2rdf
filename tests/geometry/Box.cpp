// Copyright 2020, University of Freiburg
// Authors: Axel Lehmann <lehmann@cs.uni-freiburg.de>.

#include "osm2ttl/geometry/Box.h"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include "gtest/gtest.h"

namespace osm2ttl::geometry {

osm2ttl::geometry::Box getDefaultObject() { return osm2ttl::geometry::Box(); }

osm2ttl::geometry::Box getFilledObject() {
  osm2ttl::geometry::Box obj;
  obj.min_corner() = Location{50, 50};
  obj.max_corner() = Location{200, 200};
  return obj;
}

TEST(GEOMETRY_Box, equalsOperator) {
  osm2ttl::geometry::Box o1;
  o1.min_corner() = Location{50, 50};
  o1.max_corner() = Location{200, 200};

  osm2ttl::geometry::Box o2;
  o2.min_corner() = Location{50, 200};
  o2.max_corner() = Location{200, 200};

  osm2ttl::geometry::Box o3;
  o3.min_corner() = Location{200, 200};
  o3.max_corner() = Location{50, 50};

  ASSERT_TRUE(o1 == o1);
  ASSERT_FALSE(o1 == o2);
  ASSERT_FALSE(o1 == o3);

  ASSERT_FALSE(o2 == o1);
  ASSERT_TRUE(o2 == o2);
  ASSERT_FALSE(o2 == o3);

  ASSERT_FALSE(o3 == o1);
  ASSERT_FALSE(o3 == o2);
  ASSERT_TRUE(o3 == o3);
}

TEST(GEOMETRY_Box, notEqualsOperator) {
  osm2ttl::geometry::Box o1;
  o1.min_corner() = Location{50, 50};
  o1.max_corner() = Location{200, 200};

  osm2ttl::geometry::Box o2;
  o2.min_corner() = Location{50, 200};
  o2.max_corner() = Location{200, 200};

  osm2ttl::geometry::Box o3;
  o3.min_corner() = Location{200, 200};
  o3.max_corner() = Location{50, 50};

  ASSERT_FALSE(o1 != o1);
  ASSERT_TRUE(o1 != o2);
  ASSERT_TRUE(o1 != o3);

  ASSERT_TRUE(o2 != o1);
  ASSERT_FALSE(o2 != o2);
  ASSERT_TRUE(o2 != o3);

  ASSERT_TRUE(o3 != o1);
  ASSERT_TRUE(o3 != o2);
  ASSERT_FALSE(o3 != o3);
}

TEST(GEOMETRY_Box, serializationBinary) {
  std::stringstream buffer;

  osm2ttl::geometry::Box origDefaultObject = getDefaultObject();
  osm2ttl::geometry::Box origFilledObject = getFilledObject();
  osm2ttl::geometry::Box loadedDefaultObject;
  osm2ttl::geometry::Box loadedFilledObject;

  // Store and load
  boost::archive::binary_oarchive oa(buffer);
  oa << origDefaultObject;
  oa << origFilledObject;
  // std::cerr << buffer.str() << std::endl;
  boost::archive::binary_iarchive ia(buffer);
  ia >> loadedDefaultObject;
  ia >> loadedFilledObject;

  // Compare
  ASSERT_TRUE(origDefaultObject == loadedDefaultObject);
  ASSERT_TRUE(origFilledObject == loadedFilledObject);
}

TEST(GEOMETRY_Box, serializationText) {
  std::stringstream buffer;

  osm2ttl::geometry::Box origDefaultObject = getDefaultObject();
  osm2ttl::geometry::Box origFilledObject = getFilledObject();
  osm2ttl::geometry::Box loadedDefaultObject;
  osm2ttl::geometry::Box loadedFilledObject;

  // Store and load
  boost::archive::text_oarchive oa(buffer);
  oa << origDefaultObject;
  oa << origFilledObject;
  // std::cerr << buffer.str() << std::endl;
  boost::archive::text_iarchive ia(buffer);
  ia >> loadedDefaultObject;
  ia >> loadedFilledObject;

  // Compare
  ASSERT_TRUE(origDefaultObject == loadedDefaultObject);
  ASSERT_TRUE(origFilledObject == loadedFilledObject);
}

}  // namespace osm2ttl::geometry