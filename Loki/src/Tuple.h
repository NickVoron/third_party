#pragma once

#include "Typelist.h"

namespace Loki
{

// вытягиватель поля по индексу из кортежа
template <class Tuple, int index> struct FieldGetter;

// кортежи с нулевым перерасходом памяти 
template<class TList, int count> struct TypeTupleImpl;

template<class TList> 
struct TypeTupleImpl<TList, 1>
{
	static const int size = 1;

	typedef TList List;
	typedef typename List::Head Head;

	Head field;

	template<class FieldType>
	FieldType& get() { return Loki::FieldGetter<TypeTupleImpl, Loki::TL::IndexOf<TList, FieldType>::value >::get(*this); }

	template<class FieldType>
	const FieldType& get() const { return Loki::FieldGetter<TypeTupleImpl, Loki::TL::IndexOf<TList, FieldType>::value >::get(*this); }
};

template<class TList, int count = TL::Length<TList>::value> 
struct TypeTupleImpl
{
	static const int size = count;

	typedef TypeTupleImpl ThisType;
	typedef TList List;
	typedef typename List::Head Head;
	typedef typename List::Tail Tail;
	typedef TypeTupleImpl<Tail, count - 1> ChildTuple;

	Head field;
	ChildTuple tuple;

	template<class FieldType>
	FieldType& get() { return Loki::FieldGetter<TypeTupleImpl, Loki::TL::IndexOf<TList, FieldType>::value >::get(*this); }	

	template<class FieldType>
	const FieldType& get() const { return Loki::FieldGetter<TypeTupleImpl, Loki::TL::IndexOf<TList, FieldType>::value >::get(*this); }
};

template<class TList> 
struct TypeTuple : public TypeTupleImpl<TList, TL::Length<TList>::value>
{

};

template <class Tuple, int index>
struct FieldGetter
{
	typedef typename Loki::TL::TypeAt<typename Tuple::List, index>::Result ResultType;
	typedef typename Tuple::ChildTuple ChildTuple;

	inline static ResultType& get(Tuple& t)	{ return FieldGetter<ChildTuple, index - 1>::get(t.tuple); }
	inline static const ResultType& get(const Tuple& t) { return FieldGetter<ChildTuple, index - 1>::get(t.tuple); }
};

template <class Tuple>
struct FieldGetter<Tuple, 0>
{
	typedef typename Loki::TL::TypeAt<typename Tuple::List, 0>::Result ResultType;

	inline static		 ResultType& get(Tuple& t)		{return t.field;}
	inline static const ResultType& get(const Tuple& t)	{return t.field;}
};

template <class Tuple> struct FieldGetter<Tuple, -1>{};

// операция над двумя кортежами
struct OpSet{ template<class T, class R> static void exec(T& v1, const R& v2){ v1 = v2;  } };
struct OpAdd{ template<class T, class R> static void exec(T& v1, const R& v2){ v1 += v2; } };
struct OpSub{ template<class T, class R> static void exec(T& v1, const R& v2){ v1 -= v2; } };
struct OpMul{ template<class T, class R> static void exec(T& v1, const R& v2){ v1 *= v2; } };
struct OpDiv{ template<class T, class R> static void exec(T& v1, const R& v2){ v1 /= v2; } };


template<class Tuple, class Op, int size>
struct TupleOpImpl
{
	typedef TupleOpImpl<typename Tuple::ChildTuple, Op, size - 1> SubImpl;

	static inline void exec(Tuple& t1, const Tuple& t2)
	{
		Op::exec(t1.field, t2.field);
		SubImpl::exec(t1.tuple, t2.tuple);
	}

	template<class Scalar>
	static inline void exec(Tuple& t1, Scalar scalar)
	{
		Op::exec(t1.field, scalar);
		SubImpl::exec(t1.tuple, scalar);
	}
};

template<class Tuple, class Op>
struct TupleOpImpl<Tuple, Op, 1>
{
	static inline void exec(Tuple& t1, const Tuple& t2)
	{
		Op::exec(t1.field, t2.field);
	}

	template<class Scalar>
	static inline void exec(Tuple& t1, Scalar scalar)
	{
		Op::exec(t1.field, scalar);
	}
};

template<class Tuple, class Op> 
struct TupleOp
{
	static const int tupleSize = Tuple::size;

	static inline void exec(Tuple& t1, const Tuple& t2)
	{
		TupleOpImpl<Tuple, Op, tupleSize>::exec(t1, t2);
	}

