Haskell continuation monad in C++
=================================

A basic but working implementation of the continuation monad in C++. Can be used by Qt to mitigate the callback hell

Featuring:
* A Monad implementation for ```boost::optional```, ```Continuation``` and some Qt data types.
* Some basic monadic fuctions, like ```sequence```, ```mapM``` and ```liftM```.
* ```callCC``` and ```tryCont``` for some basic exception handling (asynchonous exceptions!).
* A small integraion to wrap Qt signals into a continuation.
* Some examples.

Source
------
Wikibooks: [Continuation passing style][wikibooks]

Supported Platforms
-------------------

VS2012
GCC 4.7
clang 3.4


See also
--------
* Ivan Čukić: Natural task scheduling using futures and continuations: [Youtube][youtube]
* GitHub: [The Functional Template Library][ftl]

[youtube]: https://www.youtube.com/watch?v=LSCamsfwYQU "Ivan Čukić: Natural task scheduling using futures and continuations"
[wikibooks]: http://en.wikibooks.org/wiki/Haskell/Continuation_passing_style "Wikibooks: Continuation passing style"
[ftl]: https://github.com/beark/ftl "GitHub: The Functional Template Library"


