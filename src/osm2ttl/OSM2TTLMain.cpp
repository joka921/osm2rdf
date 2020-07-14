// Copyright 2020, University of Freiburg
// Authors: Axel Lehmann <lehmann@cs.uni-freiburg.de>.

#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <string>

#include "osmium/area/assembler.hpp"
#include "osmium/area/multipolygon_manager.hpp"
#include "osmium/io/any_input.hpp"
#include "osmium/io/reader_with_progress_bar.hpp"
#include "osmium/util/memory.hpp"

#include "osm2ttl/_config.h"
#include "osm2ttl/config/Config.h"
#include "osm2ttl/ttl/Writer.h"
#include "osm2ttl/osm/AreaHandler.h"
#include "osm2ttl/osm/DumpHandler.h"
#include "osm2ttl/osm/LocationHandler.h"

// ____________________________________________________________________________
int main(int argc, char** argv) {
  osm2ttl::config::Config& config = osm2ttl::config::Config::getInstance();
  config.fromArgs(argc, argv);

  try {
    // Setup
    // Input file reference
    osmium::io::File input_file{config.input};
    osm2ttl::ttl::Writer writer{config};
    if (!writer.open()) {
      std::cerr << "Error opening outputfile: " << config.output << std::endl;
      exit(1);
    }
    writer.writeHeader();

    osm2ttl::osm::AreaHandler areaHandler{config, &writer};
    osm2ttl::osm::DumpHandler dumpHandler{config, &writer, &areaHandler};
    osm2ttl::osm::LocationHandler* locationHandler =
      osm2ttl::osm::LocationHandler::create(config);

    {
      // Do not create empty areas
      osmium::area::Assembler::config_type assembler_config;
      assembler_config.create_empty_areas = false;
      osmium::area::MultipolygonManager<osmium::area::Assembler>
      mp_manager{assembler_config};

      // read relations for areas
      {
        osmium::io::Reader reader{input_file};
        osmium::ProgressBar progress{reader.file_size(), osmium::isatty(2)};
        std::cerr << "OSM Pass 1 ... (Relations for areas)" << std::endl;
        osmium::relations::read_relations(progress, input_file, mp_manager);
        std::cerr << "... done" << std::endl;
      }

      std::cerr << "Prepare area data for lookup" << std::endl;
      areaHandler.sort();
      std::cerr << "... done" << std::endl;

      // store data
      {
        std::cerr << "OSM Pass 2 ... (dump)" << std::endl;
        osmium::io::ReaderWithProgressBar reader{true, input_file,
          osmium::osm_entity_bits::object};
        osmium::apply(reader, *locationHandler,
          mp_manager.handler([&dumpHandler, &areaHandler](
              osmium::memory::Buffer&& buffer) {
            osmium::apply(buffer, dumpHandler, areaHandler);
        }), dumpHandler);
        reader.close();
        std::cerr << "... done reading ..." << std::endl;
      }
    }

    // All work done, close output
    writer.close();
    std::cerr << "... done writing" << std::endl;
    delete locationHandler;

    osmium::MemoryUsage memory;
    std::cerr << "Memory used: " << memory.peak() << " MBytes" << std::endl;
  } catch (const std::exception& e) {
    // All exceptions used by the Osmium library derive from std::exception.
    std::cerr << e.what() << std::endl;
    // std::exit(1);
  }
}