	template<class Scalar>
	static inline void exec(Tuple& t1, Scalar scalar)
	{
		TupleOpImpl<Tuple, Op, tupleSize>::exec(t1, scalar);
	}
};

//
//
//
template<class Op, class List, class T, bool exist> struct TupleMaxOpImplInternalN;

template<class Op, class List, class T>
struct TupleMaxOpImplInternalN<Op, List, T, true>
{
	template<class Tuple1, class Tuple2>
	inline static void exec(Tuple1& t1, const Tuple2& t2)
	{
		typedef FieldGetter< Tuple1, Loki::TL::IndexOf<typename Tuple1::List, T>::value > T1Getter;
		typedef FieldGetter< Tuple2, Loki::TL::IndexOf<typename Tuple2::List, T>::value > T2Getter;
		Op::exec(T1Getter::get(t1), T2Getter::get(t2));
		TupleMaxOpImplInternalN<Op, typename List::Tail, typename List::Tail::Head, Loki::TL::ExistType<typename Tuple1::List, typename List::Tail::Head>::value >::exec(t1, t2);
	}
};

template<class Op, class List, class T>
struct TupleMaxOpImplInternalN<Op, List, T, false>
{
	template<class Tuple1, class Tuple2>
	inline static void exec(Tuple1& t1, const Tuple2& t2)
	{
		TupleMaxOpImplInternalN<Op, typename List::Tail, typename List::Tail::Head, Loki::TL::ExistType<typename Tuple1::List, typename List::Tail::Head>::value >::exec(t1, t2);
	}
};

template<class Op, class List>
struct TupleMaxOpImplInternalN<Op, List, Loki::NullType::Head, false>
{
	template<class Tuple1, class Tuple2>
	inline static void exec(Tuple1& t1, const Tuple2& t2) {}
};

template<class Op, class T, bool exist> struct TupleMaxOpImplInternal1;

template<class Op, class T>
struct TupleMaxOpImplInternal1<Op, T, true>
{
	template<class Tuple1, class Tuple2>
	inline static void exec(Tuple1& t1, const Tuple2& t2)
	{
		typedef FieldGetter< Tuple1, Loki::TL::IndexOf<typename Tuple1::List, T>::value > T1Getter;
		typedef FieldGetter< Tuple2, Loki::TL::IndexOf<typename Tuple2::List, T>::value > T2Getter;
		Op::exec(T1Getter::get(t1), T2Getter::get(t2));
	}
};

template<class Op, class T> 
struct TupleMaxOpImplInternal1<Op, T, false>
{ 
	template<class Tuple1, class Tuple2>
	inline static void copy(Tuple1& t1, const Tuple2& t2) {} 
};


template<class Op, class Tuple1, class Tuple2, int size>
struct TupleMaxOpImpl
{
public:
	template<class T1, class T2>
	inline static void exec(T1& t1, const T2& t2)
	{
		TupleMaxOpImplInternalN<Op, typename T2::List, typename T1::Head, Loki::TL::ExistType<typename T2::List, typename T1::Head>::value >::exec(t1, t2);
	}
};

template<class Op, class Tuple1, class Tuple2>
struct TupleMaxOpImpl<Op, Tuple1, Tuple2, 1>
{
public:
	inline static void exec(Tuple1& t1, const Tuple2& t2)
	{
		TupleMaxOpImplInternal1<Op, typename Tuple1::Head, Loki::TL::ExistType<typename Tuple2::List, typename Tuple1::Head>::value >::exec(t1, t2);
	}
};


//
//
//
template<class Tuple1, class Tuple2, class Op> 
struct TupleMaxOp
{
	inline static void exec(Tuple1& t1, const Tuple2& t2)
	{		
		TupleMaxOpImpl<Op, Tuple1, Tuple2, Tuple1::size>::exec(t1, t2);
	}
};


//
//
//

template<class Tuple1, class Tuple2, class SelectedList>
struct TupleSelectOpImpl
{
	inline static void exec(Tuple1& t1, const Tuple2& t2)
	{
		typedef FieldGetter< Tuple1, TL::IndexOf<typename Tuple1::List, typename SelectedList::Head>::value > T1Getter;
		typedef FieldGetter< Tuple2, TL::IndexOf<typename Tuple2::List, typename SelectedList::Head>::value > T2Getter;
		TupleSelectOpImpl::exec(T1Getter::get(t1), T2Getter::get(t2));
		TupleSelectOpImpl<Tuple1, Tuple2, typename SelectedList::Tail>::exec(t1, t2);
	}
};

template<class Tuple1, class Tuple2>
struct TupleSelectOpImpl<Tuple1, Tuple2, Loki::NullType>
{
	inline static void exec(Tuple1& t1, const Tuple2& t2) {}
};

template<class Tuple1, class Tuple2, class SelectedList, class Op> 
struct TupleSelectOp
{
	inline static void exec(Tuple1& t1, const Tuple2& t2)
	{
		TupleSelectOpImpl<Tuple1, Tuple2, SelectedList>::exec(t1, t2);
	}
};


//
//
//
template<class Tuple> struct TupleAdd : public TupleOp<Tuple, OpAdd>{};
template<class Tuple> struct TupleSub : public TupleOp<Tuple, OpSub>{};
template<class Tuple> struct TupleMul : public TupleOp<Tuple, OpMul>{};
template<class Tuple> struct TupleDiv : public TupleOp<Tuple, OpDiv>{};
template<class Tuple> struct TupleSet : public TupleOp<Tuple, OpSet>{};

template<class Tuple1, class Tuple2> struct TupleMaxAdd : public TupleMaxOp<Tuple1, Tuple2, OpAdd>{};
template<class Tuple1, class Tuple2> struct TupleMaxSub : public TupleMaxOp<Tuple1, Tuple2, OpSub>{};
template<class Tuple1, class Tuple2> struct TupleMaxMul : public TupleMaxOp<Tuple1, Tuple2, OpMul>{};
template<class Tuple1, class Tuple2> struct TupleMaxDiv : public TupleMaxOp<Tuple1, Tuple2, OpDiv>{};
template<class Tuple1, class Tuple2> struct TupleMaxSet : public TupleMaxOp<Tuple1, Tuple2, OpSet>{};

template<class Tuple1, class Tuple2, class SelectedList> struct TupleSelectAdd : public TupleSelectOp<Tuple1, Tuple2, SelectedList, OpAdd>{};
template<class Tuple1, class Tuple2, class SelectedList> struct TupleSelectSub : public TupleSelectOp<Tuple1, Tuple2, SelectedList, OpSub>{};
template<class Tuple1, class Tuple2, class SelectedList> struct TupleSelectMul : public TupleSelectOp<Tuple1, Tuple2, SelectedList, OpMul>{};
template<class Tuple1, class Tuple2, class SelectedList> struct TupleSelectDiv : public TupleSelectOp<Tuple1, Tuple2, SelectedList, OpDiv>{};
template<class Tuple1, class Tuple2, class SelectedList> struct TupleSelectSet : public TupleSelectOp<Tuple1, Tuple2, SelectedList, OpSet>{};

//
//
//
template<class Tuple, int size>
struct TupleFuncImpl
{
	typedef TupleFuncImpl<typename Tuple::ChildTuple, size - 1> SubImpl;

