#ifndef HISTORY_H
#define HISTORY_H

class Reader;

class History1 : public NnSortable {
    NnString body_;
    NnString stamp_;
    void touch_();
public:
    History1() :
        body_(){ touch_(); }
    History1( const NnString &s ) :
        body_(s){ touch_(); }
    History1( const History1 &h) :
        body_(h.body_),stamp_(h.stamp_){}
    History1( const NnString &s , const NnString &stamp ) :
        body_(s) , stamp_(stamp){}

    History1 operator = (const NnString &s )
    {   body_ = s ; touch_(); return *this; }
    History1 operator = (const History1 &h )
    {   body_ = h.body_ ; stamp_ = h.stamp_; return *this; }
    const NnString &stamp(){ return stamp_; }
    NnString &body(){ return body_; }
    int compare( const NnSortable &s ) const;
    const char *repr() const { return body_.chars(); }
};

class History : public NnVector {
public:
    History &operator << ( const NnString & );
    History1 *operator[](int at);
    History1 *top(){
	return this->size() >= 1 
            ? dynamic_cast<History1*>(NnVector::top()) : NULL ; 
    }
    void drop(){ delete (History1*)this->pop(); }
    void pack();
    int set(int at,NnString &value);
    int get(int at,NnString &value);
    int get(int at,History1 &value);
    int seekLineStartsWith( const NnString &key, History1 &line);
    int seekLineHas(        const NnString &key, History1 &line);
    void read( Reader &reader );
};

#endif
