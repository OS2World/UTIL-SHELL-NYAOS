#ifndef LFN_H
#define LFN_H

#include "nnstring.h"
#include "nnhash.h"

typedef unsigned long long filesize_t;

struct NnTimeStamp {
    int second;
    int minute;
    int hour;
    int day;
    int month;
    int year;

    NnTimeStamp() :
	second(0) , minute(0) , hour(0) ,
	day(0) , month(0) , year(0){}
    int compare( const NnTimeStamp &) const ;
};

class NnFileStat : public NnSortable {
    NnString  name_ ;
    unsigned  long attr_ ;
    filesize_t size_ ;
    NnTimeStamp stamp_ ;
public:
    enum{
	ATTR_READONLY = 0x01 ,
	ATTR_HIDDEN   = 0x02 ,
	ATTR_SYSTEM   = 0x04 ,
	ATTR_LABEL    = 0x08 ,
	ATTR_DIRECTORY= 0x10 ,
	ATTR_ARCHIVED = 0x20 
    };

    NnFileStat( const NnString &name , unsigned attr , filesize_t size ,
	const NnTimeStamp &stamp )
	: name_(name) ,    attr_(attr)    , size_(size) , stamp_(stamp) { }
    NnFileStat( const NnFileStat &p )
	: name_(p.name_) , attr_(p.attr_) , size_(p.size_) , stamp_(p.stamp_){ }
    ~NnFileStat();

    const NnString &name() const { return name_; }
    unsigned attr() const { return attr_; }
    filesize_t size() const { return size_; }
    const NnTimeStamp &stamp() const { return stamp_; }

    int isReadOnly() const { return (attr_ & ATTR_READONLY ) != 0 ; }
    int isHidden()   const { return (attr_ & ATTR_HIDDEN   ) != 0 ; }
    int isSystem()   const { return (attr_ & ATTR_SYSTEM   ) != 0 ; }
    int isLabel()    const { return (attr_ & ATTR_LABEL    ) != 0 ; }
    int isDir()      const { return (attr_ & ATTR_DIRECTORY) != 0 ; }
    int isArchived() const { return (attr_ & ATTR_ARCHIVED ) != 0 ; }

    virtual int compare( const NnSortable &another ) const ;

    static NnFileStat *stat( const NnString &name );
};

int fnexplode( const char *path , NnVector &list );
int fnexplode_( const char *path , NnVector &list );
NnString &glob_all( const char *line , NnString &result );

class NnDir : public NnEnum {
    struct Core;
    Core       *core;
    int 	status;
    unsigned	attr_ ;
    filesize_t  size_ ;
    NnString    name_;
    NnTimeStamp stamp_;
    unsigned findfirst( const NnString &path , unsigned attr );
    unsigned findfirst( const char *path     , unsigned attr );
    unsigned findnext ( );
    void findclose();
public:
    NnDir( const NnString &path , int attr=0x37 ) : core(0)
	{ status = NnDir::findfirst( path , attr ); }
    NnDir( const char *path , int attr=0x37 ) : core(0)
	{ status = NnDir::findfirst( path , attr ); }
    NnDir() : core(0)
        { status = NnDir::findfirst( "." , 0x37 ); }
    ~NnDir();
    virtual NnObject *operator *();
    virtual void      operator++(void);
    NnString *operator ->(){ return &name_; }
    const char *name() const { return name_.chars() ; }
    int isReadOnly() const { return (attr_ & NnFileStat::ATTR_READONLY )!=0; }
    int isHidden()   const { return (attr_ & NnFileStat::ATTR_HIDDEN   )!=0; }
    int isSystem()   const { return (attr_ & NnFileStat::ATTR_SYSTEM   )!=0; }
    int isLabel()    const { return (attr_ & NnFileStat::ATTR_LABEL    )!=0; }
    int isDir()      const { return (attr_ & NnFileStat::ATTR_DIRECTORY)!=0; }
    int isArchived() const { return (attr_ & NnFileStat::ATTR_ARCHIVED )!=0; }
    int attr()       const { return  attr_ ; }
    filesize_t size()const { return  size_ ; }
    const NnTimeStamp &stamp() const { return stamp_; }

    NnFileStat *stat() const { return new NnFileStat(name_,attr_,size_,stamp_);}

    // 各種 VFAT 用ツール集.
    static void f2b( const char * , NnString & );
    static void filter( const char * , NnString & );
    static int  getcwd( NnString &pwd );
    static int  getcwdrive();
    static int  chdrive( int driveletter );
    static int  chdir( const char *dirname );
    static int  open( const char *fname , const char *mode );
    static int  write( int fd , const void *ptr , size_t size );
    static void close( int fd );
    static const NnString &tempfn();
    static int access( const char *path );
    static int lastRoot(const char *path)
	{ return NnString::findLastOf(path,"/\\:"); }
    static int seekEnd( int handle );
    static const char *long2short( const char *src ){ return src; }
    static void extractDots( const char *&sp, NnString &dst );
    static NnHash specialFolder;
    static void set_default_special_folder();
};

#endif
