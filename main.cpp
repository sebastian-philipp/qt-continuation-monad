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

typedef Cont<QList<QUrl>> UrlContinuatoin;
typedef Cont<QNetworkReply*> ReplyContinuatoin;

ReplyContinuatoin waitForReplyFinished(QNetworkReply *rep);
UrlContinuatoin grabLinks(const QUrl& url);
QList<QUrl> scrapUrls(const QUrl& base, QIODevice* dev);

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	// Continuation monad

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
		grabLinks(QUrl("http://www.heise.de")) >>= std::function<VoidCont(QList<QUrl>)>([&a](QList<QUrl> urls) -> VoidCont {
		qDebug() << "urls.len():" << urls.length();
		a.exit();
		return pure<Cont, Unit>(Unit());
	})).evalCont();


	// Maybe Monad
	qDebug() << join<Maybe, int>(pure<Maybe, Maybe<int>>(pure<Maybe, int>(42))).fromJust();
	qDebug() << (pure<Maybe, int>(42) >> std::function<Maybe<int>()>([](){return Maybe<int>();})).isJust();

	// return 0;
	return a.exec();
}


UrlContinuatoin grabLinks(const QUrl& url)
{
	QSharedPointer<QNetworkAccessManager> nam(new QNetworkAccessManager());
	return waitForReplyFinished(nam->get(QNetworkRequest(url))) >>= std::function<UrlContinuatoin(QNetworkReply*)>([url, nam](QNetworkReply* rep) -> UrlContinuatoin {
//		qDebug() << "grabLinks";
		return pure<Cont, QList<QUrl>>(scrapUrls(url, rep));
	});
}


QList<QUrl> scrapUrls(const QUrl& base, QIODevice* dev)
{
	using namespace htmlcxx::HTML;
	struct Handler: ParserSax {
		QList<QUrl>& m_Urls;
		const QUrl& m_Base;
		Handler(const QUrl& b, QList<QUrl>& u): m_Urls(u), m_Base(b){}

		void foundTag(Node node, bool isEnd) {
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
	QList<QUrl> urls;
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



