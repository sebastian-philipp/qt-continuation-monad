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

#include <QtGlobal>


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

// the empty struct. reason, void can be complicated as template parameter.
struct Unit {};

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
// Current limitation: you can return somehing from the monad. Currently, r equals void.

// data Cont r a = Cont { runCont :: (a -> r) -> r }
template<typename A>
struct Cont {
	typedef std::function<void(A)> Inner;
	typedef std::function<void(Inner)> Type;
	Type m_Cont;
	Cont(const Type& func);

	Cont(); // dont use. Requires default constructor for A
	bool operator == (const Cont<A>& rhs) const {return this == &rhs;}

	inline Q_CONSTEXPR Type runCont() const;

	template <typename F>
	inline void runCont1(F f) const;

	void evalCont() const;
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
	m_Cont([](typename Cont<A>::Inner f) -> void { f(A()); })
{
}

//runCont :: Cont r a -> (a -> r) -> r
template<typename A>
inline Q_CONSTEXPR typename Cont<A>::Type Cont<A>::runCont() const
{
	return m_Cont;
}

//runCont1 = uncurry runCont
//runCont1 :: (Cont r a, (a -> r)) -> r
template<typename A>
template<typename F>
inline void Cont<A>::runCont1(F f) const
{
	runCont()(f);
}

/*!
  evalCont :: Cont r a -> r

  the return value is typically empty.
*/
template<typename A>
void Cont<A>::evalCont() const
{
	runCont1([](A) -> void {
	});
}

//instance Monad (Cont r) where
template<typename B>
struct Monad<Cont, B>
{
	//    return x = cont ($ x)
	//    s >>= f  = cont $ \c -> runCont s $ \x -> runCont (f x) c
	static inline Q_CONSTEXPR Cont<B> pure(B x)
	{
		return Cont<B>([x](typename Cont<B>::Inner f) -> void  {
			f(x);
		});
	}

	// (>>=) :: Cont a -> (a -> Cont b) -> Cont b
	template<typename A>
	static inline Q_CONSTEXPR Cont<B> bind(Cont<A> s, std::function<Cont<B>(A)> f)
	{
		return Cont<B>([f, s](std::function<void(B)> c) -> void {
			s.runCont1([f, c](A x) -> void {
				f(x).runCont1(c);
			});
		});
	}
};

template<typename A>
Cont<A> abortContWith()
{
	return Cont<A>(std::function<void(typename Cont<A>::Inner)>([](typename Cont<A>::Inner){
	}));
}

// callCC :: ((a -> Cont r b) -> Cont r a) -> Cont r a
// callCC f = cont $ \h -> runCont (f (\a -> cont $ \_ -> h a)) h

template<typename A, typename B, typename F>
Cont<A> callCC(F /*std::function<Cont<A>(std::function<Cont<B>(A)>)>*/ f)
{
	return Cont<A>(
		// \h -> runCont (f (\a -> cont $ \_ -> h a)) h
		[f](typename Cont<A>::Inner h) -> void {
			// (f (\a -> cont $ \_ -> h a))
			f(std::function<Cont<B>(A)>([h](A a) -> Cont<B> {
				return Cont<B>([h, a](typename Cont<B>::Inner) -> void {
					h(a);
				});
			})).runCont1(h);
		}
	);
}

//tryCont :: MonadCont m => ((err -> m a) -> m a) -> (err -> m a) -> m a
//tryCont c h = callCC $ \ok -> do
//    err <- callCC $ \notOk -> do
//        x <- c notOk
//        ok x
//    h err

template<typename A, typename ERR>
Cont<A> tryCont_(/*C*/ std::function<Cont<A>(std::function<Cont<A>(ERR)>)> c, std::function<Cont<A>(ERR)> h)
{
	return callCC<A, ERR>([c, h](std::function<Cont<ERR>(A)> ok) -> Cont<A> {
		return callCC<ERR, A>([ok, c](std::function<Cont<A>(ERR)> notOk) -> Cont<ERR> {
			return c(notOk) >>= [ok](A x) -> Cont<ERR> {
				return ok(x);
			};
		}) >>= [h](ERR error) -> Cont<A> {
		return h(error);
		};
	});
}

template<typename A, typename ERR, typename C, typename H>
Cont<A> tryCont(C c, H h)
{
	return tryCont_<A, ERR>(std::function<Cont<A>(std::function<Cont<A>(ERR)>)>(c), std::function<Cont<A>(ERR)>(h));
}


#include <QHash>

template<typename T>
inline uint qHash(const Cont<T> &c, uint seed = 0)
//	Q_DECL_NOEXCEPT_EXPR(noexcept(qHash(c)))
{
	return qHash((quintptr)seed ^ reinterpret_cast<quintptr>(&c));
}

//
// Async is a Cont + Reader Monad
//

struct AsyncException {
	virtual ~AsyncException(){}
};

// newtype ReaderT r m a = ReaderT { runReaderT :: r -> m a }
// specialized for
// r == ThrowF
// m == Cont
template<typename A>
struct Async
{
	typedef std::function<Cont<A>(AsyncException*)> ThrowF; // ThrowF aka R
	typedef std::function<Cont<A>(ThrowF)> Type;

	Async(const Type& func);
	Async(); // dont use. Requires default constructor for A

