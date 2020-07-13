// Copyright 2020, University of Freiburg
// Authors: Axel Lehmann <lehmann@cs.uni-freiburg.de>.

#include "osm2ttl/ttl/Writer.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include "boost/geometry.hpp"
#include "osmium/geom/factory.hpp"
#include "osmium/osm/area.hpp"
#include "osmium/osm/item_type.hpp"
#include "osmium/osm/location.hpp"
#include "osmium/osm/node.hpp"
#include "osmium/osm/node_ref.hpp"
#include "osmium/osm/relation.hpp"
#include "osmium/osm/tag.hpp"
#include "osmium/osm/way.hpp"

#include "osm2ttl/config/Config.h"

#include "osm2ttl/geometry/Location.h"

#include "osm2ttl/osm/Area.h"
#include "osm2ttl/osm/Box.h"
#include "osm2ttl/osm/Node.h"
#include "osm2ttl/osm/Tag.h"
#include "osm2ttl/osm/TagList.h"
#include "osm2ttl/osm/WKTFactory.h"

#include "osm2ttl/ttl/BlankNode.h"
#include "osm2ttl/ttl/IRI.h"
#include "osm2ttl/ttl/Literal.h"

// ____________________________________________________________________________
osm2ttl::ttl::Writer::Writer(const osm2ttl::config::Config& config)
  : _config(config), _queue(_config.writerThreads) {
  _out = &std::cout;
  _factory = osm2ttl::osm::WKTFactory::create(_config);
}

// ____________________________________________________________________________
osm2ttl::ttl::Writer::~Writer() {
  close();
  delete _factory;
}

// ____________________________________________________________________________
bool osm2ttl::ttl::Writer::open() {
  if (!_config.output.empty()) {
    _outFile.open(_config.output);
    _out = &_outFile;
    return _outFile.is_open();
  }
  return true;
}

// ____________________________________________________________________________
void osm2ttl::ttl::Writer::close() {
  _queue.quit();
  if (_outFile.is_open()) {
    _outFile.close();
  }
}

// ____________________________________________________________________________
bool osm2ttl::ttl::Writer::contains(std::string_view s,
                                    std::string_view n) {
  if (n.empty()) {
    return true;
  }
  if (s.size() < n.size()) {
    return false;
  }
  return (s.find(n) != std::string::npos);
}

// ____________________________________________________________________________
bool osm2ttl::ttl::Writer::endsWith(std::string_view s,
                                    std::string_view n) {
  if (n.empty()) {
    return true;
  }
  if (s.size() < n.size()) {
    return false;
  }
  return (s.find(n, s.size() - n.size()) != std::string::npos);
}

// ____________________________________________________________________________
bool osm2ttl::ttl::Writer::startsWith(std::string_view s,
                                      std::string_view n) {
  if (n.empty()) {
    return true;
  }
  if (s.size() < n.size()) {
    return false;
  }
  return (s.rfind(n, 0) != std::string::npos);
}

// ____________________________________________________________________________
void osm2ttl::ttl::Writer::writeHeader() {
  const std::lock_guard<std::mutex> lock(_outMutex);
  *_out << _config.outputFormat.header();
}

// ____________________________________________________________________________
template<typename S, typename O>
void osm2ttl::ttl::Writer::writeTriple(const S& s, const osm2ttl::ttl::IRI& p,
                                     const O& o) {
  static_assert(std::is_same<S, osm2ttl::ttl::BlankNode>::value
                || std::is_same<S, osm2ttl::ttl::IRI>::value);
  static_assert(std::is_same<O, osm2ttl::ttl::BlankNode>::value
                || std::is_same<O, osm2ttl::ttl::IRI>::value
                || std::is_same<O, osm2ttl::ttl::Literal>::value);
  _queue.dispatch([this, s, p, o]{
    const std::lock_guard<std::mutex> lock(_outMutex);
    *_out << _config.outputFormat.format(s);
    *_out << " ";
    *_out << _config.outputFormat.format(p);
    *_out << " ";
    *_out << _config.outputFormat.format(o);
    *_out << " .\n";
  });
}

