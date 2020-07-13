#include <QtTest>
#include <QObject>

#include <SciQLopPlot.hpp>
#include <QCustomPlotWrapper.hpp>

class ASimplePlot : public QObject
{
    Q_OBJECT
public:
    explicit ASimplePlot(QObject *parent = nullptr) : QObject(parent){}
signals:

private slots:
    void initTestCase(){}
    void cleanupTestCase(){}

    void test1()
    {
        QCOMPARE(1+1, 2);
    }

private:

};


QTEST_MAIN(ASimplePlot)

#include "main.moc"

