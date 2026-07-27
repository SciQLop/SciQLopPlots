// THREAD-01: _AbstractResampler::_resample() used to hold _plot_info_mutex
// across the dispatch to the derived resample(), which in the real subclasses
// is where _resample_sig() is emitted. Emitting under the lock makes every
// other thread touching plot info (set_plot_size, set_x_scale_log, ...) wait on
// Qt's connection-list walk for no reason.
//
// The lock is recursive, so a same-thread probe cannot detect this: the check
// has to come from a second thread while the dispatch is in flight.
#include <QObject>
#include <QThread>
#include <QtTest/QtTest>

#include <atomic>
#include <thread>

#include "SciQLopPlots/Plotables/Resamplers/AbstractResampler.hpp"

namespace
{
/// Minimal CRTP host: resample() stands in for the real subclasses' emit.
class ProbeResampler : public _AbstractResampler<false, ProbeResampler>
{
public:
    std::atomic_bool probe_started { false };
    std::atomic_bool probe_finished { false };
    bool observed_blocked = false;
    std::thread probe;

    void resample(const QCPRange)
    {
        // set_plot_size() re-enters here from the probe thread once unblocked.
        if (probe_started.exchange(true))
            return;

        probe = std::thread(
            [this]
            {
                // Any public accessor that takes _plot_info_mutex will do.
                this->set_plot_size(QSize(11, 22));
                probe_finished.store(true);
            });

        QDeadlineTimer deadline(2000);
        while (!probe_finished.load() && !deadline.hasExpired())
            QThread::msleep(1);
        observed_blocked = !probe_finished.load();
    }

    void _resample_impl(const ResamplerData1d&, const ResamplerPlotInfo&) { }
};
}

class TestResamplerLocking : public QObject
{
    Q_OBJECT

private slots:
    void resampleDispatchDoesNotHoldPlotInfoMutex();
};

void TestResamplerLocking::resampleDispatchDoesNotHoldPlotInfoMutex()
{
    ProbeResampler resampler;
    // Takes _plot_info_mutex, releases it, then calls _resample() -> resample().
    resampler.set_plot_size(QSize(800, 600));

    if (resampler.probe.joinable())
        resampler.probe.join();

    QVERIFY2(resampler.probe_started.load(), "the resample dispatch never ran");
    QVERIFY2(!resampler.observed_blocked,
             "a second thread could not read plot info while the resample dispatch "
             "was in flight: _plot_info_mutex is still held across it");
}

QTEST_GUILESS_MAIN(TestResamplerLocking)
#include "test_resampler_locking.moc"