// ____________________________________________________________________________
void osm2ttl::ttl::Writer::writeArea(const osm2ttl::osm::Area& area) {
  osm2ttl::ttl::IRI s{area.fromWay()?"osmway":"osmrel",
      std::to_string(area.objId())};

  std::ostringstream tmp;
  tmp << boost::geometry::wkt(area.geom());
  writeTriple(s,
    osm2ttl::ttl::IRI("geo", "hasGeometry"),
    osm2ttl::ttl::Literal(std::move(tmp.str()),
                          osm2ttl::ttl::IRI("geo", "wktLiteral")));

  if (_config.addEnvelope) {
    writeBox(s, osm2ttl::ttl::IRI("osm", "envelope"), area.envelope());
  }
}

// ____________________________________________________________________________
template<typename S>
void osm2ttl::ttl::Writer::writeOsmiumBox(const S& s,
                                     const osm2ttl::ttl::IRI& p,
                                     const osmium::Box& box) {
  static_assert(std::is_same<S, osm2ttl::ttl::BlankNode>::value
                || std::is_same<S, osm2ttl::ttl::IRI>::value);
  writeTriple(s, p, osm2ttl::ttl::Literal(box));
}

// ____________________________________________________________________________
template<typename S>
void osm2ttl::ttl::Writer::writeBox(const S& s,
                                    const osm2ttl::ttl::IRI& p,
                                    const osm2ttl::osm::Box& box) {
  static_assert(std::is_same<S, osm2ttl::ttl::BlankNode>::value
                || std::is_same<S, osm2ttl::ttl::IRI>::value);
  std::ostringstream tmp;
  tmp << boost::geometry::wkt(box.geom());
  writeTriple(s, p, osm2ttl::ttl::Literal(tmp.str()));
}

// ____________________________________________________________________________
void osm2ttl::ttl::Writer::writeNode(const osm2ttl::osm::Node& node) {
  osm2ttl::ttl::IRI s{"osmnode", node};

  writeTriple(s,
    osm2ttl::ttl::IRI("rdf", "type"),
    osm2ttl::ttl::IRI("osm", "node"));

  std::ostringstream tmp;
  tmp << boost::geometry::wkt(node.geom());
  writeTriple(s,
    osm2ttl::ttl::IRI("geo", "hasGeometry"),
    osm2ttl::ttl::Literal(std::move(tmp.str()),
                          osm2ttl::ttl::IRI("geo", "wktLiteral")));

  writeTagList(s, node.tags());
}

// ____________________________________________________________________________
void osm2ttl::ttl::Writer::writeOsmiumRelation(
  const osmium::Relation& relation) {
  osm2ttl::ttl::IRI s{"osmrel", relation};

  writeTriple(s,
    osm2ttl::ttl::IRI("rdf", "type"),
    osm2ttl::ttl::IRI("osm", "relation"));

  writeOsmiumTagList(s, relation.tags());
  writeOsmiumRelationMembers(s, relation.members());
}

// ____________________________________________________________________________
template<typename S>
void osm2ttl::ttl::Writer::writeOsmiumRelationMembers(
    const S& s,
    const osmium::RelationMemberList& members) {
  static_assert(std::is_same<S, osm2ttl::ttl::BlankNode>::value
                || std::is_same<S, osm2ttl::ttl::IRI>::value);
  // If only basic data is requested, skip this.

  std::uint32_t i = 0;
  for (const osmium::RelationMember& member : members) {
    std::string role{member.role()};
    if (!role.empty() && role != "outer" && role != "inner") {
      writeTriple(s,
        osm2ttl::ttl::IRI("osmrel", role),
        osm2ttl::ttl::IRI("osm"
          + std::string((osmium::item_type_to_name(
            member.type()) == std::string("relation")) ? "rel" : "way"),
          member));
    }
    if (!_config.expandedData) {
      continue;
    }
    if (role.empty()) {
      role = "member";
    }
    osm2ttl::ttl::BlankNode b;
    writeTriple(s,
      osm2ttl::ttl::IRI("osmrel", "membership"),
      b);

    writeTriple(b,
      osm2ttl::ttl::IRI("osmrel", role),
      osm2ttl::ttl::IRI("osm"
        + std::string(osmium::item_type_to_name(member.type())),
        member));

    writeTriple(b,
      osm2ttl::ttl::IRI("osmm", "pos"),
      osm2ttl::ttl::Literal(std::to_string(++i),
                          osm2ttl::ttl::IRI("xsd", "integer")));
  }
}

