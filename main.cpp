/**************************************************
**
** Copyright (c) 2015 TeamDrive Systems GmbH
**
** See the file LICENSE.txt for copying permission.
**
***************************************************/
#include <QCoreApplication>
#include <QDebug>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QSet>
#include <QHash>
#include <QList>



#include <htmlcxx/html/ParserSax.h>


#include "cont.h"

typedef Cont<QSet<QUrl>> UrlContinuation;
typedef Cont<QNetworkReply*> ReplyContinuatoin;

void testBasics();
void testOptional();
void testSequence();
void testGrabLinks(QCoreApplication& a);
void testMapM();
void testLoop();
void testCallCC();
void testException();


ReplyContinuatoin waitForReplyFinished(QNetworkReply *rep);
UrlContinuation grabLinks(const QUrl& url, int depth);
QSet<QUrl> scrapUrls(const QUrl& base, QIODevice* dev);

QNetworkAccessManager *nam = 0;

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	// Continuation monad
	nam = new QNetworkAccessManager();

//	testBasics();
//	testOptional();
//	testSequence();

//	testGrabLinks(a);

//	testMapM();

	//testLoop();
//	testCallCC();
	testException();
	return 0;
//	return a.exec();
}


UrlContinuation grabLinks(const QUrl& url, int depth)
{
	if (depth <= 0)
		return pure<Cont>(QSet<QUrl>() << url);
	return waitForReplyFinished(nam->get(QNetworkRequest(url))) >>= [url, depth](QNetworkReply* rep) -> UrlContinuation {
		QSet<QUrl> urls = scrapUrls(url, rep);
	return mapM<Cont, QSet, QUrl, QSet<QUrl>>([depth](QUrl u){
//				qDebug() << "grabLinks(u, depth-1)" << u;
				return grabLinks(u, depth-1);
			}, urls) >>=[](QSet<QSet<QUrl>> uurls){
	return pure<Cont>(join<QSet>(uurls));
	};};
}


QSet<QUrl> scrapUrls(const QUrl& base, QIODevice* dev)
{
	using namespace htmlcxx::HTML;
	struct Handler: ParserSax {
		QSet<QUrl>& m_Urls;
		const QUrl& m_Base;
		Handler(const QUrl& b, QSet<QUrl>& u): m_Urls(u), m_Base(b){}

		void foundTag(Node node, bool) {
			if (node.tagName() == "a") {
				node.parseAttributes();
//				qDebug() << "Read Start Tag : " << QString::fromStdString(node.tagName()) << QString::fromStdString(node.attribute("href").second);
				auto a =  node.attribute("href").second;
				if (a != ""){
					m_Urls << m_Base.resolved(QUrl(QString::fromStdString(a)));
				}
			}
		}
	};
	QSet<QUrl> urls;
	Handler xml(base, urls);
	xml.parse(QString::fromUtf8(dev->readAll()).toStdString());
	return urls;
}

ReplyContinuatoin waitForReplyFinished(QNetworkReply *rep)
{
	return ReplyContinuatoin (
		[rep](ReplyContinuatoin::Inner inner) -> void {
			if (rep->isFinished()) {
				inner(rep);
			}
			QObject* guard = new QObject();
			qDebug() << rep->request().url();
			QObject::connect(rep, &QNetworkReply::finished, guard, [guard, inner, rep]() -> void {
//				qDebug() << "waitForReplyFinished";
				delete guard;
				rep->deleteLater();
				inner(rep);
			});
			QTimer::singleShot(10000, guard, [guard, rep](){
				qDebug() << "Killing request after 10s" << rep->request().url();
				rep->abort();
			});
		}
	);
}

void testBasics()
{
	IntCont myCont = pure<Cont>(1) >>= [](int i) {
		return pure<Cont>(i + 1);
	};
	qDebug() << ">>=" << evalWithEventLoop(myCont);

	qDebug() << "join" << evalWithEventLoop(join<Cont>(pure<Cont>(pure<Cont>(1))));

	IntCont myCont2 = (pure<Cont>(1)) >> []() -> IntCont {
		return pure<Cont>(42);
	};
	qDebug() << ">>" << evalWithEventLoop(myCont2);
}

void testOptional()
{
	// Maybe Monad
	qDebug() << pure<boost::optional>(42);
	qDebug() << pure<boost::optional>(42);

	qDebug() << (pure<boost::optional>(42) >>= [](int i) -> boost::optional<int> {return boost::optional<int>(i);});
	qDebug() << (pure<boost::optional>(42) >> []() -> boost::optional<int> {return boost::optional<int>();});
	qDebug() << join<boost::optional>(pure<boost::optional>(pure<boost::optional>(42))).get();
}

void testSequence()
{
	// sequence
	qDebug() << sequence<boost::optional, QList>(QList<boost::optional<int>>{ boost::optional<int>(3), boost::optional<int>(4) , boost::optional<int>(5) });
	qDebug() << sequence<boost::optional, QList>(QList<boost::optional<int>>{ boost::optional<int>(3), boost::optional<int>() , boost::optional<int>(5) });
	// qDebug() << sequence<boost::optional, std::vector, int>(std::vector<boost::optional<int>> { boost::optional<int>(3), boost::optional<int>(4) , boost::optional<int>(5) }); ?

}

