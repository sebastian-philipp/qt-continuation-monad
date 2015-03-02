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

template<class T> using IntCont = Cont<int, T>;
template<class T> using VoidCont = Cont<void, T>;

typedef IntCont<int> IntIntCont;

typedef VoidCont<bool> VoidVoidCont;
typedef VoidCont<QList<QUrl>> UrlContinuatoin;
typedef VoidCont<QNetworkReply*> ReplyContinuatoin;

ReplyContinuatoin waitForReplyFinished(QNetworkReply *rep);
UrlContinuatoin grabLinks(const QUrl& url);
QList<QUrl> scrapUrls(const QUrl& base, QIODevice* dev);

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	IntIntCont myCont = unit<int, int>(1) >>= std::function<IntIntCont(int)>([](int i) -> IntIntCont {
		return unit<int, int>(i + 1);
	});
	qDebug() << ">>=" << myCont.evalCont();

	qDebug() << "join" << join<IntCont, int>(unit<int, IntCont<int>>(unit<int, int>(1))).evalCont();

	IntIntCont myCont2 = ((IntIntCont)unit<int, int>(1)) >> std::function<IntIntCont()>([]() -> IntIntCont {
		return unit<int, int>(42);
	});
	qDebug() << ">>" << myCont2.evalCont();

	(
		grabLinks(QUrl("http://www.heise.de")) >>= std::function<VoidVoidCont(QList<QUrl>)>([&a](QList<QUrl> urls){
		qDebug() << "urls:" << urls;
		a.exit();
		return unit(true);
	})).evalCont();


	// return 0;
	return a.exec();
}


UrlContinuatoin grabLinks(const QUrl& url)
{
	QSharedPointer<QNetworkAccessManager> nam(new QNetworkAccessManager());
	return waitForReplyFinished(nam->get(QNetworkRequest(url))) >>= std::function<UrlContinuatoin(QNetworkReply*)>([url, nam](QNetworkReply* rep) -> UrlContinuatoin {
//		qDebug() << "grabLinks";
		return unit(scrapUrls(url, rep));
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
		[rep](ReplyContinuatoin::Inner inner) -> void {
			QObject::connect(rep, &QNetworkReply::finished, [inner, rep](){
//				qDebug() << "waitForReplyFinished";
				inner(rep);
			});
		}
	);
}



