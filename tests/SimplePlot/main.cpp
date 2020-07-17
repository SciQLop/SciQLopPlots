#include <QObject>
#include <QtTest>

#include <DataGenerator.hpp>
#include <QCustomPlotWrapper.hpp>
#include <SciQLopPlot.hpp>

#include <QtGuiTestUtils/GUITestUtils.h>

class ASimplePlot : public QObject
{
    Q_OBJECT

    SciQLopPlots::SciQLopPlot plot;
    DataProducer<SciQLopPlots::SciQLopPlot> gen;

public:
    explicit ASimplePlot(QObject* parent = nullptr) : QObject(parent), gen(&plot, 0) { }
signals:

private slots:
    void initTestCase() { }
    void cleanupTestCase() { }

    void scrolls_left_with_mouse() { mouse_scroll(200); }

    void scrolls_right_with_mouse() { mouse_scroll(-200); }

private:
    void mouse_scroll(int value)
    {
        QVERIFY(prepare_gui_test(&plot));
        auto xRange = plot.xRange();
        for (auto i = 0; i < 100; i++)
        {
            scroll_graph(&plot, value);
        }
        if (value >= 0)
            QVERIFY(xRange.second > plot.xRange().second);
        else
            QVERIFY(xRange.second < plot.xRange().second);
    }
};

QT_BEGIN_NAMESPACE
QTEST_ADD_GPU_BLACKLIST_SUPPORT_DEFS
QT_END_NAMESPACE
int main(int argc, char* argv[])
{
    QApplication app { argc, argv };
    app.setAttribute(Qt::AA_Use96Dpi, true);
    QTEST_DISABLE_KEYPAD_NAVIGATION;
    QTEST_ADD_GPU_BLACKLIST_SUPPORT;
    ASimplePlot tc;
    QTEST_SET_MAIN_SOURCE_PATH;
    return QTest::qExec(&tc, argc, argv);
}

#include "main.moc"
