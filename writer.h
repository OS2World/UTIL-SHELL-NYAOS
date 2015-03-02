#ifndef WRITER_H
#define WRITER_H
#include <stdio.h>
#include "nnstring.h"
#include "mysystem.h"

class Writer : public NnObject {
public:
    virtual ~Writer();
    virtual Writer &write( char c )=0;
    virtual Writer &write( const char *s)=0;
    virtual int isatty() const;
    virtual int ok() const=0;

    Writer &operator << ( char c ){ return write(c); }
    Writer &operator << ( const char *s ){ return write(s); }
    Writer &operator << ( int n );
    Writer &operator << (const NnString &s){ return *this << s.chars(); }
};

extern Writer *conOut_,*conErr_;
#define conOut (*conOut_)
#define conErr (*conErr_)

/* <stdio.h> �� FILE* ��ʂ��ďo�͂��� Writer�n�N���X.
 * �t�@�C���̃I�[�v���N���[�Y�Ȃǂ͂��Ȃ��B
 * �قƂ�ǁAstdout,stderr��p
 */
class StreamWriter : public Writer {
    FILE *fp_;
protected:
    FILE *fp(){ return fp_; }
    void setFp( FILE *fp ){ fp_ = fp; }
public:
    ~StreamWriter(){}
    StreamWriter() : fp_(NULL) {}
    StreamWriter( FILE *fp ) : fp_(fp){}
    Writer &write( char c );
    Writer &write( const char *s );
    int ok() const { return fp_ != NULL ; }
    int fd() const { return fileno(fp_); }
    int isatty() const;
    NnObject *clone() const { return new StreamWriter(fp_); }
};

/* �ᐅ��I/O�C���^�[�t�F�C�X���g���āA��ʏo�͂�����B
 * - �t�@�C���f�B�X�N���v�^�͎����N���[�Y���Ȃ�
 */
class RawWriter : public Writer {
    int fd_;
    int failed;
public:
    ~RawWriter();
    RawWriter(int fd=-1) : fd_(fd) , failed(0){}
    Writer &write( char c );
    Writer &write( const char *s );
    int ok() const;
    int fd() const { return fd_; }
    void setFd(int fd){ fd_ = fd; }
    int isatty() const;
    NnObject *clone() const { return new RawWriter(fd_); }
};

class FileWriter : public StreamWriter {
public:
    ~FileWriter();
    FileWriter( const char *fn , const char *mode );
    NnObject *clone() const { return 0; }
};


class PipeWriter : public RawWriter {
    NnString cmds;
    void open( const NnString &cmds );
    phandle_t pid;
public:
    ~PipeWriter();
    PipeWriter( const char *cmds );
    PipeWriter( const NnString &cmds ){ pid=0;open(cmds); }
    NnObject *clone() const { return 0; }
};

#ifdef NYACUS

/* �G�X�P�[�v�V�[�P���X�����߂��āA��ʂ̃A�g���r���[�g��
 * �R���g���[���܂ōs���o�̓N���X.
 */
class AnsiConsoleWriter : public RawWriter {
    static int default_color;
    int flag;
    int param[20];
    size_t n;
    enum { PRINTING , STACKING } mode ;
    enum { BAREN = 1 , GREATER = 2 } ;
    char prevchar;
public:
    AnsiConsoleWriter( int fd ) 
	: RawWriter(fd) , flag(0) , n(0) , mode(PRINTING) , prevchar(0){}
    AnsiConsoleWriter( FILE *fp )
	: RawWriter(fileno(fp)) , flag(0) , n(0) , mode(PRINTING) , prevchar(0){fflush(fp);}

    Writer &write( char c );
    Writer &write( const char *p );

    NnObject *clone() const { return new AnsiConsoleWriter(fd()); }
};

#else

#define AnsiConsoleWriter RawWriter

#endif

/* Writer �̃|�C���^�ϐ��ɑ΂��ă��_�C���N�g���� Writer �N���X. */
class WriterClone : public Writer {
    Writer *rep;
public:
    WriterClone( Writer *rep_ ) : rep(rep_) {}
    ~WriterClone(){}
    Writer &write( char c ){ return rep->write( c ); }
    Writer &write( const char *s ){ return rep->write( s ); }
    int ok() const { return rep->ok(); }
    int isatty() const { return rep->isatty(); }
};

/* �o�͓��e��S�Ď̂ĂĂ��܂� Writer */
class NullWriter : public Writer {
public:
    NullWriter(){}
    ~NullWriter(){}
    Writer &write( char c ){ return *this; }
    Writer &write( const char *s ){ return *this; }
    int ok() const { return 1; }
};

/* �W���o�́E���͂����_�C���N�g������A���ɖ߂����肷��N���X */
class Redirect {
    int original_fd;
    int fd_;
    int isOpenFlag;
public:
    Redirect(int x) : original_fd(-1) , fd_(x) {}
    ~Redirect(){ reset(); }
    void close();
    void reset();
    int  set(int x);
    int  fd() const { return fd_; }

    int switchTo( const NnString &fn , const char *mode );
};

#endif
