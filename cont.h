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


// decltype call expression

#if __GNUC__ > 4 || \
	(__GNUC__ == 4 && (__GNUC_MINOR__ > 8 || \
		(__GNUC_MINOR__ == 8 && __GNUC_PATCHLEVEL__ > 1)))
#define HAS_DECLTYPE_CALLEXPR
#endif


#if __clang_major__ > 3 || \
	(__clang_major__ == 3 && (__clang_minor__ >= 4))
#define HAS_DECLTYPE_CALLEXPR
#endif

#if _MSC_VER >= 1600
#define HAS_DECLTYPE_CALLEXPR
#endif


// Functor class

template<template <typename> class F, typename B>
struct Functor {
	typedef B ValueT;

	template<typename A>
	static F<B> fmap(std::function<B(A)> f, F<A> s);
};

template<template <typename> class F, typename A, typename B>
F<B> fmap(std::function<B(A)> f, F<A> s)
{
	return Functor<F,B>::fmap(f, s);
}

#ifdef HAS_DECLTYPE_CALLEXPR
template<template <typename> class M, typename A, typename F>
auto fmap (M<A> s, F f) -> M<decltype(f(A()))>
{
	return fmap(std::function<decltype(f(A()))(A)>(f), s);
}
#endif

// Monad class

template<template <typename> class M, typename B>
struct Monad {
	typedef B ValueT;

	static M<B> pure(B);

	template<typename A>
	static M<B> bind(M<A>, std::function<M<B>(A)>);
};

template<template <typename> class M, typename A>
M<A> pure(A x)
{
	return Monad<M,A>::pure(x);
}

template<template <typename> class M, typename A, typename B>
M<B> operator >>= (M<A> s, std::function<M<B>(A)> f)
{
	return Monad<M,B>::bind(s, f);
}

#ifdef HAS_DECLTYPE_CALLEXPR
template<template <typename> class M, typename A, typename F>
auto operator >>= (M<A> s, F f) -> decltype(f(A()))
{
	return s >>= std::function<decltype(f(A()))(A)>(f);
}
#endif


//join   :: (Monad m) => m (m a) -> m a
//join x =  x >>= id
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

#ifdef HAS_DECLTYPE_CALLEXPR
template<template <typename> class M, typename A, typename F>
auto operator >> (M<A> m, F f) -> decltype(f())
{
	return m >> std::function<decltype(f())()>(f);
}
#endif

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
				xs += x;
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

// mapM :: (Monad m, Traversable t) => (a -> m b) -> t a -> m t b
template<template <typename> class M, template <typename> class T, typename A, typename B>
M<T<B>> mapM(std::function<M<B>(A)> f, T<A> as)
{
	// mapM f as       =  sequence (map f as)
	return sequence<M,T,B>(fmap<T,A,M<B>>(f, as));
}

//
// Continuation monad:
//

#include <QVariant>

// data Cont r a = Cont { runCont :: (a -> r) -> r }
template<typename A>
struct Cont {
	typedef std::function<QVariant(A)> Inner;
	typedef std::function<QVariant(Inner)> Type;
	Type m_Cont;
	Cont(const Type& func);

	Cont(); // dont use. Requires default constructor for A
	bool operator == (const Cont<A>& rhs) const {return this == &rhs;}

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
template<typename B>
struct Monad<Cont, B>
{
	//    return x = cont ($ x)
	//    s >>= f  = cont $ \c -> runCont s $ \x -> runCont (f x) c
	static Cont<B> pure(B x)
	{
		return Cont<B>([x](typename Cont<B>::Inner f) -> QVariant {
			return f(x);
		});
	}

