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

// data Cont r a = Cont { runCont :: (a -> r) -> r }
template<typename R, typename A>
struct Cont {
	typedef std::function<R(A)> Inner;
	typedef std::function<R(Inner)> Type;
	Type m_Cont;
	Cont(const Type& func);

	Cont(); // dont use. Requires default constructor for A

	Type runCont() const;
	R runCont1(Inner f) const;
	R evalCont() const;
};

//cont :: ((a -> r) -> r) -> Cont r a
template<typename R, typename A>
Cont<R, A>::Cont(const typename Cont<R, A>::Type &func):
	m_Cont(func)
{
}

// :: Cont r a
template<typename R, typename A>
Cont<R, A>::Cont():
	m_Cont(pure(A()).m_Cont)
{
}


//runCont :: Cont r a -> (a -> r) -> r
template<typename R, typename A>
typename Cont<R, A>::Type Cont<R, A>::runCont() const
{
	return m_Cont;
}

//runCont1 = uncurry runCont
template<typename R, typename A>
R Cont<R, A>::runCont1(typename Cont<R, A>::Inner f) const
{
	return runCont()(f);
}

// evalCont :: Cont r a -> r
template<typename R, typename A>
R Cont<R, A>::evalCont() const
{
	return runCont1([](A a) -> R {
		return a;
	});
}

//instance Monad (Cont r) where
//    return x = cont ($ x)
//    s >>= f  = cont $ \c -> runCont s $ \x -> runCont (f x) c
template<typename R, typename A>
Cont<R, A> pure(A x)
{
	return Cont<R, A>([x](typename Cont<R, A>::Inner f) -> R {
		return f(x);
	});
}

template<typename R, typename A, typename B>
Cont<R, B> operator >>= (Cont<R, A> s, std::function<Cont<R, B>(A)> f)
{
	return Cont<R, B>([f, s](std::function<R(B)> c) -> R {
		return s.runCont1([f, c](A x) -> R {
			return f(x).runCont1(c);
		});
	});
}

// (>>) :: forall a b. m a -> m b -> m b
// m >> k = m >>= \_ -> k
template<typename R, typename A, typename B>
Cont<R, B> operator >> (Cont<R, A> m, std::function<Cont<R, B>()> k)
{
	return (m >>= std::function<Cont<R, B>(A)>([k](A) -> Cont<R, B> {
		return k();
	}));
}

template<typename R, typename A>
Cont<R,A> abortContWith(R r)
{
	return Cont<R,A>(std::function<R(typename Cont<R,A>::Inner)>([r](typename Cont<R,A>::Inner){
		return r;
	}));
}

// template specialization for Cont<void, A>
// necessarry, because void is not a complete type in C++
template<typename A>
struct Cont<void, A>{
	typedef std::function<void(A)> Inner;
	typedef std::function<void(Inner)> Type;
	Type m_Cont;
	Cont(const Type& func);
//private:
	Cont(); // dont use.
//public:
	Type runCont() const;
	void runCont1(Inner f) const;
	void evalCont() const;

};

//cont :: ((a -> ()) -> ()) -> Cont () a
template<typename A>
Cont<void, A>::Cont(const typename Cont<void, A>::Type &func):
	m_Cont(func)
{
}

// :: Cont () a
template<typename A>
Cont<void, A>::Cont():
	m_Cont(pure(A()).m_Cont)
{
}

//runCont :: Cont () a -> (a -> ()) -> ()
template<typename A>
typename Cont<void, A>::Type Cont<void, A>::runCont() const
{
	return m_Cont;
}

//runCont1 = uncurry runCont
template<typename A>
void Cont<void, A>::runCont1(typename Cont<void, A>::Inner f) const
{
	runCont()(f);
}

template<typename A>
void Cont<void, A>::evalCont() const
{
	runCont1([](A){});
}

//instance Monad (Cont r) where
//    return x = cont ($ x)
//    s >>= f  = cont $ \c -> runCont s $ \x -> runCont (f x) c
template<typename A>
Cont<void, A> pure(A x)
{
	return Cont<void, A>([x](typename Cont<void, A>::Inner f) -> void {
		f(x);
	});
}


template<typename A, typename B>
Cont<void, B> operator >>= (Cont<void, A> s, std::function<Cont<void, B>(A)> f)
{
	return Cont<void, B>([f, s](std::function<void(B)> c) -> void {
		s.runCont1([f, c](A x) -> void {
			f(x).runCont1(c);
		});
	});
}

// (>>) :: forall a b. m a -> m b -> m b
// m >> k = m >>= \_ -> k
template<typename A, typename B>
Cont<void, B> operator >> (Cont<void, A> m, std::function<Cont<void, B>()> k)
{
	return (m >>= std::function<Cont<void, B>(A)>([k](A) -> Cont<void, B> {
		return k();
	}));
}

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

#endif // CONT_H
