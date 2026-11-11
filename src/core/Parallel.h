#pragma once

#include <QFuture>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <vector>

namespace pdn {

// Splits [begin, end) into contiguous chunks and runs fn(chunkBegin, chunkEnd) on the global
// thread pool, using the calling thread for the last chunk. Blocks until every chunk finished.
//
// Safe to call from a pool thread: QFuture::waitForFinished() runs not-yet-started jobs inline,
// so nested calls cannot deadlock on an exhausted pool.
template <typename Fn>
void parallelFor(int begin, int end, int minChunk, Fn&& fn) {
    const int total = end - begin;
    if (total <= 0) return;

    const int maxThreads = std::max(1, QThread::idealThreadCount());
    const int chunks = std::clamp(total / std::max(1, minChunk), 1, maxThreads);
    if (chunks == 1) {
        fn(begin, end);
        return;
    }

    std::vector<QFuture<void>> futures;
    futures.reserve(chunks - 1);
    const int step = total / chunks;
    int chunkBegin = begin;
    for (int i = 0; i < chunks - 1; ++i) {
        const int chunkEnd = chunkBegin + step;
        futures.push_back(QtConcurrent::run([&fn, chunkBegin, chunkEnd]() { fn(chunkBegin, chunkEnd); }));
        chunkBegin = chunkEnd;
    }
    fn(chunkBegin, end);
    for (auto& f : futures) f.waitForFinished();
}

// Convenience wrapper for per-row image work: fn(y) is called once for every row in [0, height).
template <typename Fn>
void parallelRows(int height, Fn&& fn, int minRowsPerChunk = 32) {
    parallelFor(0, height, minRowsPerChunk, [&fn](int y0, int y1) {
        for (int y = y0; y < y1; ++y) fn(y);
    });
}

} // namespace pdn