void testGrabLinks(QCoreApplication& a)
{
	(
		grabLinks(QUrl("http://www.teamdrive.com/"), 2) >>= [&a](QSet<QUrl> urls) -> VoidCont {
		qDebug() << "urls.len():" << urls;
		a.exit();
		return pure<Cont>(Unit());
	}).evalCont();
}

void testMapM()
{	// mapM
	qDebug() << mapM<boost::optional, QList, int, QString>([](int i){
		return boost::optional<QString>(QString::number(i * 10, 16));
	},QList<int>{1,2,3,4,5,6,7,8,9});
	qDebug() << mapM<QList, QList, int, int>([](int i){
		return QList<int>() << i << i + 1;
	},QList<int>{0,0,0,0,0,0,0});
}

Cont<int> recContCount(int i)
{
	if (i > 0) {
		return pure<Cont>(Unit()) >> [i](){
		return recContCount(i - 1) >>= [](int i){
		return pure<Cont>(i + 1);
		};};
	} else {
		return pure<Cont>(0);
	}
}

void testLoop()
{
	int res = evalWithEventLoop(recContCount(10));
	qDebug() << res;
}

void testCallCC()
{
//	quux :: Cont r Int
//	quux = callCC $ \k -> do
//	    let n = 5
//	    k n
//	    return 25
	(callCC<int, int>([](std::function<Cont<int>(int)> k){
		return k(5);
		return pure<Cont>(25);
	}) >>= [](int i){
		qDebug() << "5 =" << i;
		return pure<Cont>(i);
	}).evalCont();


//	data SqrtException = LessThanZero deriving (Show, Eq)

//	sqrtIO :: (SqrtException -> ContT r IO ()) -> ContT r IO ()
//	sqrtIO throw = do
//	    ln <- lift (putStr "Enter a number to sqrt: " >> readLn)
//	    when (ln < 0) (throw LessThanZero)
//	    lift $ print (sqrt ln)

//	main = runContT (tryCont sqrtIO (lift . print)) return
	enum SqrtException {LessThanZero};

	std::function<Cont<int>(std::function<Cont<int>(SqrtException)>)> sqrtIO ([](std::function<Cont<int>(SqrtException)> throwF) -> Cont<int> {
		return pure<Cont>(0) >>= [throwF](int i) -> Cont<int> {
			if (i <= 0)
				return throwF(LessThanZero);
		return pure<Cont>(i) >>= [](int i) -> Cont<int> {
			qDebug() << "i = " << i;
		return pure<Cont>(i);
	};};});

	tryCont<int, SqrtException>(sqrtIO, std::function<Cont<int>(SqrtException)>([](SqrtException e) -> Cont<int> {
		qDebug() << "e";
		return pure<Cont>(0);
	})).evalCont();


}

void testException()
{
	Async<Unit>::raise(new AsyncException()
	).tryAsync(
		[](AsyncException*) -> Cont<Unit>{
			qDebug("ok 1");
			return pure<Cont>(Unit());
		}
	).evalCont();

	(
		pure<Async>(Unit()) >> [](){
		return Async<Unit>::raise(new AsyncException()) >> [](){
		return pure<Async>(Unit()) >> [](){
			qDebug("error");
		return pure<Async>(Unit());
		};};}
	).tryAsync([](AsyncException*){
		qDebug("ok 2");
		return pure<Cont>(Unit());
	}).evalCont();

#if 0
	(
		(
			Async<Unit>::raise(new AsyncException()
		).tryAsync([](AsyncException*){
			qDebug("ok 3");
			return pure<Cont>(Unit());
		})
	).tryAsync([](AsyncException*){
		qDebug("error");
		return pure<Cont>(Unit());
	})).evalCont();


	(
		(
			Async<Unit>::raise(new AsyncException())
		).tryAsync([](AsyncException*){
			qDebug("ok 4");
			return Async<Unit>::raise(new AsyncException());
		})
	).tryAsync([](AsyncException*){
		qDebug("ok 5");
		return pure<Cont>(Unit());
	}).evalCont();

	(
		Async<Unit>::raise(new AsyncException()) >> [](){
		return (
			Async<Unit>::raise(new AsyncException())
		).tryAsync([](AsyncException*){
				qDebug("error!");
				return Async<Unit>::raise(new AsyncException());
		});}
	).tryAsync([](AsyncException*){
		qDebug("ok 6");
		return pure<Cont>(Unit());
	}).evalCont();

	(
		(
			pure<Async>(Unit())
		).tryAsync([](AsyncException*){
			qDebug("error!");
			return Async<Unit>::raise(new AsyncException());
		}) >> [](){
		return Async<Unit>::raise(new AsyncException());
		}
	).tryAsync([](AsyncException*){
		qDebug("ok 7/7");
		return pure<Cont>(Unit());
	}).evalCont();
#endif

}


