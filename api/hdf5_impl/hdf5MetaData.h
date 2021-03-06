/*
 * Copyright (C) 2012 by Glenn Hickey (hickey@soe.ucsc.edu)
 * Copyright (C) 2012-2019 by UCSC Computational Genomics Lab
 *
 * Released under the MIT license, see LICENSE.txt
 */

#ifndef _HDF5METADATA_H
#define _HDF5METADATA_H

#include "halMetaData.h"
#include "hdf5ExternalArray.h"
#include <H5Cpp.h>
#include <map>
#include <string>

namespace hal {

    /**
     * HDF5 string map used for general metadata
     */
    class HDF5MetaData : public MetaData {
      public:
        HDF5MetaData();
        HDF5MetaData(H5::PortableH5Location *parent, const std::string &name);
        virtual ~HDF5MetaData();

        void set(const std::string &key, const std::string &value);
        const std::string &get(const std::string &key) const;
        bool has(const std::string &key) const;
        const std::map<std::string, std::string> &getMap() const;

        void write();

        void open(H5::PortableH5Location *parent, const std::string &name);

      private:
        static const std::string MetaGroupName;

        H5::PortableH5Location *_parent;
        H5::Group _group;
        std::map<std::string, std::string> _map;
        bool _dirty;
        std::string _name;
    };
}
#endif

// Local Variables:
// mode: c++
// End:
