#include <stdlib.h>
#include "reader.h"

Reader *conIn_ = new StreamReader(stdin);

/* ˆês“ü—Í
 *    line - “ü—Í•¶Žš—ñ(‰üs‚ÍŠÜ‚Ü‚È‚¢)
 * return
 *     •¶Žš” / -1:EOF
 */
int Reader::readLine( NnString &line )
{
    line.erase();
    if( this->eof() )
	return -1;

    int ch;
    while( (ch=this->getchr()) != '\n' ){
        if( ch == EOF ){
            if( line.length() <= 0 )
                return -1;
            else
                return line.length();
        }
	line += char(ch);
    }

    return line.length();
}


int StreamReader::eof() const
{
    return fp==NULL || feof(fp);
}

int StreamReader::getchr()
{
    return fp==NULL || feof(fp) ? EOF : getc(fp);
}


FileReader::FileReader( const char *path )
{
    fp = NULL;
    this->open( path );
}

int FileReader::open( const char *path )
{
    this->close();
    fp = fopen( NnDir::long2short(path) , "r" );
    return fp != NULL ? 0 : -1 ;
}

void FileReader::close()
{
    if( fp != NULL )
	fclose( fp );
    fp = NULL;
}

FileReader::~FileReader()
{
    this->close();
}
