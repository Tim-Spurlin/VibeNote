#pragma once

#include <QImage>
#include <QJsonObject>
#include <QRect>
#include <QString>

#include <cstdint>
#include <memory>
#include <vector>

// This interface is consumed by frame_diff.cpp to process detected regions.
// Backends must implement thread-safe processing.

struct OcrSpan {
    QString text;           ///< Recognised text segment
    QRect bounding_box;     ///< Pixel coordinates of the span within the frame
    float confidence;       ///< Confidence score [0,1]
    int64_t timestamp_ms;   ///< Timestamp of the frame in milliseconds
};

class OcrEngine {
public:
    virtual ~OcrEngine() = default;

    //! Initialise the engine with the provided configuration.
    virtual bool initialize(const QJsonObject &config) = 0;

    //! Perform OCR on the given frame and regions.
    //! This method must be thread-safe and callable from multiple threads.
    virtual std::vector<OcrSpan> process(
        const QImage &frame, const std::vector<QRect> &regions) = 0;

    //! Release any resources held by the backend.
    virtual void shutdown() = 0;

    //! Human-friendly backend identifier.
    virtual QString getBackendName() const = 0;

    //! Indicates whether the backend uses GPU acceleration.
    virtual bool isGpuAccelerated() const = 0;
};

// Forward declarations for backend-specific creators to avoid circular dependencies.
std::unique_ptr<OcrEngine> createOcrTesseract(const QJsonObject &config);
std::unique_ptr<OcrEngine> createOcrPaddle(const QJsonObject &config);

//! Factory that constructs an OCR engine for the requested backend.
inline std::unique_ptr<OcrEngine> createOcrEngine(
    const QString &backend, const QJsonObject &config) {
    if (backend.compare(QStringLiteral("tesseract"), Qt::CaseInsensitive) == 0) {
        return createOcrTesseract(config);
    }
    if (backend.compare(QStringLiteral("paddle"), Qt::CaseInsensitive) == 0) {
        return createOcrPaddle(config);
    }
    return nullptr;
}