// ____________________________________________________________________________
template<typename S>
void osm2ttl::ttl::Writer::writeOsmiumTag(const S& s,
                                     const osmium::Tag& tag) {
  static_assert(std::is_same<S, osm2ttl::ttl::BlankNode>::value
                || std::is_same<S, osm2ttl::ttl::IRI>::value);
  // No spaces allowed in tag keys (see 002.problem.nt)
  std::string key = std::string(tag.key());
  std::string tmp;
  tmp.reserve(key.size());
  for (size_t pos = 0; pos < key.size(); ++pos) {
    switch (key[pos]) {
      case ' ':
        tmp += "_";
        break;
      default:
        tmp += key[pos];
    }
  }
  auto tagType = _config.tagKeyType.find(tag.key());
  if (tagType != _config.tagKeyType.end()) {
    writeTriple(s,
      osm2ttl::ttl::IRI("osmt", tmp),
      osm2ttl::ttl::Literal(tag.value(), tagType->second));
  } else {
    writeTriple(s,
      osm2ttl::ttl::IRI("osmt", tmp),
      osm2ttl::ttl::Literal(tag.value()));
  }
}

// ____________________________________________________________________________
template<typename S>
void osm2ttl::ttl::Writer::writeTag(const S& s, const osm2ttl::osm::Tag& tag) {
  static_assert(std::is_same<S, osm2ttl::ttl::BlankNode>::value
                || std::is_same<S, osm2ttl::ttl::IRI>::value);
  const std::string &key = tag.first;
  const std::string &value = tag.second;
  auto tagType = _config.tagKeyType.find(key);
  if (tagType != _config.tagKeyType.end()) {
    writeTriple(s,
      osm2ttl::ttl::IRI("osmt", key),
      osm2ttl::ttl::Literal(value, tagType->second));
  } else {
    writeTriple(s,
      osm2ttl::ttl::IRI("osmt", key),
      osm2ttl::ttl::Literal(value));
  }
}

// ____________________________________________________________________________
template<typename S>
void osm2ttl::ttl::Writer::writeOsmiumTagList(const S& s,
                                         const osmium::TagList& tags) {
  static_assert(std::is_same<S, osm2ttl::ttl::BlankNode>::value
                || std::is_same<S, osm2ttl::ttl::IRI>::value);
  for (const osmium::Tag& tag : tags) {
    writeOsmiumTag(s, tag);
    if (!_config.skipWikiLinks) {
      if (std::string(tag.key()) == "wikidata") {
        std::string value{tag.value()};
        // Only take first wikidata entry if ; is found
        auto end = value.find(';');
        if (end != std::string::npos) {
          value = value.erase(end);
        }
        // Remove all but Q and digits to ensuder Qdddddd format
        value.erase(remove_if(value.begin(), value.end(), [](char c) {
          return (!isdigit(c) && c != 'Q');
        }), value.end());

        writeTriple(s,
          osm2ttl::ttl::IRI("osm", tag.key()),
          osm2ttl::ttl::IRI("wd", value));
      }
      if (Writer::endsWith(tag.key(), "wikipedia") &&
          !Writer::contains(tag.key(), "fixme")) {
        std::string v = tag.value();
        auto pos = v.find(':');
        if (pos != std::string::npos) {
          std::string lang = v.substr(0, pos);
          std::string entry = v.substr(pos + 1);
          writeTriple(s,
            osm2ttl::ttl::IRI("osm", "wikipedia"),
            osm2ttl::ttl::IRI("https://"+lang+".wikipedia.org/wiki/", entry));
        } else {
          writeTriple(s,
            osm2ttl::ttl::IRI("osm", "wikipedia"),
            osm2ttl::ttl::IRI("https://www.wikipedia.org/wiki/", v));
        }
      }
    }
  }
}

