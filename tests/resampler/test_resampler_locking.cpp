// THREAD-01 and its sibling on the data path.
//
// _AbstractResampler::_resample() used to hold _plot_info_mutex across the
// dispatch to the derived resample(), and setData() still held
// _next_data_mutex across the same dispatch. In the real subclasses that
// dispatch is where _resample_sig() is emitted, so Qt's connection-list walk
// ran under a lock that other threads need.
//
// Both locks are recursive, so a same-thread probe cannot detect this: the
// check has to come from a second thread while the dispatch is in flight.
#include <QObject>
#include <QThread>
#include <QtTest/QtTest>

#include <atomic>
#include <functional>
#include <thread>

#include "SciQLopPlots/Plotables/Resamplers/AbstractResampler.hpp"

namespace
{
/// Minimal CRTP host: resample() stands in for the real subclasses' emit, and
/// runs `probe_action` on another thread to see which locks are still held.
class ProbeResampler : public _AbstractResampler<false, ProbeResampler>
{
public:
    std::function<void(ProbeResampler&)> probe_action;
    std::atomic_bool probe_started { false };
    std::atomic_bool probe_finished { false };
    bool observed_blocked = false;
    std::thread probe;

    ~ProbeResampler()
    {
        if (probe.joinable())
            probe.join();
    }

    void resample(const QCPRange)
    {
        // The probe re-enters here once unblocked; only the first call probes.
        if (probe_started.exchange(true))
            return;

        probe = std::thread(
            [this]
            {
                probe_action(*this);
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
    void resampleDispatchDoesNotHoldNextDataMutex();
};

void TestResamplerLocking::resampleDispatchDoesNotHoldPlotInfoMutex()
{
    ProbeResampler resampler;
    // Any public accessor that takes _plot_info_mutex will do.
    resampler.probe_action = [](ProbeResampler& r) { r.set_plot_size(QSize(11, 22)); };

    // Takes _plot_info_mutex, releases it, then calls _resample() -> resample().
    resampler.set_plot_size(QSize(800, 600));
    resampler.probe.join();

    QVERIFY2(resampler.probe_started.load(), "the resample dispatch never ran");
    QVERIFY2(!resampler.observed_blocked,
             "a second thread could not read plot info while the resample dispatch "
             "was in flight: _plot_info_mutex is still held across it");
}

void TestResamplerLocking::resampleDispatchDoesNotHoldNextDataMutex()
{
    ProbeResampler resampler;
    // setData() is the public entry point that takes _next_data_mutex. Empty
    // buffers keep this off the GIL: _bounds() short-circuits on flat_size() == 0.
    resampler.probe_action
        = [](ProbeResampler& r) { r.setData(SciQLopPyBuffer {}, SciQLopPyBuffer {}); };

    resampler.setData(SciQLopPyBuffer {}, SciQLopPyBuffer {});
    resampler.probe.join();

    QVERIFY2(resampler.probe_started.load(), "the resample dispatch never ran");
    QVERIFY2(!resampler.observed_blocked,
             "a second thread could not push new data while the resample dispatch "
             "was in flight: _next_data_mutex is still held across it");
}

QTEST_GUILESS_MAIN(TestResamplerLocking)
#include "test_resampler_locking.moc"
