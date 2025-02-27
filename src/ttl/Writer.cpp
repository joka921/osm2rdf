// Copyright 2020, University of Freiburg
// Authors: Axel Lehmann <lehmann@cs.uni-freiburg.de>
//          Patrick Brosi <brosi@cs.uni-freiburg.de>.

// This file is part of osm2rdf.
//
// osm2rdf is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// osm2rdf is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with osm2rdf.  If not, see <https://www.gnu.org/licenses/>.

#include "osm2rdf/ttl/Writer.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "boost/iostreams/filter/bzip2.hpp"
#include "omp.h"
#include "osm2rdf/config/Config.h"
#include "osm2rdf/ttl/Constants.h"
#include "osmium/osm/item_type.hpp"

// ____________________________________________________________________________
template <typename T>
osm2rdf::ttl::Writer<T>::Writer(const osm2rdf::config::Config& config,
                                osm2rdf::util::Output* output)
    : _config(config), _out(output) {
  _prefixes = {
      // well-known prefixes
      {osm2rdf::ttl::constants::NAMESPACE__GEOSPARQL,
       "http://www.opengis.net/ont/geosparql#"},
      {osm2rdf::ttl::constants::NAMESPACE__WIKIDATA_ENTITY,
       "http://www.wikidata.org/entity/"},
      {osm2rdf::ttl::constants::NAMESPACE__XML_SCHEMA,
       "http://www.w3.org/2001/XMLSchema#"},
      {osm2rdf::ttl::constants::NAMESPACE__RDF,
       "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
      {osm2rdf::ttl::constants::NAMESPACE__OPENGIS,
       "http://www.opengis.net/rdf#"},
      // own prefix
      {osm2rdf::ttl::constants::NAMESPACE__OSM2RDF,
       "https://osm2rdf.cs.uni-freiburg.de/rdf#"},
      {osm2rdf::ttl::constants::NAMESPACE__OSM2RDF_GEOM,
       "https://osm2rdf.cs.uni-freiburg.de/rdf/geom#"},
      // osm prefixes
      {osm2rdf::ttl::constants::NAMESPACE__OSM,
       "https://www.openstreetmap.org/"},
      // https://wiki.openstreetmap.org/wiki/Sophox#How_OSM_data_is_stored
      // https://github.com/Sophox/sophox/blob/master/osm2rdf/osmutils.py#L35-L39
      {osm2rdf::ttl::constants::NAMESPACE__OSM_NODE,
       "https://www.openstreetmap.org/node/"},
      {osm2rdf::ttl::constants::NAMESPACE__OSM_RELATION,
       "https://www.openstreetmap.org/relation/"},
      {osm2rdf::ttl::constants::NAMESPACE__OSM_TAG,
       "https://www.openstreetmap.org/wiki/Key:"},
      {osm2rdf::ttl::constants::NAMESPACE__OSM_WAY,
       "https://www.openstreetmap.org/way/"}};

  // Generate constants
  osm2rdf::ttl::constants::IRI__GEOSPARQL__HAS_GEOMETRY =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__GEOSPARQL, "hasGeometry");
  osm2rdf::ttl::constants::IRI__GEOSPARQL__HAS_SERIALIZATION = generateIRI(
      osm2rdf::ttl::constants::NAMESPACE__GEOSPARQL, "hasSerialization");
  osm2rdf::ttl::constants::IRI__GEOSPARQL__AS_WKT =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__GEOSPARQL, "asWKT");
  osm2rdf::ttl::constants::IRI__GEOSPARQL__WKT_LITERAL =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__GEOSPARQL, "wktLiteral");
  osm2rdf::ttl::constants::IRI__OSM2RDF_CONTAINS_AREA =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__OSM2RDF, "contains_area");
  osm2rdf::ttl::constants::IRI__OSM2RDF_CONTAINS_NON_AREA = generateIRI(
      osm2rdf::ttl::constants::NAMESPACE__OSM2RDF, "contains_nonarea");
  osm2rdf::ttl::constants::IRI__OSM2RDF_INTERSECTS_AREA = generateIRI(
      osm2rdf::ttl::constants::NAMESPACE__OSM2RDF, "intersects_area");
  osm2rdf::ttl::constants::IRI__OSM2RDF_INTERSECTS_NON_AREA = generateIRI(
      osm2rdf::ttl::constants::NAMESPACE__OSM2RDF, "intersects_nonarea");
  osm2rdf::ttl::constants::IRI__OSM2RDF_GEOM__CONVEX_HULL = generateIRI(
      osm2rdf::ttl::constants::NAMESPACE__OSM2RDF_GEOM, "convex_hull");
  osm2rdf::ttl::constants::IRI__OSM2RDF_GEOM__ENVELOPE =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__OSM2RDF_GEOM, "envelope");
  osm2rdf::ttl::constants::IRI__OSM2RDF_GEOM__OBB =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__OSM2RDF_GEOM, "obb");
  osm2rdf::ttl::constants::IRI__OSM2RDF__POS =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__OSM2RDF, "pos");
  osm2rdf::ttl::constants::IRI__OSMWAY_IS_CLOSED =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__OSM_WAY, "is_closed");
  osm2rdf::ttl::constants::IRI__OSMWAY_NEXT_NODE =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__OSM_WAY, "next_node");
  osm2rdf::ttl::constants::IRI__OSMWAY_NEXT_NODE_DISTANCE = generateIRI(
      osm2rdf::ttl::constants::NAMESPACE__OSM_WAY, "next_node_distance");
  osm2rdf::ttl::constants::IRI__OSMWAY_NODE =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__OSM_WAY, "node");
  osm2rdf::ttl::constants::IRI__OSMWAY_NODE_COUNT =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__OSM_WAY, "nodeCount");
  osm2rdf::ttl::constants::IRI__OSMWAY_UNIQUE_NODE_COUNT = generateIRI(
      osm2rdf::ttl::constants::NAMESPACE__OSM_WAY, "uniqueNodeCount");
  osm2rdf::ttl::constants::IRI__OSM_NODE =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__OSM, "node");
  osm2rdf::ttl::constants::IRI__OSM_RELATION =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__OSM, "relation");
  osm2rdf::ttl::constants::IRI__OSM_TAG =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__OSM, "tag");
  osm2rdf::ttl::constants::IRI__OSM_WAY =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__OSM, "way");
  osm2rdf::ttl::constants::IRI__RDF_TYPE =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__RDF, "type");

  osm2rdf::ttl::constants::IRI__XSD_DECIMAL =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__XML_SCHEMA, "decimal");
  osm2rdf::ttl::constants::IRI__XSD_DOUBLE =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__XML_SCHEMA, "double");
  osm2rdf::ttl::constants::IRI__XSD_FLOAT =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__XML_SCHEMA, "float");
  osm2rdf::ttl::constants::IRI__XSD_INTEGER =
      generateIRI(osm2rdf::ttl::constants::NAMESPACE__XML_SCHEMA, "integer");

  osm2rdf::ttl::constants::LITERAL__NO = generateLiteral("no", "");
  osm2rdf::ttl::constants::LITERAL__YES = generateLiteral("yes", "");

  // Prepare statistic variables