// ____________________________________________________________________________
template<typename S>
void osm2ttl::ttl::Writer::writeTagList(const S& s,
                                        const osm2ttl::osm::TagList& tags) {
  static_assert(std::is_same<S, osm2ttl::ttl::BlankNode>::value
                || std::is_same<S, osm2ttl::ttl::IRI>::value);
  for (const osm2ttl::osm::Tag& tag : tags) {
    writeTag(s, tag);
    const std::string &key = tag.first;
    std::string value = tag.second;
    if (!_config.skipWikiLinks) {
      if (key == "wikidata") {
        // Only take first wikidata entry if ; is found
        auto end = value.find(';');
        if (end != std::string::npos) {
          value = value.erase(end);
        }
        // Remove all but Q and digits to ensuder Qdddddd format
        value.erase(remove_if(value.begin(), value.end(), [](char c) {
          return (!isdigit(c) && c != 'Q');
        }), value.end());

        writeTriple(s,
          osm2ttl::ttl::IRI("osm", key),
          osm2ttl::ttl::IRI("wd", value));
      }
      if (key == "wikipedia") {
        auto pos = value.find(':');
        if (pos != std::string::npos) {
          std::string lang = value.substr(0, pos);
          std::string entry = value.substr(pos + 1);
          writeTriple(s,
            osm2ttl::ttl::IRI("osm", "wikipedia"),
            osm2ttl::ttl::IRI("https://"+lang+".wikipedia.org/wiki/", entry));
        } else {
          writeTriple(s,
            osm2ttl::ttl::IRI("osm", "wikipedia"),
            osm2ttl::ttl::IRI("https://www.wikipedia.org/wiki/", value));
        }
      }
    }
  }
}

// ____________________________________________________________________________
void osm2ttl::ttl::Writer::writeOsmiumWay(const osmium::Way& way) {
  osm2ttl::ttl::IRI s{"osmway", way};

  writeTriple(s,
    osm2ttl::ttl::IRI("rdf", "type"),
    osm2ttl::ttl::IRI("osm", "way"));

  writeOsmiumTagList(s, way.tags());
  writeOsmiumWayNodeList(s, way.nodes());

  // Count unique points, this only checks direct duplicates.
  size_t numUniquePoints = 0;
  osmium::Location last;
  for (auto const *it = way.nodes().cbegin(); it != way.nodes().cend(); ++it) {
    if (last != it->location()) {
      last = it->location();
      numUniquePoints++;
    }
  }

  // Select geometry object in relation to the number of unique points.
  if (numUniquePoints > 3 && way.is_closed()) {
    writeTriple(s,
      osm2ttl::ttl::IRI("geo", "hasGeometry"),
      osm2ttl::ttl::Literal(_factory->create_polygon(way),
        osm2ttl::ttl::IRI("geo", "wktLiteral")));
  } else if (numUniquePoints > 1) {
    writeTriple(s,
      osm2ttl::ttl::IRI("geo", "hasGeometry"),
      osm2ttl::ttl::Literal(_factory->create_linestring(way),
        osm2ttl::ttl::IRI("geo", "wktLiteral")));
  } else {
    writeTriple(s,
      osm2ttl::ttl::IRI("geo", "hasGeometry"),
      osm2ttl::ttl::Literal(
        _factory->create_point(way.nodes()[0]),
        osm2ttl::ttl::IRI("geo", "wktLiteral")));
  }

  if (_config.metaData) {
    writeTriple(s,
      osm2ttl::ttl::IRI("osmway", "is_closed"),
      osm2ttl::ttl::Literal(way.is_closed()?"yes":"no"));
    writeTriple(s,
      osm2ttl::ttl::IRI("osmway", "nodeCount"),
      osm2ttl::ttl::Literal(std::to_string(way.nodes().size())));
    writeTriple(s,
      osm2ttl::ttl::IRI("osmway", "uniqueNodeCount"),
      osm2ttl::ttl::Literal(std::to_string(numUniquePoints)));
  }

  if (_config.addEnvelope) {
    writeOsmiumBox(s, osm2ttl::ttl::IRI("osm", "envelope"), way.envelope());
  }
}

// ____________________________________________________________________________
template<typename S>
void osm2ttl::ttl::Writer::writeOsmiumWayNodeList(const S& s,
                                             const osmium::WayNodeList& nodes) {
  static_assert(std::is_same<S, osm2ttl::ttl::BlankNode>::value
                || std::is_same<S, osm2ttl::ttl::IRI>::value);
  // If only basic data is requested, skip this.
  if (!_config.expandedData) {
    return;
  }

  uint32_t i = 0;
  for (const osmium::NodeRef& nodeRef : nodes) {
    osm2ttl::ttl::BlankNode b;
    writeTriple(s, osm2ttl::ttl::IRI("osmway", "node"), b);

    writeTriple(b,
      osm2ttl::ttl::IRI("osmway", "node"),
      osm2ttl::ttl::IRI("osmnode", nodeRef));

    writeTriple(b,
      osm2ttl::ttl::IRI("osmm", "pos"),
      osm2ttl::ttl::Literal(std::to_string(++i),
        osm2ttl::ttl::IRI("xsd", "integer")));
  }
}
