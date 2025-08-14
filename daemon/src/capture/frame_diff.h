#pragma once

#include <QImage>

namespace vibenote {

class FrameDiff {
public:
    static double calculateDifference(const QImage &frame1, const QImage &frame2);
    static bool isSignificantChange(const QImage &frame1, const QImage &frame2, double threshold = 0.05);
};

} // namespace vibenote
