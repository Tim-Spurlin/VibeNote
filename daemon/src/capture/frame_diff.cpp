#include <QImage>
#include <QObject>
#include <QRect>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "ocr_engine.h"

class FrameDiff : public QObject {
    Q_OBJECT

public:
    explicit FrameDiff(OcrEngine *engine = nullptr, int threshold = 30, QObject *parent = nullptr)
        : QObject(parent), ocr_engine_(engine), threshold_(threshold) {}

    void setReferenceFrame(const QImage &frame) { reference_frame_ = frame.convertToFormat(QImage::Format_Grayscale8); }

    std::vector<QRect> processFrame(const QImage &new_frame) {
        std::vector<QRect> regions;
        if (new_frame.isNull()) {
            return regions;
        }
        if (reference_frame_.isNull()) {
            setReferenceFrame(new_frame);
            return regions;
        }

        cv::Mat ref = toGray(reference_frame_);
        cv::Mat curr = toGray(new_frame);

        cv::Mat diff;
        cv::absdiff(curr, ref, diff);

        cv::Mat binary;
        cv::threshold(diff, binary, threshold_, 255, cv::THRESH_BINARY);

        cv::Mat morph;
        cv::dilate(binary, morph, cv::Mat(), cv::Point(-1, -1), 1);
        cv::erode(morph, morph, cv::Mat(), cv::Point(-1, -1), 1);

        cv::Mat labels, stats, centroids;
        int count = cv::connectedComponentsWithStats(morph, labels, stats, centroids);
        for (int i = 1; i < count; ++i) {
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            if (area < 100) {
                continue;
            }
            int x = stats.at<int>(i, cv::CC_STAT_LEFT);
            int y = stats.at<int>(i, cv::CC_STAT_TOP);
            int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
            int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);
            regions.emplace_back(x, y, w, h);
        }

        if (ocr_engine_) {
            auto spans = ocr_engine_->process(new_frame, regions);
            emit ocrResults(spans);
        }

        const double alpha = 0.1;
        cv::addWeighted(curr, alpha, ref, 1.0 - alpha, 0.0, ref);
        reference_frame_ = fromGray(ref);

        return regions;
    }

    void reset() { reference_frame_ = QImage(); }

public slots:
    void onWindowChanged() { reset(); }

signals:
    void ocrResults(const std::vector<OcrSpan> &results);

private:
    static cv::Mat toGray(const QImage &img) {
        QImage gray = img.format() == QImage::Format_Grayscale8
                           ? img
                           : img.convertToFormat(QImage::Format_Grayscale8);
        return cv::Mat(gray.height(), gray.width(), CV_8UC1, const_cast<uchar *>(gray.constBits()),
                       gray.bytesPerLine())
            .clone();
    }

    static QImage fromGray(const cv::Mat &mat) {
        QImage img(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        return img.copy();
    }

    QImage reference_frame_;
    OcrEngine *ocr_engine_;
    int threshold_;
};

#include "frame_diff.moc"
