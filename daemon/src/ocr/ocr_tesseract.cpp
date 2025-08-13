#include "ocr_engine.h"

#include <QBuffer>
#include <QImage>
#include <QRect>
#include <mutex>
#include <vector>
#include <string>
#include <iostream>

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

// CPU-based OCR engine using Tesseract.
class OcrTesseract : public OcrEngine {
 public:
  OcrTesseract() : api_(nullptr) {}
  ~OcrTesseract() override { shutdown(); }

  bool initialize() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (api_) {
      return true;
    }

    api_ = new (std::nothrow) tesseract::TessBaseAPI();
    if (!api_) {
      std::cerr << "OcrTesseract: failed to allocate TessBaseAPI" << std::endl;
      return false;
    }

    if (api_->Init(nullptr, "eng", tesseract::OEM_DEFAULT) != 0) {
      std::cerr << "OcrTesseract: failed to initialise Tesseract" << std::endl;
      delete api_;
      api_ = nullptr;
      return false;
    }
    api_->SetPageSegMode(tesseract::PSM_AUTO);
    if (!api_->SetVariable("tessedit_do_invert", "0")) {
      std::cerr << "OcrTesseract: unable to set tessedit_do_invert" << std::endl;
    }
    return true;
  }

  std::vector<OcrSpan> process(const QImage &frame,
                               const std::vector<QRect> &regions) override {
    std::vector<OcrSpan> spans;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!api_) {
      std::cerr << "OcrTesseract: engine not initialised" << std::endl;
      return spans;
    }

    for (const QRect &region : regions) {
      QImage sub = frame.copy(region).convertToFormat(QImage::Format_Grayscale8);
      QByteArray buf;
      QBuffer buffer(&buf);
      buffer.open(QIODevice::WriteOnly);
      if (!sub.save(&buffer, "PNG")) {
        std::cerr << "OcrTesseract: failed to serialise image" << std::endl;
        continue;
      }
      PIX *pix = pixReadMemPng(reinterpret_cast<const l_uint8 *>(buf.constData()),
                                buf.size());
      if (!pix) {
        std::cerr << "OcrTesseract: failed to create PIX" << std::endl;
        continue;
      }
      api_->SetImage(pix);
      char *text = api_->GetUTF8Text();  // Ensure recognition
      if (text) {
        delete[] text;
      }
      if (api_->Recognize(nullptr) != 0) {
        std::cerr << "OcrTesseract: recognition failed" << std::endl;
        pixDestroy(&pix);
        continue;
      }
      tesseract::ResultIterator *it = api_->GetIterator();
      if (!it) {
        std::cerr << "OcrTesseract: no result iterator" << std::endl;
        pixDestroy(&pix);
        continue;
      }
      do {
        int x1, y1, x2, y2;
        const char *word = it->GetUTF8Text(tesseract::RIL_WORD);
        if (!word) {
          continue;
        }
        float conf = it->Confidence(tesseract::RIL_WORD);
        if (it->BoundingBox(tesseract::RIL_WORD, &x1, &y1, &x2, &y2)) {
          OcrSpan span;
          span.rect = QRect(region.left() + x1, region.top() + y1,
                            x2 - x1, y2 - y1);
          span.text = std::string(word);
          span.confidence = conf;
          spans.push_back(span);
        }
        delete[] word;
      } while (it->Next(tesseract::RIL_WORD));
      pixDestroy(&pix);
    }
    return spans;
  }

  void shutdown() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (api_) {
      api_->End();
      delete api_;
      api_ = nullptr;
    }
  }

  const char *getBackendName() const override { return "tesseract"; }

  bool isGpuAccelerated() const override { return false; }

 private:
  tesseract::TessBaseAPI *api_;
  mutable std::mutex mutex_;
};

// Factory registration would occur elsewhere in ocr_engine.h implementation.