	Type runAsync() const;
	Cont<A> runAsync1(const ThrowF& throwF) const;

//	bool operator == (const Cont<A>& rhs) const {return this == &rhs;}
	Async<ThrowF> ask();


	static Async<A> raise(AsyncException* e);

	template<typename H>
	Cont<A> tryAsync(H handler);


	Type m_Cont;

//	void evalCont() const;
};

template<typename A, typename L>
Async<A> lambdaToAsync(L l)
{
	return Async<A>(typename Async<A>::Type(l));
}

template<typename A>
Async<A> contToAsync(Cont<A> c)
{
	return lambdaToAsync<A>([c](typename Async<A>::ThrowF){return c;});
}


template<typename A>
Async<A>::Async(const typename Async<A>::Type& func):
	m_Cont(func)
{

}

template<typename A>
Async<A>::Async():
	m_Cont()
{

}

template<typename A>
typename Async<A>::Type Async<A>::runAsync() const
{
	return m_Cont;
}

template<typename A>
Cont<A> Async<A>::runAsync1(const ThrowF& throwF) const
{
	return runAsync()(throwF);
}

//instance (Monad m) => Monad (ReaderT r m) where
//    return a = ReaderT $ \_ -> return a
//    s >>= f  = ReaderT $ \r -> do
//        a <- runReaderT s r
//        runReaderT (f a) r
// specialized for
// r == ThrowF
// m == Cont
template<typename B>
struct Monad<Async, B>
{
	static inline Q_CONSTEXPR Async<B> pure(B x)
	{
		return contToAsync<B>(Cont<B>([x](typename Cont<B>::Inner f) -> void  {
			f(x);
		}));
//		return contToAsync<B>(pure<Cont>(x));
	}

	// (>>=) :: Async a -> (a -> Async b) -> Async b
	template<typename A>
	static inline Q_CONSTEXPR Async<B> bind(Async<A> s, std::function<Async<B>(A)> f)
	{
		return Async<B>([s, f](typename Async<B>::ThrowF throwF) -> Cont<B>{
			return s.runAsync1(throwF) >>= [f, throwF](A a){
			return f(a).runAsync1(throwF);
			};
		});
	}
};

// ask = ReaderT $ \x -> return x
template<typename A>
Async<typename Async<A>::ThrowF> Async<A>::ask()
{
	return lambdaToAsync<ThrowF>([](Async<A>::ThrowF f){
		return pure<Async>(f);
	});
}

//template<typename A, typename ERR>
//Cont<A> tryCont_(/*C*/ std::function<Cont<A>(std::function<Cont<A>(ERR)>)> c, std::function<Cont<A>(ERR)> h)

template<typename A>
Cont<A> tryAsync_(Async<A> block, typename Async<A>::ThrowF handler)
{
	return tryCont<A, AsyncException*>([block](typename Async<A>::ThrowF throwF) -> Cont<A> {
			return block.runAsync1(throwF);
		}
		,
		handler
	);
}

template<typename A>
Async<A> Async<A>::raise(AsyncException* e)
{
	return lambdaToAsync<A>([e](Async<A>::ThrowF throwF) {
		return throwF(e);
	});
}

template<typename A>
template<typename H>
Cont<A> Async<A>::tryAsync(H handler)
{
	return tryAsync_(*this, typename Async<A>::ThrowF(handler));
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

Q_DECLARE_METATYPE(Unit)

typedef Cont<int> IntCont;
typedef Cont<Unit> VoidCont;


template<typename Q, typename S>
VoidCont waitForQObjectSignal0(Q* obj, S signal)
{
	return VoidCont([obj, signal](VoidCont::Inner inner){
		QObject* guard = new QObject();
		QObject::connect(obj, signal, guard, [guard, inner]() -> void {
			delete guard;
			inner(Unit());
		});
	});
}

template<typename R, typename Q, typename S>
Cont<R> waitForQObjectSignal1(Q* obj, S signal, QObject* parent = 0)
{
	return Cont<R>([obj, signal, parent](typename Cont<R>::Inner inner) -> void {
		QObject* guard = new QObject(parent);
		QObject::connect(obj, signal, guard, [guard, inner](R r) -> void {
			delete guard;
			inner(r);
		});
	});
}

template<typename R, typename Q, typename S, typename C>
Cont<R> waitForQObjectSignal1c(Q* obj, S signal, C execAfter, QObject* parent = 0)
{
	std::function<void(void)> execAfter_(execAfter);
	return Cont<R>([obj, signal, parent, execAfter_](typename Cont<R>::Inner inner) -> void {
		QObject* guard = new QObject(parent);
		QObject::connect(obj, signal, guard, [guard, inner](R r) -> void {
			delete guard;
			inner(r);
		});
		execAfter_();
	});
}


#include <QEventLoop>

template<typename R>
R evalWithEventLoop(Cont<R> cont)
{
	QEventLoop loop;
	bool isSet = false;
	R result;
	(
		cont >>= [&result, &loop, &isSet](R r) -> Cont<Unit>{
			result = r;
			isSet = true;
			loop.exit();
			return pure<Cont>(Unit());
		}
	).evalCont();
	if (!isSet) {
		loop.exec();
	}
	return result;
}

#endif // CONT_H
