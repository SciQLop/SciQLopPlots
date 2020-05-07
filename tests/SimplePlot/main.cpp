#include <QtTest>
#include <QObject>

#include <SciQLopPlot.hpp>

class SimpleTest : public QObject

{
    Q_OBJECT
public:
    explicit SimpleTest(QObject *parent = nullptr) : QObject(parent){}
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


QTEST_MAIN(SimpleTest)

#include "main.moc"