	template<class Func, class... Args> static inline void exec(Tuple& t1, Func&& func, Args&... args) 
	{ 
		func(t1.field, args...); 
		SubImpl::exec(t1.tuple, std::forward<Func>(func), args...);
	}

	template<class Func, class... Args> static inline void exec(const Tuple& t1, Func&& func, Args&... args) 
	{ 
		func(t1.field, args...); 
		SubImpl::exec(t1.tuple, std::forward<Func>(func), args...);
	}
};

template<class Tuple>
struct TupleFuncImpl<Tuple, 1>
{
	template<class Func, class... Args> static inline void exec(Tuple& t1, Func&& func, Args&... args) 
	{ 
		func(t1.field, args...);
	}

	template<class Func, class... Args> static inline void exec(const Tuple& t1, Func&& func, Args&... args) 
	{ 
		func(t1.field, args...); 
	}
};

struct TupleFunc
{
	template<class Tuple, class Func, class... Args> static inline void exec(Tuple& t1, Func&& func, Args&... args)			{ TupleFuncImpl<Tuple, Tuple::size>::exec(t1, std::forward<Func>(func), args...); }
	template<class Tuple, class Func, class... Args> static inline void exec(const Tuple& t1, Func&& func, Args&... args)	{ TupleFuncImpl<Tuple, Tuple::size>::exec(t1, std::forward<Func>(func), args...); }
};

}