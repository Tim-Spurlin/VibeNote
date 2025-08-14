#ifndef OCR_ENGINE_H
#define OCR_ENGINE_H

#include <QString>
#include <QImage>

class OcrEngine {
public:
    virtual ~OcrEngine() = default;
    virtual QString recognize(const QImage &image) = 0;
};

#endif
