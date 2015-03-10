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

// fold aka accumulate
#include <numeric>

#include <boost/optional.hpp>

#include <QVariant>


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

// (>>=) :: Cont a -> (a -> Cont b) -> Cont b
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
// Monad instance for boost::optional:
//


// pure :: a -> Maybe a
template<template <typename> class M , typename A>
inline typename std::enable_if<std::is_same<M<A>, boost::optional<A>>::value, boost::optional<A>>::type // required for return type dispatching
pure(A x)
{
	return boost::optional<A>(x);
}

template<typename A, typename B>
boost::optional<B> operator >>= (boost::optional<A> s, std::function<boost::optional<B>(A)> f)
{
	if (s) {
		return f(s.get());
	} else {
		return boost::optional<B>();
	}
}

template <class T>
inline QDebug operator<<(QDebug debug, const boost::optional<T> &m)
{
	if (m) {
		debug.nospace() << "boost::optional(" << m.get() << ")";
	} else {
		debug.nospace() << "Nothing()";
	}
	return debug.maybeSpace();
}


// ---------------------------------


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
template<template <typename> class M, typename A, typename B>
M<B> operator >> (M<A> m, std::function<M<B>()> k)
{
	return (m >>= std::function<M<B>(A)>([k](A) -> M<B> {
		return k();
	}));
}


// sequence :: (Monad m, Traversable t) => t (m a) -> m (t a)
template<template <typename> class M, template <typename> class T, typename A>
M<T<A>> sequence(T<M<A>> x)
{
//	sequence ms = foldr k (return []) ms
//	            where
//	              k m mm = do { x <- m; xs <- mm; return (x:xs) }
	return std::accumulate(std::begin(x), std::end(x), pure<M, T<A>>(T<A>()),
		[](const M<T<A>>& mm, M<A> m) -> M<T<A>>{
			return ( m >>= std::function<M<T<A>>(A)>   ([mm](A x)     -> M<T<A>> {
			return (mm >>= std::function<M<T<A>>(T<A>)>([x ](T<A> xs) -> M<T<A>> {
				xs.push_back(x);
			return pure<M,T<A>>(xs);
			}));}));});
}

// Promote a function to a monad.
// liftM   :: (Monad m) => (a1 -> r) -> m a1 -> m r
template<template <typename> class M, typename A, typename R>
M<R> liftM(std::function<R(A)> f, M<A> m1)
{
	// liftM f m1              = do { x1 <- m1; return (f x1) }
	return m1 >>= std::function<M<R>(A)>([f](A x1) -> M<R>{
	return pure<M,R>(f(x1));
	});
}

// promote a function to a traversable
template<template <typename> class T, class A, class B>
T<B> fmap(std::function<B(A)> f, T<A> as) {
	T<B> bs;
	std::transform(std::begin(as), std::end(as), std::back_inserter(bs), f);
	return bs;
}

// mapM :: Monad m => (a -> m b) -> [a] -> m [b]
template<template <typename> class M, template <typename> class T, typename A, typename B>
M<T<B>> mapM(std::function<M<B>(A)> f, T<A> as)
{
	// mapM f as       =  sequence (map f as)
	return sequence<M,T,B>(fmap<T,A,M<B>>(f, as));
}




#endif // CONT_H
