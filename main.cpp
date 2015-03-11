/**************************************************
**
** Copyright (c) 2015 TeamDrive Systems GmbH
**
** All rights reserved.
**
***************************************************/
#include <QCoreApplication>
#include <QDebug>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include <htmlcxx/html/ParserSax.h>


#include "cont.h"

struct Unit {};
Q_DECLARE_METATYPE(Unit)

typedef Cont<int> IntCont;
typedef Cont<Unit> VoidCont;

typedef Cont<QSet<QUrl>> UrlContinuation;
typedef Cont<QNetworkReply*> ReplyContinuatoin;

ReplyContinuatoin waitForReplyFinished(QNetworkReply *rep);
UrlContinuation grabLinks(const QUrl& url, int depth);
QSet<QUrl> scrapUrls(const QUrl& base, QIODevice* dev);

QNetworkAccessManager *nam = 0;

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	// Continuation monad
	nam = new QNetworkAccessManager();

	IntCont myCont = pure<Cont, int>(1) >>= std::function<IntCont(int)>([](int i) -> IntCont {
		return pure<Cont, int>(i + 1);
	});
	qDebug() << ">>=" << myCont.evalCont();

	qDebug() << "join" << join<Cont, int>(pure<Cont, Cont<int>>(pure<Cont, int>(1))).evalCont();

	IntCont myCont2 = ((IntCont)pure<Cont, int>(1)) >> std::function<IntCont()>([]() -> IntCont {
		return pure<Cont, int>(42);
	});
	qDebug() << ">>" << myCont2.evalCont();

	(
		grabLinks(QUrl("http://www.teamdrive.com/de/home.html"), 1) >>= std::function<VoidCont(QSet<QUrl>)>([&a](QSet<QUrl> urls) -> VoidCont {
		qDebug() << "urls.len():" << urls;
		a.exit();
		return pure<Cont, Unit>(Unit());
	})).evalCont();


	// Maybe Monad
	qDebug() << pure<boost::optional, int>(42);
	qDebug() << (pure<boost::optional, int>(42) >>= std::function<boost::optional<int>(int)>([](int i) -> boost::optional<int> {return boost::optional<int>(i);}));
	qDebug() << (pure<boost::optional, int>(42) >> std::function<boost::optional<int>()>([]() -> boost::optional<int> {return boost::optional<int>();}));
	qDebug() << join<boost::optional, int>(pure<boost::optional, boost::optional<int>>(pure<boost::optional, int>(42))).get();

	// sequence
	qDebug() << sequence<boost::optional, QList, int>(QList<boost::optional<int>>{ boost::optional<int>(3), boost::optional<int>(4) , boost::optional<int>(5) });
	qDebug() << sequence<boost::optional, QList, int>(QList<boost::optional<int>>{ boost::optional<int>(3), boost::optional<int>() , boost::optional<int>(5) });
	// qDebug() << sequence<boost::optional, std::vector, int>(std::vector<boost::optional<int>> { boost::optional<int>(3), boost::optional<int>(4) , boost::optional<int>(5) }); ?

	// mapM
	qDebug() << mapM<boost::optional, QList, int, QString>([](int i){
		return boost::optional<QString>(QString::number(i * 10, 16));
	},QList<int>{1,2,3,4,5,6,7,8,9});
	// return 0;
	return a.exec();
}


UrlContinuation grabLinks(const QUrl& url, int depth)
{
	if (depth <= 0)
		return pure<Cont, QSet<QUrl>>(QSet<QUrl>() << url);
	return waitForReplyFinished(nam->get(QNetworkRequest(url))) >>= std::function<UrlContinuation(QNetworkReply*)>([url, depth](QNetworkReply* rep) -> UrlContinuation {
		QSet<QUrl> urls = scrapUrls(url, rep);
	return mapM<Cont, QSet, QUrl, QSet<QUrl>>(std::function<UrlContinuation(QUrl)>([depth](QUrl u){
//				qDebug() << "grabLinks(u, depth-1)" << u;
				return grabLinks(u, depth-1);
			}), urls) >>= std::function<UrlContinuation(QSet<QSet<QUrl>>)>([](QSet<QSet<QUrl>> uurls){
	return pure<Cont, QSet<QUrl>>(join<QSet,QUrl>(uurls));
	});});
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
		[rep](ReplyContinuatoin::Inner inner) -> QVariant {
			QObject::connect(rep, &QNetworkReply::finished, [inner, rep]() -> void {
//				qDebug() << "waitForReplyFinished";
				inner(rep);
			});
			return QVariant();
		}
	);
}



