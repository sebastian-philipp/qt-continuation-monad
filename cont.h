/**************************************************
**
** Copyright (c) 2015 TeamDrive Systems GmbH
**
** All rights reserved.
**
***************************************************/
#ifndef CONT_H
#define CONT_H

#include <functional>
#include <QVariant>


//join              :: (Monad m) => m (m a) -> m a
//join x            =  x >>= id

template <template <typename> class M, typename A>
M<A> join(const M<M<A>> x)
{
	return x >>= std::function<M<A>(M<A>)>([](M<A> y) -> M<A>{
		return y;
	});
}


// (>>) :: forall a b. m a -> m b -> m b
// m >> k = m >>= \_ -> k
// If you get this error here:
// "note: candidate template ignored: substitution failure : template template argument
//   has different template parameters than its corresponding template template parameter"
// then you need to define your own operator >>
template<template <typename> class M, typename A, typename B>
M<B> operator >> (M<A> m, std::function<M<B>()> k)
{
	return (m >>= std::function<M<B>(A)>([k](A) -> M<B> {
		return k();
	}));
}


//
// Continuation monad:
//


// data Cont r a = Cont { runCont :: (a -> r) -> r }
template<typename A>
struct Cont {
	typedef std::function<QVariant(A)> Inner;
	typedef std::function<QVariant(Inner)> Type;
	Type m_Cont;
	Cont(const Type& func);

	Cont(); // dont use. Requires default constructor for A

	Type runCont() const;
	QVariant runCont1(Inner f) const;
	QVariant evalCont() const;
};

//cont :: ((a -> r) -> r) -> Cont r a
template<typename A>
Cont<A>::Cont(const typename Cont<A>::Type &func):
	m_Cont(func)
{
}

// :: Cont r a
template<typename A>
Cont<A>::Cont():
	m_Cont(pure(A()).m_Cont)
{
}


//runCont :: Cont r a -> (a -> r) -> r
template<typename A>
typename Cont<A>::Type Cont<A>::runCont() const
{
	return m_Cont;
}

//runCont1 = uncurry runCont
template<typename A>
QVariant Cont<A>::runCont1(typename Cont<A>::Inner f) const
{
	return runCont()(f);
}

// evalCont :: Cont r a -> r
template<typename A>
QVariant Cont<A>::evalCont() const
{
	return runCont1([](A a) -> QVariant {
		return QVariant::fromValue<A>(a);
	});
}

//instance Monad (Cont r) where
//    return x = cont ($ x)
//    s >>= f  = cont $ \c -> runCont s $ \x -> runCont (f x) c
template<template <typename> class M , typename A>
inline typename std::enable_if<std::is_same<M<A>, Cont<A>>::value, Cont<A>>::type // required for return type dispatching
pure(A x)
{
	return Cont<A>([x](typename Cont<A>::Inner f) -> QVariant {
		return f(x);
	});
}

template<typename A, typename B>
Cont<B> operator >>= (Cont<A> s, std::function<Cont<B>(A)> f)
{
	return Cont<B>([f, s](std::function<QVariant(B)> c) -> QVariant {
		return s.runCont1([f, c](A x) -> QVariant {
			return f(x).runCont1(c);
		});
	});
}

template<typename A>
Cont<A> abortContWith(QVariant r)
{
	return Cont<A>(std::function<QVariant(typename Cont<A>::Inner)>([r](typename Cont<A>::Inner){
		return r;
	}));
}

//
// Maybe monad:
//

// data Cont r a = Cont { runCont :: (a -> r) -> r }
template<typename A>
struct Maybe {

	const QScopedPointer<A> m_Val;

	Maybe(const A& val);
	Maybe(const Maybe& rhs);
	Maybe();

	bool isJust() const;
	const A& fromJust() const;
};

template<typename A>
Maybe<A>::Maybe():
	m_Val()
{

}

template<typename A>
Maybe<A>::Maybe(const A& val):
	m_Val(new A(val))
{

}

template<typename A>
Maybe<A>::Maybe(const Maybe<A>& rhs):
	m_Val(rhs.m_Val ?  new A(*(rhs.m_Val)) : nullptr)
{

}


template<typename A>
bool Maybe<A>::isJust() const
{
	return !m_Val.isNull();
}

template<typename A>
const A& Maybe<A>::fromJust() const
{
	return *m_Val;
}

template<template <typename> class M , typename A>
inline typename std::enable_if<std::is_same<M<A>, Maybe<A>>::value, Maybe<A>>::type // required for return type dispatching
pure(A x)
{
	return Maybe<A>(x);
}

template<typename A, typename B>
Maybe<B> operator >>= (Maybe<A> s, std::function<Maybe<B>(A)> f)
{
	if (s.isJust()) {
		return f(s.fromJust());
	} else {
		return Maybe<B>();
	}
}




#endif // CONT_H