	// (>>=) :: Cont a -> (a -> Cont b) -> Cont b
	template<typename A>
	static Cont<B> bind(Cont<A> s, std::function<Cont<B>(A)> f)
	{
		return Cont<B>([f, s](std::function<QVariant(B)> c) -> QVariant {
			return s.runCont1([f, c](A x) -> QVariant {
				return f(x).runCont1(c);
			});
		});
	}
};

template<typename A>
Cont<A> abortContWith(QVariant r)
{
	return Cont<A>(std::function<QVariant(typename Cont<A>::Inner)>([r](typename Cont<A>::Inner){
		return r;
	}));
}

#include <QHash>

template<typename T>
inline uint qHash(const Cont<T> &c, uint seed = 0)
//	Q_DECL_NOEXCEPT_EXPR(noexcept(qHash(c)))
{
	return qHash((quintptr)seed ^ reinterpret_cast<quintptr>(&c));
}


//
// Monad instance for boost::optional:
//

#include <boost/optional.hpp>

template<typename B>
struct Monad<boost::optional, B>
{
	// pure :: a -> Maybe a
	static boost::optional<B> pure(B x)
	{
		return boost::optional<B>(x);
	}

	template<typename A>
	static boost::optional<B> bind(boost::optional<A> s, std::function<boost::optional<B>(A)> f)
	{
		if (s) {
			return f(s.get());
		} else {
			return boost::optional<B>();
		}
	}
};


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

//
// Monad instance for QSet:
//

#include <QSet>

template<typename T>
inline uint qHash(const QSet<T> &s, uint seed);

// functor instance
template<typename B>
struct Functor<QSet, B>
{
	template<typename A>
	static QSet<B> fmap(std::function<B(A)> f, QSet<A> as)
	{
		QSet<B> bs;
		foreach(const A& a, as) {
			bs.insert(f(a));
		}
		return bs;
	}
};

template<typename B>
struct Monad<QSet, B>
{
	// pure :: a -> QSet a
	static QSet<B> pure(B x)
	{
		return QSet<B>() << x;
	}

	template<typename A>
	static QSet<B> bind(QSet<A> s, std::function<QSet<B>(A)> f)
	{
		QSet<B> ret;
		foreach (const QSet<B>& ss, fmap(f, s)) {
			ret.unite(ss);
		}
		return ret;
	}
};


//
// Monad instance for QList:
//

#include <QList>

template<typename T>
inline uint qHash(const QList<T> &s, uint seed)
//	Q_DECL_NOEXCEPT_EXPR(noexcept(qHash(s)))
{
	uint hash = seed;
	foreach(const T& t, s){
		return (hash = hash ^ qHash(t, seed));
	}
	return hash;
}


// functor instance
template<typename B>
struct Functor<QList, B>
{
	template<typename A>
	static QList<B> fmap(std::function<B(A)> f, QList<A> as)
	{
		QList<B> bs;
		std::transform(std::begin(as), std::end(as), std::back_inserter(bs), f);
		return bs;
	}
};

template<typename B>
struct Monad<QList, B>
{
	// pure :: a -> [a]
	static QList<B> pure(B x)
	{
		return QList<B>() << x;
	}

	template<typename A>
	static QList<B> bind(QList<A> s, std::function<QList<B>(A)> f)
	{
		QList<B> ret;
		foreach (const QList<B>& ss, fmap(f, s)) {
			ret.append(ss);
		}
		return ret;
	}
};


// ---------------------------------


template<typename T>
inline uint qHash(const QSet<T> &s, uint seed)
//	Q_DECL_NOEXCEPT_EXPR(noexcept(qHash(s)))
{
	uint hash = seed;
	foreach(const T& t, s){
		return (hash = hash ^ qHash(t, seed));
	}
	return hash;
}

// ---------------------------

struct Unit {};
Q_DECLARE_METATYPE(Unit)

typedef Cont<int> IntCont;
typedef Cont<Unit> VoidCont;


template<typename Q, typename S>
VoidCont waitForQObjectSignal0(Q* obj, S signal)
{
	return Cont<R>([obj, signal](VoidCont::Inner inner){
			QObject* guard = new QObject();
			QObject::connect(obj, signal, guard, [inner]() -> void {
				delete guard;
				inner(Unit());
			});
	});
}

template<typename Q, typename S, typename R>
Cont<R> waitForQObjectSignal1(Q* obj, S signal)
{
	return Cont<R>([obj, signal](Cont<R>::Inner inner){
			QObject* guard = new QObject();
			QObject::connect(obj, signal, guard, [inner](R r) -> void {
				delete guard;
				inner(r);
			});
	});
}



#endif // CONT_H
