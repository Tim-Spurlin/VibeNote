#include "frame_diff.h"
#include "../logging.h"

namespace vibenote {

double FrameDiff::calculateDifference(const QImage &frame1, const QImage &frame2) {
    if (frame1.size() != frame2.size()) {
        return 1.0;
    }
    
    // Simple pixel difference calculation
    long long totalDiff = 0;
    int pixelCount = frame1.width() * frame1.height();
    
    for (int y = 0; y < frame1.height(); ++y) {
        for (int x = 0; x < frame1.width(); ++x) {
            QRgb p1 = frame1.pixel(x, y);
            QRgb p2 = frame2.pixel(x, y);
            
            int dr = qRed(p1) - qRed(p2);
            int dg = qGreen(p1) - qGreen(p2);
            int db = qBlue(p1) - qBlue(p2);
            
            totalDiff += qAbs(dr) + qAbs(dg) + qAbs(db);
        }
    }
    
    return static_cast<double>(totalDiff) / (pixelCount * 765.0); // 765 = 255*3
}

bool FrameDiff::isSignificantChange(const QImage &frame1, const QImage &frame2, double threshold) {
    double diff = calculateDifference(frame1, frame2);
    return diff > threshold;
}

} // namespace vibenote
