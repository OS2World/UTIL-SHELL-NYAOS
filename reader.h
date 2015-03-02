#ifndef READER_H
#define READER_H

#include <stdio.h>
#include "nndir.h"

class Reader : public NnObject {
public:
    virtual int getchr()=0;
    virtual int eof() const =0 ;
    int readLine( NnString &line );
};

extern Reader *conIn_;
#define conIn (*conIn_)

class StreamReader : public Reader {
protected:
    FILE *fp;
public:
    StreamReader() : fp(0){}
    StreamReader(FILE *fp_) : fp(fp_){}
    virtual int getchr();
    virtual int eof() const;
    int fd() const { return fileno(fp); }

    void getpos( fpos_t &pos ){ fgetpos(fp,&pos); }
    void setpos( fpos_t &pos ){ fsetpos(fp,&pos); }
};

class FileReader : public StreamReader {
public:
    FileReader(){}
    FileReader( const char *fn );
    ~FileReader();

    int open( const char *path );
    void close();
};

#endif
