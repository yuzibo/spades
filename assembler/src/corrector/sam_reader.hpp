//***************************************************************************
//* Copyright (c) 2011-2014 Saint-Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//****************************************************************************
#pragma once

#include "read.hpp"

#include "io/ireader.hpp"

#include <samtools/sam.h>
#include <samtools/bam.h>

namespace corrector {

class MappedSamStream {
public:
    MappedSamStream(const std::string &filename)
            : filename_(filename) {
        open();
    }

    virtual ~MappedSamStream() {
    }

    bool is_open() const;
    bool eof() const;
    MappedSamStream& operator>>(SingleSamRead& read);
    MappedSamStream& operator >>(PairedSamRead& read);
    string get_contig_name(int i) const;
    void close();
    void reset();
    io::ReadStreamStat get_stat() const;

private:
    samfile_t *reader_;
    bam1_t *seq_ = bam_init1();
    std::string filename_;
    bool is_open_;
    bool eof_;

    void open();
};

}
;