#if defined(_OPENMP)
  _numOuts = omp_get_max_threads();
#else
  _numOuts = 1;
#endif
  _blankNodeCount = new uint64_t[_numOuts];
  _headerLines = new uint64_t[_numOuts];
  _lineCount = new uint64_t[_numOuts];
  for (size_t i = 0; i < _numOuts; ++i) {
    _blankNodeCount[i] = 0;
    _headerLines[i] = 0;
    _lineCount[i] = 0;
  }
}

// ____________________________________________________________________________
template <typename T>
osm2rdf::ttl::Writer<T>::~Writer() {
  delete[] _blankNodeCount;
  delete[] _headerLines;
  delete[] _lineCount;
}

// ____________________________________________________________________________
template <typename T>
bool osm2rdf::ttl::Writer<T>::addPrefix(const std::string& prefix,
                                        std::string_view value) {
  auto prefixIt = _prefixes.find(prefix);
  if (prefixIt != _prefixes.end()) {
    return false;
  }
  _prefixes[prefix] = value;
  return true;
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::resolvePrefix(std::string_view p) {
  auto prefix = _prefixes.find(std::string{p});
  if (prefix != _prefixes.end()) {
    return prefix->second;
  }
  return std::string(p);
}

// ____________________________________________________________________________
template <typename T>
void osm2rdf::ttl::Writer<T>::writeStatisticJson(
    const std::filesystem::path& output) {
  // Combine data from threads.
  uint64_t blankNodeCount = 0;
  uint64_t headerLines = 0;
  uint64_t lineCount = 0;
  for (size_t i = 0; i < _numOuts; ++i) {
    blankNodeCount += _blankNodeCount[i];
    headerLines += _headerLines[i];
    lineCount += _lineCount[i];
  }

  // Write json
  std::ofstream out{output};
  out << "{" << std::endl;
  out << "  \"blankNodes\": " << blankNodeCount << "," << std::endl;
  out << "  \"header\": " << headerLines << "," << std::endl;
  out << "  \"lines\": " << lineCount << "," << std::endl;
  out << "  \"triples\": " << lineCount - headerLines << std::endl;
  out << "}" << std::endl;
  out.close();
}

// ____________________________________________________________________________
template <typename T>
void osm2rdf::ttl::Writer<T>::writeHeader() {
  for (const auto& [prefix, iriref] : _prefixes) {
    writeTriple("@prefix", prefix + ":", "<" + iriref + ">");
#if defined(_OPENMP)
    _headerLines[omp_get_thread_num()]++;
#else
    _headerLines[0]++;
#endif
  }
}

// ____________________________________________________________________________
template <>
void osm2rdf::ttl::Writer<osm2rdf::ttl::format::NT>::writeHeader() {}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::generateBlankNode() {
  int theadId = 0;
#if defined(_OPENMP)
  theadId = omp_get_thread_num();
#endif
  return "_:" + std::to_string(theadId) + "_" +
         std::to_string(_blankNodeCount[theadId]++);
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::generateIRI(std::string_view p,
                                                 uint64_t v) {
  return generateIRIUnsafe(p, std::to_string(v));
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::generateIRIUnsafe(std::string_view p,
                                                       std::string_view v) {
  return formatIRIUnsafe(p, v);
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::generateIRI(std::string_view p,
                                                 std::string_view v) {
  // trims whitespace
  auto begin = std::find_if(v.begin(), v.end(),
                            [](int c) { return std::isspace(c) == 0; });
  auto end = std::find_if(v.rbegin(), v.rend(),
                          [](int c) { return std::isspace(c) == 0; });
  // Trim strings
  return formatIRI(
      p, v.substr(begin - v.begin(), std::distance(begin, end.base())));
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::generateLangTag(std::string_view s) {
  bool allowDigits = false;
  bool ok = true;
  for (size_t pos = 0; pos < s.length(); pos++) {
    if (pos == 0 && s[0] == '-') {
      ok = false;
      break;
    }
    if ((s[pos] >= 'a' && s[pos] <= 'z') || (s[pos] >= 'A' && s[pos] <= 'Z') ||
        (s[pos] >= '0' && s[pos] <= '9' && allowDigits)) {
    } else if (s[pos] == '-') {
      allowDigits = true;
    } else {
      ok = false;
      break;
    }
  }
  if (!ok) {
    throw std::domain_error("Invalid LangTag s: '" + std::string{s});
  }
  std::string tmp = "@";
  tmp.reserve(s.size() + 1);
  tmp += s;
  return tmp;
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::generateLiteral(std::string_view v,
                                                     std::string_view s) {
  return STRING_LITERAL_QUOTE(v) + std::string(s);
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::generateLiteralUnsafe(std::string_view v,
                                                           std::string_view s) {
  // only put literal in quotes
  std::string ret;
  ret.reserve(v.size() + 2 + s.size());
  ret += '"';
  ret += v;
  ret += '"';
  ret += s;

  return ret;
}

// ____________________________________________________________________________
template <typename T>
void osm2rdf::ttl::Writer<T>::writeTriple(const std::string& s,
                                          const std::string& p,
                                          const std::string& o) {
  _out->write(s);
  _out->write(' ');
  _out->write(p);
  _out->write(' ');
  _out->write(o);
  _out->write(' ');
  _out->write('.');
  _out->writeNewLine();
#if defined(_OPENMP)
  _lineCount[omp_get_thread_num()]++;
#else
  _lineCount[0]++;
#endif
}

// ____________________________________________________________________________
template <>
std::string osm2rdf::ttl::Writer<osm2rdf::ttl::format::NT>::formatIRI(
    std::string_view p, std::string_view v) {
  // NT:  [8]    IRIREF
  //      https://www.w3.org/TR/n-triples/#grammar-production-IRIREF
  auto prefix = _prefixes.find(std::string{p});
  if (prefix != _prefixes.end()) {
    return IRIREF(prefix->second, v);
  }
  return IRIREF(p, v);
}

// ____________________________________________________________________________
template <>
std::string osm2rdf::ttl::Writer<osm2rdf::ttl::format::NT>::formatIRIUnsafe(
    std::string_view p, std::string_view v) {
  return formatIRI(p, v);
}

// ____________________________________________________________________________
template <>
std::string osm2rdf::ttl::Writer<osm2rdf::ttl::format::TTL>::formatIRIUnsafe(
    std::string_view p, std::string_view v) {
  // TTL: [135s] iri
  //      https://www.w3.org/TR/turtle/#grammar-production-iri
  //      [18]   IRIREF (same as NT)
  //      https://www.w3.org/TR/turtle/#grammar-production-IRIREF
  //      [136s] PrefixedName
  //      https://www.w3.org/TR/turtle/#grammar-production-PrefixedName
  auto prefix = _prefixes.find(std::string{p});
  // If known prefix -> PrefixedName
  if (prefix != _prefixes.end()) {
    return PrefixedNameUnsafe(p, v);
  }
  return IRIREF(p, v);
}

// ____________________________________________________________________________
template <>
std::string osm2rdf::ttl::Writer<osm2rdf::ttl::format::QLEVER>::formatIRIUnsafe(
    std::string_view p, std::string_view v) {
  // TTL: [135s] iri
  //      https://www.w3.org/TR/turtle/#grammar-production-iri
  //      [18]   IRIREF (same as NT)
  //      https://www.w3.org/TR/turtle/#grammar-production-IRIREF
  //      [136s] PrefixedName
  //      https://www.w3.org/TR/turtle/#grammar-production-PrefixedName
  auto prefix = _prefixes.find(std::string{p});
  // If known prefix -> PrefixedName
  if (prefix != _prefixes.end()) {
    return PrefixedNameUnsafe(p, v);
  }
  return IRIREF(p, v);
}

// ____________________________________________________________________________
template <>
std::string osm2rdf::ttl::Writer<osm2rdf::ttl::format::TTL>::formatIRI(
    std::string_view p, std::string_view v) {
  // TTL: [135s] iri
  //      https://www.w3.org/TR/turtle/#grammar-production-iri
  //      [18]   IRIREF (same as NT)
  //      https://www.w3.org/TR/turtle/#grammar-production-IRIREF
  //      [136s] PrefixedName
  //      https://www.w3.org/TR/turtle/#grammar-production-PrefixedName
  auto prefix = _prefixes.find(std::string{p});
  // If known prefix -> PrefixedName
  if (prefix != _prefixes.end()) {
    return PrefixedName(p, v);
  }
  return IRIREF(p, v);
}

// ____________________________________________________________________________
template <>
std::string osm2rdf::ttl::Writer<osm2rdf::ttl::format::QLEVER>::formatIRI(
    std::string_view p, std::string_view v) {
  // TTL: [135s] iri
  //      https://www.w3.org/TR/turtle/#grammar-production-iri
  //      [18]   IRIREF (same as NT)
  //      https://www.w3.org/TR/turtle/#grammar-production-IRIREF
  //      [136s] PrefixedName
  //      https://www.w3.org/TR/turtle/#grammar-production-PrefixedName
  auto prefix = _prefixes.find(std::string{p});
  // If known prefix -> PrefixedName
  if (prefix != _prefixes.end()) {
    return PrefixedName(p, v);
  }
  return IRIREF(p, v);
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::IRIREF(std::string_view p,
                                            std::string_view v) {
  // NT:  [8]    IRIREF
  //      https://www.w3.org/TR/n-triples/#grammar-production-IRIREF
  // TTL: [18]   IRIREF (same as NT)
  //      https://www.w3.org/TR/turtle/#grammar-production-IRIREF
  return "<" + encodeIRIREF(p) + encodeIRIREF(v) + ">";
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::PrefixedName(std::string_view p,
                                                  std::string_view v) {
  // TTL: [136s] PrefixedName
  //      https://www.w3.org/TR/turtle/#grammar-production-PrefixedName
  return encodePN_PREFIX(p) + ":" + encodePN_LOCAL(v);
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::PrefixedNameUnsafe(std::string_view p,
                                                        std::string_view v) {
  // TTL: [136s] PrefixedName
  //      https://www.w3.org/TR/turtle/#grammar-production-PrefixedName
  return std::string(p) + ":" + std::string(v);
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::STRING_LITERAL_QUOTE(std::string_view s) {
  // NT:  [9]   STRING_LITERAL_QUOTE
  //      https://www.w3.org/TR/n-triples/#grammar-production-STRING_LITERAL_QUOTE
  // TTL: [22]  STRING_LITERAL_QUOTE
  //      https://www.w3.org/TR/turtle/#grammar-production-STRING_LITERAL_QUOTE
  std::string tmp;
  tmp.reserve(s.size() * 2);
  tmp += "\"";
  for (const auto c : s) {
    switch (c) {
      case '\"':  // #x22
        tmp += "\\\"";
        break;
      case '\\':  // #x5C
        tmp += "\\\\";
        break;
      case '\n':  // #x0A
        tmp += "\\n";
        break;
      case '\r':  // #x0D
        tmp += "\\r";
        break;
      default:
        tmp += c;
    }
  }
  tmp += "\"";
  return tmp;
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::STRING_LITERAL_SINGLE_QUOTE(
    std::string_view s) {
  // TTL: [23]  STRING_LITERAL_QUOTE
  //      https://www.w3.org/TR/turtle/#grammar-production-STRING_LITERAL_SINGLE_QUOTE
  std::string tmp;
  tmp.reserve(s.size() * 2);
  tmp += "\'";
  for (const auto c : s) {
    switch (c) {
      case '\'':  // #x22
        tmp += "\\\'";
        break;
      case '\\':  // #x5C
        tmp += "\\\\";
        break;
      case '\n':  // #x0A
        tmp += "\\n";
        break;
      case '\r':  // #x0D
        tmp += "\\r";
        break;
      default:
        tmp += c;
    }
  }
  tmp += "\'";
  return tmp;
}

// ____________________________________________________________________________
template <typename T>
uint8_t osm2rdf::ttl::Writer<T>::utf8Length(char c) {
  auto cp = static_cast<uint8_t>(c);
  if ((cp & k0x80) == 0) {
    return k1Byte;
  }
  if ((cp & k0xE0) == k0xC0) {
    return k2Byte;
  }
  if ((cp & k0xF0) == k0xE0) {
    return k3Byte;
  }
  if ((cp & k0xF8) == k0xF0) {
    return k4Byte;
  }
  throw std::domain_error("Invalid UTF-8 Sequence start " + std::to_string(cp) +
                          "(dec)");
}

// ____________________________________________________________________________
template <typename T>
uint8_t osm2rdf::ttl::Writer<T>::utf8Length(std::string_view s) {
  if (s.empty()) {
    return 0;
  }
  return utf8Length(s[0]);
}

// ____________________________________________________________________________
template <typename T>
uint32_t osm2rdf::ttl::Writer<T>::utf8Codepoint(std::string_view s) {
  switch (utf8Length(s)) {
    case k4Byte:
      // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
      //      111   111111   111111   111111 = 7 3F 3F 3F
      return (((s[0] & k0x07) << UTF8_CODEPOINT_OFFSET_BYTE4) |
              ((s[1] & k0x3F) << UTF8_CODEPOINT_OFFSET_BYTE3) |
              ((s[2] & k0x3F) << UTF8_CODEPOINT_OFFSET_BYTE2) | (s[3] & k0x3F));
    case k3Byte:
      // 1110xxxx 10xxxxxx 10xxxxxx
      //     1111   111111   111111 = F 3F 3F
      return (((s[0] & k0x0F) << UTF8_CODEPOINT_OFFSET_BYTE3) |
              ((s[1] & k0x3F) << UTF8_CODEPOINT_OFFSET_BYTE2) | (s[2] & k0x3F));
    case k2Byte:
      // 110xxxxx 10xxxxxx
      //    11111   111111 = 1F 3F
      return (((s[0] & k0x1F) << UTF8_CODEPOINT_OFFSET_BYTE2) | (s[1] & k0x3F));
    default:
      // 0xxxxxxx
      //  1111111 = 7F
      return (s[0] & k0x7F);
  }
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::UCHAR(char c) {
  return UCHAR(static_cast<uint32_t>(c));
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::UCHAR(std::string_view s) {
  return UCHAR(utf8Codepoint(s));
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::UCHAR(uint32_t codepoint) {
  // NT:  [10]  UCHAR
  //      https://www.w3.org/TR/n-triples/#grammar-production-UCHAR
  // TTL: [26]  UCHAR
  //      https://www.w3.org/TR/turtle/#grammar-production-UCHAR
  std::ostringstream tmp;
  tmp << std::setfill('0');
  if (codepoint > k0xFFFFU) {
    tmp << "\\U" << std::setw(UTF8_BYTES_LONG);
  } else {
    tmp << "\\u" << std::setw(UTF8_BYTES_SHORT);
  }
  tmp << std::hex << codepoint;
  return tmp.str();
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::encodeIRIREF(std::string_view s) {
  // NT:  [8]   IRIREF
  //      https://www.w3.org/TR/n-triples/#grammar-production-IRIREF
  // TTL: [18]  IRIREF
  //      https://www.w3.org/TR/turtle/#grammar-production-IRIREF
  std::string tmp;
  tmp.reserve(s.size() * 2);
  for (size_t pos = 0; pos < s.size(); ++pos) {
    // Force non-allowed chars to UCHAR
    auto c = s[pos];
    if ((c >= 0x00 && c <= ' ') || c == '<' || c == '>' || c == '{' ||
        c == '}' || c == '\"' || c == '|' || c == '^' || c == '`' ||
        c == '\\') {
      tmp += UCHAR(c);
      continue;
    }
    uint8_t length = utf8Length(c);
    tmp += s.substr(pos, length);
    pos += length - 1;
  }
  return tmp;
}

// ____________________________________________________________________________
template <>
std::string osm2rdf::ttl::Writer<osm2rdf::ttl::format::QLEVER>::encodeIRIREF(
    std::string_view s) {
  // NT:  [8]   IRIREF
  //      https://www.w3.org/TR/n-triples/#grammar-production-IRIREF
  // TTL: [18]  IRIREF
  //      https://www.w3.org/TR/turtle/#grammar-production-IRIREF
  std::string tmp;
  tmp.reserve(s.size() * 2);
  for (size_t pos = 0; pos < s.size(); ++pos) {
    uint8_t length = utf8Length(s[pos]);
    // Force non-allowed chars to PERCENT
    if (length == k1Byte) {
      if ((s[pos] >= 0x00 && s[pos] <= ' ') || s[pos] == '<' || s[pos] == '>' ||
          s[pos] == '{' || s[pos] == '}' || s[pos] == '\"' || s[pos] == '|' ||
          s[pos] == '^' || s[pos] == '`' || s[pos] == '\\') {
        tmp += encodePERCENT(s.substr(pos, 1));
        continue;
      }
    }
    tmp += s.substr(pos, length);
    pos += length - 1;
  }
  return tmp;
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::encodePERCENT(char c) {
  return encodePERCENT(static_cast<uint32_t>(c));
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::encodePERCENT(std::string_view s) {
  return encodePERCENT(utf8Codepoint(s));
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::encodePERCENT(uint32_t codepoint) {
  // TTL: [170s] PERCENT
  //      https://www.w3.org/TR/turtle/#grammar-production-PERCENT
  std::vector<std::string> parts;
  parts.reserve(4);

  // Generate parts
  std::ostringstream tmp;
  tmp << std::setfill('0');
  do {
    tmp << "%" << std::setw(2) << std::hex << (codepoint & k0xFFU);
    parts.push_back(tmp.str());
    tmp.seekp(0);
    codepoint = codepoint >> NUM_BITS_IN_BYTE;
  } while (codepoint > 0);

  // Revert order of string parts
  std::reverse(parts.begin(), parts.end());
  for (const auto& part : parts) {
    tmp << part;
  }
  return tmp.str();
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::encodePN_PREFIX(std::string_view s) {
  // TTL: [167s] PN_LOCAL
  //      https://www.w3.org/TR/turtle/#grammar-production-PN_PREFIX
  std::string tmp;
  tmp.reserve(s.size() * 2);
  for (size_t pos = 0; pos < s.size(); ++pos) {
    // PN_PREFIX     ::= PN_CHARS_BASE ((PN_CHARS | '.')* PN_CHARS)?
    //
    // PN_CHARS_U    ::= PN_CHARS_BASE | '_'
    //
    // PN_CHARS      ::= PN_CHARS_U | '-' | [0-9] | #x00B7 | [#x0300-#x036F] |
    //                   [#x203F-#x2040]
    //
    // PN_CHARS_BASE ::= [A-Z] | [a-z] | [#x00C0-#x00D6] | [#x00D8-#x00F6] |
    //                   [#x00F8-#x02FF] | [#x0370-#x037D] | [#x037F-#x1FFF] |
    //                   [#x200C-#x200D] | [#x2070-#x218F] | [#x2C00-#x2FEF] |
    //                   [#x3001-#xD7FF] | [#xF900-#xFDCF] | [#xFDF0-#xFFFD] |
    //                   [#x10000-#xEFFFF]

    auto currentChar = s[pos];
    // _, :, A-Z, a-z, and 0-9 always allowed:
    if ((currentChar >= 'A' && currentChar <= 'Z') ||
        (currentChar >= 'a' && currentChar <= 'z')) {
      tmp += currentChar;
      continue;
    }
    // First chars are never 0-9, _ or -
    if (pos > 0) {
      if ((currentChar >= '0' && currentChar <= '9') || currentChar == '_' ||
          currentChar == '-') {
        tmp += currentChar;
        continue;
      }
      if (currentChar == '.' && pos < s.size() - 1) {
        tmp += currentChar;
        continue;
      }
    }
    uint8_t length = utf8Length(currentChar);
    std::string_view sub = s.substr(pos, length);
    uint32_t c = utf8Codepoint(sub);
    // Handle allowed Codepoints for CHARS_U
    if ((c >= k0xC0 && c <= k0xD6) || (c >= k0xD8 && c <= k0xF6) ||
        (c >= k0xF8 && c <= k0x2FF) || (c >= k0x370 && c <= k0x37D) ||
        (c >= k0x37F && c <= k0x1FFF) || (c >= k0x200C && c <= k0x200D) ||
        (c >= k0x2070 && c <= k0x218F) || (c >= k0x2C00 && c <= k0x2FEF) ||
        (c >= k0x3001 && c <= k0xD7FF) || (c >= k0xF900 && c <= k0xFDCF) ||
        (c >= k0xFDF0 && c <= k0xFFFD) || (c >= k0x10000 && c <= k0xEFFFF)) {
      tmp += sub;
    } else if (pos > 0 && (c == k0xB7 || (c >= k0x300 && c <= k0x36F) ||
                           (c >= k0x203F && c <= k0x2040))) {
      tmp += sub;
    } else {
      throw std::domain_error("Invalid UTF-8 Sequence: '" + std::string{sub} +
                              "' '" + encodePERCENT(sub) +
                              "' A:" + std::string(s));
    }
    // Shift new pos according to utf8-bytecount
    pos += length - 1;
  }
  return tmp;
}

// ____________________________________________________________________________
template <typename T>
std::string osm2rdf::ttl::Writer<T>::encodePN_LOCAL(std::string_view s) {
  // TTL: [168s] PN_LOCAL
  //      https://www.w3.org/TR/turtle/#grammar-production-PN_LOCAL
  std::string tmp;
  tmp.reserve(s.size() * 2);
  for (size_t pos = 0; pos < s.size(); ++pos) {
    // PN_LOCAL      ::= (PN_CHARS_U | ':' | [0-9] | PLX)
    //                   ((PN_CHARS | '.' | ':' | PLX)*
    //                   (PN_CHARS | ':' | PLX))?
    //
    // PN_CHARS_U    ::= PN_CHARS_BASE | '_'
    //
    // PN_CHARS      ::= PN_CHARS_U | '-' | [0-9] | #x00B7 | [#x0300-#x036F] |
    //                   [#x203F-#x2040]
    //
    // PN_CHARS_BASE ::= [A-Z] | [a-z] | [#x00C0-#x00D6] | [#x00D8-#x00F6] |
    //                   [#x00F8-#x02FF] | [#x0370-#x037D] | [#x037F-#x1FFF] |
    //                   [#x200C-#x200D] | [#x2070-#x218F] | [#x2C00-#x2FEF] |
    //                   [#x3001-#xD7FF] | [#xF900-#xFDCF] | [#xFDF0-#xFFFD] |
    //                   [#x10000-#xEFFFF]
    //
    // PLX           ::= PERCENT | PN_LOCAL_ESC
    //
    // PERCENT       ::= '%' HEX HEX
    //
    // HEX           ::= [0-9] | [A-F] | [a-f]
    //
    // PN_LOCAL_ESC  ::= '\' ('_' | '~' | '.' | '-' | '!' | '$' | '&' | "'" |
    //                        '(' | ')' | '*' | '+' | ',' | ';' | '=' | '/' |
    //                        '?' | '#' | '@' | '%')

    auto currentChar = s[pos];
    // _, :, A-Z, a-z, and 0-9 always allowed:
    if (currentChar == ':' || currentChar == '_' ||
        (currentChar >= 'A' && currentChar <= 'Z') ||
        (currentChar >= 'a' && currentChar <= 'z') ||
        (currentChar >= '0' && currentChar <= '9')) {
      tmp += currentChar;
      continue;
    }
    // First and last char is never .
    if (currentChar == '.' && pos > 0 && pos < s.size() - 1) {
      tmp += currentChar;
      continue;
    }
    // First char is never -
    if (currentChar == '-' && pos > 0) {
      tmp += currentChar;
      continue;
    }
    // Handle PN_LOCAL_ESC
    if (currentChar == '!' || (currentChar >= '#' && currentChar <= '/') ||
        currentChar == ';' || currentChar == '=' || currentChar == '?' ||
        currentChar == '@' || currentChar == '~') {
      tmp += '\\';
      tmp += currentChar;
      continue;
    }
    // Percent encoding has 2 HEX slots -> use for rest of ascii 0x00 - 0x7F
    if (currentChar >= 0x00) {
      tmp += encodePERCENT(currentChar);
      continue;
    }
    uint8_t length = utf8Length(currentChar);
    std::string_view sub = s.substr(pos, length);
    uint32_t c = utf8Codepoint(sub);
    // Handle allowed Codepoints for CHARS_U
    if ((c >= k0xC0 && c <= k0xD6) || (c >= k0xD8 && c <= k0xF6) ||
        (c >= k0xF8 && c <= k0x2FF) || (c >= k0x370 && c <= k0x37D) ||
        (c >= k0x37F && c <= k0x1FFF) || (c >= k0x200C && c <= k0x200D) ||
        (c >= k0x2070 && c <= k0x218F) || (c >= k0x2C00 && c <= k0x2FEF) ||
        (c >= k0x3001 && c <= k0xD7FF) || (c >= k0xF900 && c <= k0xFDCF) ||
        (c >= k0xFDF0 && c <= k0xFFFD) || (c >= k0x10000 && c <= k0xEFFFF)) {
      tmp += sub;
    } else if (pos > 0 && (c == k0xB7 || (c >= k0x300 && c <= k0x36F) ||
                           (c >= k0x203F && c <= k0x2040))) {
      tmp += sub;
    } else {
      // TODO(lehmanna): handle all other symbols?
      // PLX only allows "\X" and PERCENT "% HEX HEX" -> no utf8?
      throw std::domain_error("Invalid UTF-8 Sequence: '" + std::string{sub} +
                              "' '" + encodePERCENT(sub) +
                              "' B:" + std::string(s));
    }
    // Shift new pos according to utf8-bytecount
    pos += length - 1;
  }
  return tmp;
}

// ____________________________________________________________________________
template class osm2rdf::ttl::Writer<osm2rdf::ttl::format::NT>;
template class osm2rdf::ttl::Writer<osm2rdf::ttl::format::TTL>;
template class osm2rdf::ttl::Writer<osm2rdf::ttl::format::QLEVER>;
